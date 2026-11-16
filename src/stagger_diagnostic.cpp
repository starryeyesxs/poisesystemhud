#include "stagger_diagnostic.h"

#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
#include "builtin_poise.h"
#include "stagger_gate.h"
#include "stagger_suppression.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstring>
#include <cwchar>
#include <intrin.h>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <tlhelp32.h>
#include <type_traits>
#include <vector>

namespace
{
	constexpr unsigned kLimit = 1500;
	std::atomic<unsigned> routineCount{ 0 };
	std::atomic<unsigned> effectCount{ 0 };
	std::atomic<unsigned> eventCount{ 0 };
	std::atomic<unsigned> dumpCount{ 0 };
	std::atomic<unsigned> stackCount{ 0 };
	std::atomic<unsigned> hitCount{ 0 };
	std::atomic<unsigned> damageCount{ 0 };
	std::atomic<ULONGLONG> lastRoutineTick{ 0 };
	std::atomic<bool> playerWasStaggering{ false };
	thread_local int effectDepth = 0;

	struct RecentEvent
	{
		ULONGLONG tick = 0;
		std::string name;
		const void* caller = nullptr;
	};
	std::mutex recentMutex;
	std::array<RecentEvent, 48> recentEvents{};
	std::size_t recentNext = 0;

	bool TakeSample(std::atomic<unsigned>& count, const char* category)
	{
		const auto sample = count.fetch_add(1, std::memory_order_relaxed);
		if (sample == kLimit) {
			logger::info("Stagger diagnostic sample limit reached for {}; restart the game for more samples.", category);
		}
		return sample < kLimit;
	}

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

	std::string NarrowFileName(const wchar_t* path, std::size_t length)
	{
		std::size_t begin = 0;
		for (std::size_t i = 0; i < length; ++i) {
			if (path[i] == L'\\' || path[i] == L'/') {
				begin = i + 1;
			}
		}
		std::string name;
		for (std::size_t i = begin; i < length; ++i) {
			name.push_back(path[i] < 128 ? static_cast<char>(path[i]) : '?');
		}
		return name;
	}

