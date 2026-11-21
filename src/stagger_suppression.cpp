#include "stagger_suppression.h"

#include "builtin_poise.h"
#include "stagger_gate.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <intrin.h>

namespace
{
	constexpr unsigned kLoggedSuppressions = 25;
	std::atomic<unsigned> suppressedCount{ 0 };
	StaggerSuppression::RoutineObserver routineObserver = nullptr;

	bool IsExecutable(std::uintptr_t address)
	{
		MEMORY_BASIC_INFORMATION memory{};
		if (!address || VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory) ||
			memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
			return false;
		}
		switch (memory.Protect & 0xFF) {
		case PAGE_EXECUTE:
		case PAGE_EXECUTE_READ:
		case PAGE_EXECUTE_READWRITE:
		case PAGE_EXECUTE_WRITECOPY:
			return true;
		default:
			return false;
		}
	}

	void LogSuppression(const char* what, RE::Actor* actor, const void* caller)
	{
		const auto count = suppressedCount.fetch_add(1, std::memory_order_relaxed);
		if (count < kLoggedSuppressions) {
			const auto base = REL::Module::get().base();
			const auto address = reinterpret_cast<std::uintptr_t>(caller);
			logger::info("Suppressed vanilla stagger ({}): target={:08X}, caller offset=0x{:X}.",
				what, actor ? actor->GetFormID() : 0, address >= base ? address - base : 0);
		} else if (count == kLoggedSuppressions) {
			logger::info("Further suppressed vanilla staggers will not be logged individually.");
		}
	}

	using StaggerFunction = void (*)(RE::Actor*, float, RE::Actor*);
	StaggerFunction originalStagger = nullptr;

	// Reached by a jmp from the routine's entry, so the return address is the original caller.
	void StaggerGateRoutine(RE::Actor* target, float magnitude, RE::Actor* aggressor)
	{
		const void* caller = _ReturnAddress();
		const bool suppressed = BuiltinPoise::ShouldSuppressStagger(target);
		if (routineObserver) {
			routineObserver(target, magnitude, aggressor, caller, suppressed);
		}
		if (suppressed) {
			LogSuppression("routine", target, caller);
			return;
		}
		originalStagger(target, magnitude, aggressor);
	}

	bool InstallRoutineGate(std::uintptr_t site)
	{
		constexpr std::array<std::uint8_t, 11> prologue{
			0x48, 0x8B, 0xC4, 0x57, 0x48, 0x81, 0xEC, 0xC0, 0x00, 0x00, 0x00
		};
		constexpr std::array<std::uint8_t, 6> tail{ 0x81, 0xEC, 0xC0, 0x00, 0x00, 0x00 };
		if (std::memcmp(reinterpret_cast<const void*>(site), prologue.data(), prologue.size()) != 0) {
			logger::warn("Stagger suppression routine prologue validation FAILED (already patched or different build); vanilla staggers are unchanged.");
			return false;
		}
		// The prologue is position-independent, so it is copied into a stub that resumes after the 11 bytes.
		auto* stub = static_cast<std::uint8_t*>(SKSE::GetTrampoline().allocate(32));
		std::memcpy(stub, prologue.data(), prologue.size());
		stub[11] = 0xFF;
		stub[12] = 0x25;
		std::memset(stub + 13, 0, 4);
		const std::uint64_t resume = site + prologue.size();
		std::memcpy(stub + 17, &resume, sizeof(resume));
		originalStagger = reinterpret_cast<StaggerFunction>(stub);
		// The 5-byte jump ends inside the third instruction; the remaining tail is nop-filled below.
		SKSE::GetTrampoline().write_branch<5>(site, StaggerGateRoutine, true);
		if (!REL::safe_fill(site + 5, 0x90, tail.size(), tail)) {
			logger::error("Stagger suppression could not clear the prologue tail after patching the routine.");
		}
		return *reinterpret_cast<const std::uint8_t*>(site) == 0xE9;
	}

	template <class T>
	struct WhitelistedEffect
	{
		static inline void (*original)(RE::ActiveEffect*) = nullptr;

		static void Thunk(RE::ActiveEffect* effect)
		{
			StaggerGate::ScopedAllowStagger allowStagger;
			original(effect);
		}
	};

		struct DisarmEffectTag { static constexpr auto name = "DisarmEffect"; };

		template <class T>
		bool InstallEffectWhitelist(REL::Relocation<std::uintptr_t>& vtable)
		{
			const auto slot = vtable.address() + 0x14 * sizeof(std::uintptr_t);
			const auto current = *reinterpret_cast<const std::uintptr_t*>(slot);
			if (!IsExecutable(current)) {
				logger::warn("Stagger suppression {}::Start slot is not executable; its staggers will be suppressed like other vanilla staggers.", T::name);
				return false;
			}
			WhitelistedEffect<T>::original = reinterpret_cast<void (*)(RE::ActiveEffect*)>(current);
			vtable.write_vfunc(0x14, WhitelistedEffect<T>::Thunk);
			return *reinterpret_cast<const std::uintptr_t*>(slot) == reinterpret_cast<std::uintptr_t>(&WhitelistedEffect<T>::Thunk);
		}

		// StaggerEffect::Start is shared by shouts (which must stay whitelisted) and melee-triggered stagger spells
		// (Valhalla's "Small Impact: stagger", etc.). Shouts are SpellType::kVoicePower and keep the allow guard so the
		// routine gate lets them through; every other stagger spell is suppressed for poise-managed actors.
		struct StaggerEffectGate
		{
			static inline void (*original)(RE::ActiveEffect*) = nullptr;

			static void Thunk(RE::ActiveEffect* effect)
			{
				auto* stagger = static_cast<RE::StaggerEffect*>(effect);
				const bool isShout = stagger->spell &&
					stagger->spell->GetSpellType() == RE::MagicSystem::SpellType::kVoicePower;
				if (isShout) {
					StaggerGate::ScopedAllowStagger allowStagger;
					original(effect);
					return;
				}
				// `target` is a MagicTarget* secondary base of Actor (offset 0x98), so the ActiveEffect
				// target-actor accessor's reinterpret_cast<Actor*>(target) yields a garbage pointer. Resolve
				// through the virtual GetTargetStatsObject() (returns the TESObjectREFR* base) and As<Actor>().
				auto* refr = stagger->target ? stagger->target->GetTargetStatsObject() : nullptr;
				auto* actor = refr ? refr->As<RE::Actor>() : nullptr;
				if (actor && BuiltinPoise::ShouldSuppressStagger(actor)) {
					LogSuppression("magic stagger", actor, _ReturnAddress());
					// Skip the stagger and cleanly remove the effect so it does not linger on the MagicTarget.
					stagger->Dispel(false);
					return;
				}
				original(effect);
			}
		};

		bool InstallStaggerEffectGate(REL::Relocation<std::uintptr_t>& vtable)
		{
			const auto slot = vtable.address() + 0x14 * sizeof(std::uintptr_t);
			const auto current = *reinterpret_cast<const std::uintptr_t*>(slot);
			if (!IsExecutable(current)) {
				logger::warn("Stagger suppression StaggerEffect::Start slot is not executable; magic staggers are unchanged.");
				return false;
			}
			StaggerEffectGate::original = reinterpret_cast<void (*)(RE::ActiveEffect*)>(current);
			vtable.write_vfunc(0x14, StaggerEffectGate::Thunk);
			return *reinterpret_cast<const std::uintptr_t*>(slot) == reinterpret_cast<std::uintptr_t>(&StaggerEffectGate::Thunk);
		}

	using NotifyFunction = bool (*)(RE::IAnimationGraphManagerHolder*, const RE::BSFixedString&);

	template <class T>
	struct WardBreakGate
	{
		static inline NotifyFunction original = nullptr;

		static bool Thunk(RE::IAnimationGraphManagerHolder* holder, const RE::BSFixedString& event)
				{
					if ((StaggerGate::IsWardBreakEvent(event.c_str()) || StaggerGate::IsStaggerStartEvent(event.c_str())) && holder) {
						auto* actor = static_cast<RE::Actor*>(holder);
						if (BuiltinPoise::ShouldSuppressStagger(actor)) {
							LogSuppression(StaggerGate::IsWardBreakEvent(event.c_str()) ? "ward break" : "stagger event",
								actor, _ReturnAddress());
							return false;
						}
					}
					return original(holder, event);
				}
	};

	struct PlayerTag { static constexpr auto name = "player"; };
	struct CharacterTag { static constexpr auto name = "character"; };

	template <class T>
	bool InstallWardBreakGate(REL::Relocation<std::uintptr_t>& vtable)
	{
		const auto slot = vtable.address() + 1 * sizeof(std::uintptr_t);
		const auto current = *reinterpret_cast<const std::uintptr_t*>(slot);
		if (!IsExecutable(current)) {
			logger::warn("Stagger suppression {} NotifyAnimationGraph slot validation FAILED; ward breaks are unchanged.", T::name);
			return false;
		}
		WardBreakGate<T>::original = reinterpret_cast<NotifyFunction>(current);
		vtable.write_vfunc(1, WardBreakGate<T>::Thunk);
		return *reinterpret_cast<const std::uintptr_t*>(slot) == reinterpret_cast<std::uintptr_t>(&WardBreakGate<T>::Thunk);
	}
}