	// Names the module that owns an address so callers in other mods are identifiable.
	std::string Origin(const void* address)
	{
		HMODULE owner = nullptr;
		if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(address), &owner) || !owner) {
			return "unknown";
		}
		wchar_t path[MAX_PATH]{};
		const auto length = GetModuleFileNameW(owner, path, static_cast<DWORD>(std::size(path)));
		return std::format("{}+0x{:X}", NarrowFileName(path, length),
			reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(owner));
	}

	std::string Describe(RE::Actor* actor)
	{
		if (!actor) {
			return "null";
		}
		const char* name = actor->GetName();
		return std::format("{:08X} '{}'{}{} {}", actor->GetFormID(), name ? name : "",
			actor->IsPlayerRef() ? " player" : "", actor->HasKeywordString("ActorTypeNPC") ? " npc" : " non-npc",
			BuiltinPoise::DescribeActorClass(actor));
	}

	bool StartsWithI(std::string_view text, std::string_view prefix)
	{
		return text.size() >= prefix.size() &&
			std::equal(prefix.begin(), prefix.end(), text.begin(), [](char a, char b) {
				return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
			});
	}

	bool ContainsI(std::string_view text, std::string_view needle)
	{
		return std::search(text.begin(), text.end(), needle.begin(), needle.end(), [](char a, char b) {
			return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		}) != text.end();
	}

	void RecordPlayerEvent(const char* name, const void* caller)
	{
		// Turn events fire every frame while idle and would push the useful events out of the buffer.
		if (!name || StartsWithI(name, "turn")) {
			return;
		}
		std::lock_guard lock(recentMutex);
		if (recentNext > 0) {
			const auto& last = recentEvents[(recentNext - 1) % recentEvents.size()];
			if (last.name == name && GetTickCount64() - last.tick < 30) {
				return;
			}
		}
		recentEvents[recentNext % recentEvents.size()] = { GetTickCount64(), name, caller };
		++recentNext;
	}

	// Lists the player animation events that preceded a stagger, with the state a block or stamina break would depend on.
	void DumpRecentEvents(RE::Actor* player)
	{
		if (dumpCount.fetch_add(1, std::memory_order_relaxed) >= 80) {
			return;
		}
		const auto now = GetTickCount64();
		std::vector<RecentEvent> snapshot;
		{
			std::lock_guard lock(recentMutex);
			const auto count = (std::min)(recentNext, recentEvents.size());
			for (std::size_t i = 0; i < count; ++i) {
				snapshot.push_back(recentEvents[(recentNext - count + i) % recentEvents.size()]);
			}
		}
		const auto routineTick = lastRoutineTick.load(std::memory_order_relaxed);
		PoiseBridge::Values poise{};
		const bool havePoise = BuiltinPoise::Read(poise);
		auto* values = player->AsActorValueOwner();
		logger::info("Stagger diagnostic player became staggered: poise={}/{} stamina={} health={} blocking={} wardPower={} msSinceRoutineCall={} recent player events (turn events omitted), oldest first:",
			havePoise ? poise.current : -1.0F, havePoise ? poise.maximum : -1.0F,
			values->GetActorValue(RE::ActorValue::kStamina), values->GetActorValue(RE::ActorValue::kHealth),
			player->IsBlocking(), values->GetActorValue(RE::ActorValue::kWardPower),
			routineTick ? static_cast<long long>(now - routineTick) : -1LL);
		for (const auto& entry : snapshot) {
			logger::info("  -{}ms event='{}' caller={}", now - entry.tick, entry.name, Origin(entry.caller));
		}
	}

	std::string DescribeHitFlags(const RE::HitData& data)
	{
		using Flag = RE::HitData::Flag;
		constexpr std::array<std::pair<Flag, const char*>, 15> names{ {
			{ Flag::kBlocked, "blocked" }, { Flag::kBlockWithWeapon, "blockWithWeapon" }, { Flag::kBlockCandidate, "blockCandidate" },
			{ Flag::kCritical, "critical" }, { Flag::kSneakAttack, "sneak" }, { Flag::kBash, "bash" },
			{ Flag::kTimedBash, "timedBash" }, { Flag::kPowerAttack, "power" }, { Flag::kLeftHand, "leftHand" },
			{ Flag::kMeleeAttack, "melee" }, { Flag::kRicochet, "ricochet" }, { Flag::kExplosion, "explosion" },
			{ Flag::kPredictDamage, "predict" }, { Flag::kDisarm, "disarm" }, { Flag::kDisableWeapon, "disableWeapon" }
		} };
		std::string text;
		for (const auto& [flag, name] : names) {
			if (data.flags.any(flag)) {
				text += (text.empty() ? "" : "|") + std::string{ name };
			}
		}
		return text.empty() ? "none" : text;
	}

	void ObserveRoutine(RE::Actor* target, float magnitude, RE::Actor* aggressor, const void* caller, bool suppressed)
	{
		lastRoutineTick.store(GetTickCount64(), std::memory_order_relaxed);
		if (TakeSample(routineCount, "the stagger routine")) {
			float ward = 0.0F;
			bool staggering = false;
			if (target) {
				ward = target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWardPower);
				staggering = target->IsStaggering();
			}
			logger::info("Stagger diagnostic routine: source={} suppressed={} caller={} target=[{}] aggressor=[{}] magnitude={} wardPower={} alreadyStaggering={} inStaggerEffect={}",
				StaggerGate::Allowed() ? "poise" : "vanilla-or-other", suppressed, Origin(caller), Describe(target),
				Describe(aggressor), magnitude, ward, staggering, effectDepth > 0);
		}
	}

	template <class T>
	struct EffectTrace
	{
		static inline void (*original)(RE::ActiveEffect*) = nullptr;

		static void Thunk(RE::ActiveEffect* effect)
		{
			if (effect && TakeSample(effectCount, "effect starts")) {
				const auto caster = effect->GetCasterActor();
				const auto* base = effect->effect ? effect->effect->baseEffect : nullptr;
				logger::info("Stagger diagnostic {}::Start: effect={:08X} '{}' spell={:08X} '{}' magnitude={} caster=[{}] target=[{}]",
					T::name, base ? base->GetFormID() : 0, base ? base->GetName() : "",
					effect->spell ? effect->spell->GetFormID() : 0, effect->spell ? effect->spell->GetName() : "",
					effect->magnitude, Describe(caster.get()), Describe(effect->GetTargetActor()));
			}
			++effectDepth;
			original(effect);
			--effectDepth;
		}
	};

	struct StaggerEffectName { static constexpr auto name = "StaggerEffect"; };
	struct DisarmEffectName { static constexpr auto name = "DisarmEffect"; };

	template <class T>
	void InstallEffectTrace(REL::Relocation<std::uintptr_t>& vtable)
	{
		const auto slot = vtable.address() + 0x14 * sizeof(std::uintptr_t);
		const auto current = *reinterpret_cast<const std::uintptr_t*>(slot);
		if (!IsExecutable(current)) {
			logger::warn("Stagger diagnostic {}::Start slot is not executable; leaving it untouched.", T::name);
			return;
		}
		EffectTrace<T>::original = reinterpret_cast<void (*)(RE::ActiveEffect*)>(current);
		vtable.write_vfunc(0x14, EffectTrace<T>::Thunk);
		const bool installed = *reinterpret_cast<const std::uintptr_t*>(slot) ==
			reinterpret_cast<std::uintptr_t>(&EffectTrace<T>::Thunk);
		logger::info("Stagger diagnostic {}::Start hook installation: {}.", T::name, installed ? "passed" : "FAILED");
	}

	using NotifyFunction = bool (*)(RE::IAnimationGraphManagerHolder*, const RE::BSFixedString&);

	struct PlayerName { static constexpr auto name = "player"; };
	struct CharacterName { static constexpr auto name = "character"; };

	bool InterestingEvent(const char* name)
	{
		if (!name) {
			return false;
		}
		const std::string_view text{ name };
		if (StartsWithI(text, "staggerStop")) {
			return false;
		}
		return StartsWithI(text, "stagger") || ContainsI(text, "recoil") || ContainsI(text, "wardbreak") ||
			StartsWithI(text, "block") || StartsWithI(text, "knock") || StartsWithI(text, "ragdoll") ||
			StartsWithI(text, "poise_") || StartsWithI(text, "msl_");
	}

	// Lists distinct code addresses found on the stack above this frame. Unlike an unwind it still works through
	// hooks without unwind data, so it reveals which mods are on the call path even when the unwind stops early.
	std::string ScanStackForCode()
	{
		const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
		const auto top = reinterpret_cast<std::uintptr_t>(tib->StackBase);
		auto cursor = reinterpret_cast<std::uintptr_t>(_AddressOfReturnAddress());
		std::vector<std::string> found;
		for (std::size_t i = 0; i < 768 && found.size() < 16 && cursor + sizeof(std::uintptr_t) <= top;
			++i, cursor += sizeof(std::uintptr_t)) {
			const auto value = *reinterpret_cast<const std::uintptr_t*>(cursor);
			if (!IsExecutable(value)) {
				continue;
			}
			auto origin = Origin(reinterpret_cast<const void*>(value));
			if (origin == "unknown" || std::find(found.begin(), found.end(), origin) != found.end()) {
				continue;
			}
			found.push_back(std::move(origin));
		}
		std::string text;
		for (const auto& entry : found) {
			text += (text.empty() ? "" : " | ") + entry;
		}
		return text.empty() ? "none" : text;
	}

	// The graph variables a sender writes just before staggerStart; the values act as a fingerprint of the sender.
	std::string StaggerVariables(RE::IAnimationGraphManagerHolder* holder)
	{
		float magnitude = -1.0F;
		float direction = -1.0F;
		if (holder) {
			holder->GetGraphVariableFloat("staggerMagnitude", magnitude);
			holder->GetGraphVariableFloat("staggerDirection", direction);
		}
		return std::format("staggerMagnitude={} staggerDirection={}", magnitude, direction);
	}

	// Names the call chain of a stagger event sent by something other than poise. The immediate caller is often
	// another mod's NotifyAnimationGraph hook, so the deeper frames identify the real sender.
	void LogSenderStack(const char* label, RE::IAnimationGraphManagerHolder* holder)
	{
		if (stackCount.fetch_add(1, std::memory_order_relaxed) >= 150) {
			return;
		}
		void* frames[24]{};
		const auto captured = CaptureStackBackTrace(2, static_cast<DWORD>(std::size(frames)), frames, nullptr);
		std::string chain;
		for (USHORT i = 0; i < captured; ++i) {
			chain += (chain.empty() ? "" : " <- ") + Origin(frames[i]);
		}
		auto* actor = holder ? static_cast<RE::Actor*>(holder) : nullptr;
		logger::info("Stagger diagnostic non-poise {} sender: actor=[{}] {} blocking={}",
			label, Describe(actor), StaggerVariables(holder), actor ? actor->IsBlocking() : false);
		logger::info("  unwound frames({}): {}", captured, chain.empty() ? "none" : chain);
		logger::info("  stack code addresses: {}", ScanStackForCode());
	}

	template <class T>
	struct NotifyTrace
	{
		static inline NotifyFunction original = nullptr;

		static bool Thunk(RE::IAnimationGraphManagerHolder* holder, const RE::BSFixedString& event)
		{
			const void* caller = _ReturnAddress();
			constexpr bool isPlayer = std::is_same_v<T, PlayerName>;
			if constexpr (isPlayer) {
				RecordPlayerEvent(event.c_str(), caller);
			}
			if (InterestingEvent(event.c_str()) && TakeSample(eventCount, "animation events")) {
				auto* actor = holder ? static_cast<RE::Actor*>(holder) : nullptr;
				const bool isStagger = event.c_str() && StartsWithI(event.c_str(), "staggerStart");
				logger::info("Stagger diagnostic {} event: event='{}' actor=[{}] caller={} poiseAllowed={} inStaggerEffect={}{}",
					T::name, event.c_str(), Describe(actor), Origin(caller), StaggerGate::Allowed(), effectDepth > 0,
					isStagger ? " " + StaggerVariables(holder) : std::string{});
			}
			if (!StaggerGate::Allowed() && effectDepth == 0 && event.c_str() && StartsWithI(event.c_str(), "staggerStart")) {
				LogSenderStack(T::name, holder);
			}
			const bool result = original(holder, event);
			if constexpr (isPlayer) {
				auto* actor = holder ? static_cast<RE::Actor*>(holder) : nullptr;
				if (actor && actor->IsStaggering()) {
					if (!playerWasStaggering.exchange(true)) {
						DumpRecentEvents(actor);
					}
				} else {
					playerWasStaggering.store(false);
				}
			}
			return result;
		}
	};

	template <class T>
	void InstallNotifyTrace(REL::Relocation<std::uintptr_t>& vtable)
	{
		const auto slot = vtable.address() + 1 * sizeof(std::uintptr_t);
		const auto current = *reinterpret_cast<const std::uintptr_t*>(slot);
		if (!IsExecutable(current) || current == reinterpret_cast<std::uintptr_t>(&NotifyTrace<T>::Thunk)) {
			logger::warn("Stagger diagnostic {} NotifyAnimationGraph slot validation FAILED; leaving it untouched.", T::name);
			return;
		}
		NotifyTrace<T>::original = reinterpret_cast<NotifyFunction>(current);
		vtable.write_vfunc(1, NotifyTrace<T>::Thunk);
		const bool installed = *reinterpret_cast<const std::uintptr_t*>(slot) ==
			reinterpret_cast<std::uintptr_t>(&NotifyTrace<T>::Thunk);
		logger::info("Stagger diagnostic {} NotifyAnimationGraph hook installation (chained to 0x{:X} [{}]): {}.",
			T::name, current, Origin(reinterpret_cast<const void*>(current)), installed ? "passed" : "FAILED");
	}

	void LogRelatedModules()
	{
		const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
		if (snapshot == INVALID_HANDLE_VALUE) {
			logger::warn("Stagger diagnostic could not enumerate loaded modules.");
			return;
		}
		MODULEENTRY32W entry{};
		entry.dwSize = sizeof(entry);
		std::string found;
		std::string plugins;
		constexpr std::array<std::string_view, 9> keywords{
			"stagger", "poise", "valhalla", "combat", "precision", "truehud", "stun", "parry", "animationreplacer"
		};
		for (BOOL ok = Module32FirstW(snapshot, &entry); ok; ok = Module32NextW(snapshot, &entry)) {
			const auto name = NarrowFileName(entry.szModule, std::wcslen(entry.szModule));
			if (std::any_of(keywords.begin(), keywords.end(), [&](std::string_view keyword) { return ContainsI(name, keyword); })) {
				found += (found.empty() ? "" : ", ") + name;
			}
			std::string path;
			for (const wchar_t* c = entry.szExePath; *c; ++c) {
				path.push_back(*c < 128 ? static_cast<char>(*c) : '?');
			}
			if (ContainsI(path, "skse\\plugins") || ContainsI(path, "skse/plugins")) {
				plugins += (plugins.empty() ? "" : ", ") + name;
			}
		}
		CloseHandle(snapshot);
		logger::info("Stagger diagnostic related loaded modules: {}", found.empty() ? "none" : found);
		logger::info("Stagger diagnostic loaded SKSE plugin DLLs: {}", plugins.empty() ? "none found by path" : plugins);
	}
}

void StaggerDiagnostic::OnHit(const HitSample& sample)
{
	if (!sample.target || hitCount.fetch_add(1, std::memory_order_relaxed) >= kLimit) {
		return;
	}
	constexpr std::array<const char*, 4> typeNames{ "melee", "bash", "creature", "projectile" };
	std::string weapon = "none";
	std::string source = "none";
	int skill = -1;
	std::string flags = "n/a";
	if (sample.data) {
		const auto& data = *sample.data;
		flags = DescribeHitFlags(data);
		skill = static_cast<int>(data.skill);
		if (data.weapon) {
			weapon = std::format("{:08X} '{}' type={}", data.weapon->GetFormID(),
				data.weapon->GetName() ? data.weapon->GetName() : "", static_cast<int>(data.weapon->GetWeaponType()));
		}
		if (const auto ref = data.sourceRef.get()) {
			source = ref->AsProjectile() ? "projectile" : "other-ref";
		}
	}
	logger::info("Stagger diagnostic hit: target=[{}] aggressor=[{}] eligible={} classified={} type={} flags={} incomingStagger={} weapon={} source={} skill={} handled={} broke={} poise {}->{} of {} blocking={} stillStaggering={}",
		Describe(sample.target), Describe(sample.aggressor), sample.eligible, sample.classified,
		sample.hitType >= 0 && sample.hitType < static_cast<int>(typeNames.size()) ? typeNames[sample.hitType] : "unclassified",
		flags, sample.incomingStagger, weapon, source, skill, sample.handled, sample.broke,
		sample.poiseBefore, sample.poiseAfter, sample.poiseMax, sample.target->IsBlocking(), sample.target->IsStaggering());
}