void StaggerSuppression::SetRoutineObserver(RoutineObserver observer)
{
	routineObserver = observer;
}

std::optional<std::size_t> StaggerSuppression::HolderVTableIndex()
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player) {
		return std::nullopt;
	}
	const auto* holder = static_cast<RE::IAnimationGraphManagerHolder*>(player);
	const auto actual = *reinterpret_cast<const std::uintptr_t*>(holder);
	// Later PlayerCharacter vtable entries are VR-only placeholders without SE/AE IDs.
	const std::size_t count = (std::min)(RE::VTABLE_PlayerCharacter.size(), std::size_t{ 8 });
	for (std::size_t i = 0; i < count; ++i) {
		REL::Relocation<std::uintptr_t> table{ RE::VTABLE_PlayerCharacter[i] };
		if (table.address() == actual) {
			return i;
		}
	}
	return std::nullopt;
}

void StaggerSuppression::Install()
{
	const auto& module = REL::Module::get();
	if (module.version() != SKSE::RUNTIME_SSE_1_7_104) {
		logger::warn("Vanilla stagger suppression not installed: only runtime 1.7.104.0 is validated.");
		return;
	}
	if (!BuiltinPoise::IsInstalled()) {
		logger::warn("Vanilla stagger suppression not installed: built-in poise hooks are not active.");
		return;
	}
	REL::Relocation<std::uintptr_t> staggerEffect{ RE::VTABLE_StaggerEffect[0] };
	REL::Relocation<std::uintptr_t> disarmEffect{ RE::VTABLE_DisarmEffect[0] };
	// The whitelists go first: the routine gate must never be live without them, or shouts would be suppressed.
		const bool staggerGate = InstallStaggerEffectGate(staggerEffect);
	const bool disarmWhitelist = InstallEffectWhitelist<DisarmEffectTag>(disarmEffect);
		const bool routine = staggerGate && disarmWhitelist &&
		InstallRoutineGate(REL::RelocationID(36700, 37710).address());
	if (!routine) {
			logger::error("Vanilla stagger suppression routine gate not installed (StaggerEffect gate={}, DisarmEffect whitelist={}); hit, bash and shout staggers are unchanged.",
				staggerGate, disarmWhitelist);
	}
	bool wardGate = false;
	if (const auto index = HolderVTableIndex(); index && *index < RE::VTABLE_Character.size()) {
		REL::Relocation<std::uintptr_t> player{ RE::VTABLE_PlayerCharacter[*index] };
		REL::Relocation<std::uintptr_t> character{ RE::VTABLE_Character[*index] };
		const bool playerGate = InstallWardBreakGate<PlayerTag>(player);
		const bool characterGate = InstallWardBreakGate<CharacterTag>(character);
		wardGate = playerGate && characterGate;
	} else {
		logger::warn("Stagger suppression could not locate the animation-graph holder vtable; ward breaks are unchanged.");
	}
	logger::info("Vanilla stagger suppression: routine gate={}, StaggerEffect gate={}, DisarmEffect whitelist={}, ward-break gate={}.",
			routine ? "passed" : "not installed", staggerGate ? "passed" : "FAILED",
		disarmWhitelist ? "passed" : "FAILED", wardGate ? "passed" : "FAILED");
}