void StaggerDiagnostic::OnDamageSample(const HitSample& sample)
{
	if (!sample.target || !sample.data || !sample.aggressor ||
		(!sample.target->IsPlayerRef() && !sample.aggressor->IsPlayerRef()) ||
		sample.data->flags.any(RE::HitData::Flag::kPredictDamage) ||
		!TakeSample(damageCount, "hit damage")) {
		return;
	}
	const auto& data = *sample.data;
	std::string weapon = "none";
	if (data.weapon) {
		weapon = std::format("{:08X} '{}' baseDamage={}", data.weapon->GetFormID(),
			data.weapon->GetName() ? data.weapon->GetName() : "", data.weapon->GetAttackDamage());
	}
	logger::info("Stagger diagnostic damage: target=[{}] aggressor=[{}] aggressorSneaking={} flags={} physicalDamage={} totalDamage={} sneakAttackBonus={} criticalDamageMult={} bonusHealthDamageMult={} percentBlocked={} resistedPhysical={} resistedTyped={} reflected={} limb={} stagger={} pushBack={} weapon={}",
		Describe(sample.target), Describe(sample.aggressor), sample.aggressor->IsSneaking(), DescribeHitFlags(data),
		data.physicalDamage, data.totalDamage, data.sneakAttackBonus, data.criticalDamageMult,
		data.bonusHealthDamageMult, data.percentBlocked, data.resistedPhysicalDamage, data.resistedTypedDamage,
		data.reflectedDamage, data.targetedLimbDamage, data.stagger, data.pushBack, weapon);
}

void StaggerDiagnostic::Install()
{
	const auto& module = REL::Module::get();
	if (module.version() != SKSE::RUNTIME_SSE_1_7_104) {
		logger::warn("Stagger diagnostic hooks disabled: only runtime 1.7.104.0 is validated.");
		return;
	}
	logger::warn("Stagger diagnostic build active: logging only; the routine trace observes the production suppression gate.");
	LogRelatedModules();
	StaggerSuppression::SetRoutineObserver(ObserveRoutine);
	REL::Relocation<std::uintptr_t> staggerEffect{ RE::VTABLE_StaggerEffect[0] };
	REL::Relocation<std::uintptr_t> disarmEffect{ RE::VTABLE_DisarmEffect[0] };
	InstallEffectTrace<StaggerEffectName>(staggerEffect);
	InstallEffectTrace<DisarmEffectName>(disarmEffect);
	const auto index = StaggerSuppression::HolderVTableIndex();
	if (!index || *index >= RE::VTABLE_Character.size()) {
		logger::warn("Stagger diagnostic could not locate the animation-graph holder vtable; animation event tracing disabled.");
		return;
	}
	logger::info("Stagger diagnostic animation-graph holder vtable index: {}.", *index);
	REL::Relocation<std::uintptr_t> player{ RE::VTABLE_PlayerCharacter[*index] };
	REL::Relocation<std::uintptr_t> character{ RE::VTABLE_Character[*index] };
	InstallNotifyTrace<PlayerName>(player);
	InstallNotifyTrace<CharacterName>(character);
}
#endif
