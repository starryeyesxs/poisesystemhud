#include "rogue_stagger_diagnostic.h"

#ifdef ARPGHUD_ROGUE_STAGGER_DIAGNOSTICS
#include "builtin_poise.h"
#include "hook_call.h"
#include "stagger_gate.h"
#include "stagger_suppression.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <format>
#include <knownfolders.h>
#include <mutex>
#include <shlobj.h>
#include <string_view>
#include <vector>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace
{
	constexpr ULONGLONG kRecentWindowMs = 150;
	constexpr ULONGLONG kPostHitWindowMs = 250;
	constexpr std::size_t kRecentCapacity = 64;
	constexpr USHORT kStackFrames = 128;

	struct RecentHit
	{
		ULONGLONG tick = 0;
		RE::FormID target = 0;
		RE::FormID attacker = 0;
		std::string attackerName;
		std::string weapon;
		RE::FormID weaponID = 0;
		float staggerBefore = 0.0F;
		float staggerAfter = 0.0F;
		float pushBack = 0.0F;
		std::string flags;
	};

	struct RecentMagic
	{
		ULONGLONG tick = 0;
		RE::FormID target = 0;
		RE::FormID caster = 0;
		std::string spell;
		RE::FormID spellID = 0;
		std::string enchantment;
		RE::FormID enchantmentID = 0;
		std::string effect;
		RE::FormID effectID = 0;
		std::string source;
		RE::FormID sourceID = 0;
		float magnitude = 0.0F;
		float explosionForce = 0.0F;
		float explosionDamage = 0.0F;
		bool explosion = false;
		bool area = false;
	};

	struct RecentPush
	{
		ULONGLONG tick = 0;
		RE::FormID target = 0;
		RE::FormID aggressor = 0;
		float force = 0.0F;
		std::string origin;
	};

	std::mutex contextMutex;
	std::array<RecentHit, kRecentCapacity> recentHits{};
	std::array<RecentMagic, kRecentCapacity> recentMagic{};
	std::array<RecentPush, kRecentCapacity> recentPushes{};
	std::size_t nextHit = 0;
	std::size_t nextMagic = 0;
	std::size_t nextPush = 0;
	std::size_t hitCount = 0;
	std::size_t magicCount = 0;
	std::size_t pushCount = 0;
	std::filesystem::path logPath;
	std::string latestAlert;
	std::atomic<ULONGLONG> playerHitTick{ 0 };

	std::string NarrowPath(const wchar_t* path)
	{
		std::string result;
		while (path && *path) {
			result.push_back(*path < 128 ? static_cast<char>(*path) : '?');
			++path;
		}
		return result;
	}

	std::string SafeName(const RE::TESForm* form)
	{
		const char* name = form ? form->GetName() : nullptr;
		return name && *name ? name : "<unnamed>";
	}

	std::string ActorName(RE::Actor* actor)
	{
		const char* name = actor ? actor->GetName() : nullptr;
		return name && *name ? name : "<unnamed>";
	}

	std::string Origin(const void* address)
	{
		HMODULE owner = nullptr;
		if (!address || !GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(address), &owner) || !owner) {
			return "unknown";
		}
		wchar_t path[MAX_PATH]{};
		const auto length = GetModuleFileNameW(owner, path, static_cast<DWORD>(std::size(path)));
		const auto text = std::filesystem::path(std::wstring_view(path, length)).filename().string();
		return std::format("{}+0x{:X}", text,
			reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(owner));
	}

	std::string StackTrace()
	{
		void* frames[kStackFrames]{};
		const auto captured = CaptureStackBackTrace(2, kStackFrames, frames, nullptr);
		std::string output;
		for (USHORT i = 0; i < captured; ++i) {
			output += std::format("{}#{}={}", output.empty() ? "" : " ", i + 1, Origin(frames[i]));
		}
		return output.empty() ? "none" : output;
	}

	std::string HitFlags(const RE::HitData& data)
	{
		using Flag = RE::HitData::Flag;
		constexpr std::array<std::pair<Flag, const char*>, 15> names{ {
			{ Flag::kBlocked, "blocked" }, { Flag::kBlockWithWeapon, "weaponBlock" },
			{ Flag::kBlockCandidate, "blockCandidate" }, { Flag::kCritical, "critical" },
			{ Flag::kSneakAttack, "sneak" }, { Flag::kBash, "bash" }, { Flag::kTimedBash, "timedBash" },
			{ Flag::kPowerAttack, "power" }, { Flag::kLeftHand, "left" }, { Flag::kMeleeAttack, "melee" },
			{ Flag::kRicochet, "ricochet" }, { Flag::kExplosion, "explosion" },
			{ Flag::kPredictDamage, "predict" }, { Flag::kDisarm, "disarm" }, { Flag::kDisableWeapon, "disableWeapon" }
		} };
		std::string output;
		for (const auto& [flag, name] : names) {
			if (data.flags.any(flag)) {
				output += (output.empty() ? "" : "|") + std::string{ name };
			}
		}
		return output.empty() ? "none" : output;
	}

	std::filesystem::path ResolveLogPath()
	{
		PWSTR documents = nullptr;
		const auto result = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents);
		std::filesystem::path path;
		if (SUCCEEDED(result) && documents) {
			path = std::filesystem::path(documents) / L"My Games" / L"Skyrim Special Edition" / L"SKSE";
			CoTaskMemFree(documents);
		} else {
			const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
			std::wstring profile(length ? length - 1 : 0, L'\0');
			if (length && GetEnvironmentVariableW(L"USERPROFILE", profile.data(), length)) {
				path = std::filesystem::path(profile) / L"Documents" / L"My Games" / L"Skyrim Special Edition" / L"SKSE";
			}
		}
		std::error_code error;
		std::filesystem::create_directories(path, error);
		return path / L"RogueStagger.log";
	}

	void AppendLog(const std::string& text)
	{
		std::ofstream file(logPath, std::ios::app);
		if (file) {
			file << text << '\n';
			file.flush();
		} else {
			logger::error("Rogue stagger diagnostic could not open {}.", logPath.string());
		}
		logger::warn("{}", text);
	}

	std::string Timestamp()
	{
		SYSTEMTIME time{};
		GetLocalTime(&time);
		return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
			time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
	}

	std::string DescribeRecent(RE::Actor* target)
	{
		const auto now = GetTickCount64();
		const auto targetID = target ? target->GetFormID() : 0;
		std::string output = " recent[150ms]={";
		bool found = false;
		for (std::size_t i = 0, count = (std::min)(hitCount, recentHits.size()); i < count; ++i) {
			const auto& hit = recentHits[(nextHit + recentHits.size() - count + i) % recentHits.size()];
			if (hit.target != targetID || now - hit.tick > kRecentWindowMs) {
				continue;
			}
			output += std::format("{}hit(age={}ms attacker={:08X} weapon={:08X} '{}' stagger={:.3f}->{:.3f} pushBack={:.3f} flags={})",
				found ? " " : "", now - hit.tick, hit.attacker, hit.weaponID, hit.weapon,
				hit.staggerBefore, hit.staggerAfter, hit.pushBack, hit.flags);
			found = true;
		}
		for (std::size_t i = 0, count = (std::min)(magicCount, recentMagic.size()); i < count; ++i) {
			const auto& magic = recentMagic[(nextMagic + recentMagic.size() - count + i) % recentMagic.size()];
			if (magic.target != targetID || now - magic.tick > kRecentWindowMs) {
				continue;
			}
			output += std::format(
				"{}magic(age={}ms caster={:08X} spell={:08X} '{}' enchantment={:08X} '{}' effect={:08X} '{}' source={:08X} '{}' magnitude={:.3f} area={} explosion={} force={:.3f} damage={:.3f})",
				found ? " " : "", now - magic.tick, magic.caster, magic.spellID, magic.spell,
				magic.enchantmentID, magic.enchantment, magic.effectID, magic.effect, magic.sourceID, magic.source,
				magic.magnitude, magic.area, magic.explosion, magic.explosionForce, magic.explosionDamage);
			found = true;
		}
		for (std::size_t i = 0, count = (std::min)(pushCount, recentPushes.size()); i < count; ++i) {
			const auto& push = recentPushes[(nextPush + recentPushes.size() - count + i) % recentPushes.size()];
			if (push.target != targetID || now - push.tick > kRecentWindowMs) {
				continue;
			}
			output += std::format("{}push(age={}ms aggressor={:08X} force={:.3f} origin={})",
				found ? " " : "", now - push.tick, push.aggressor, push.force, push.origin);
			found = true;
		}
		return output + (found ? "}" : "none}");
	}

	void Report(const char* entry, RE::Actor* target, RE::Actor* aggressor, float magnitude, const char* event = nullptr,
		const void* caller = nullptr, bool suppressed = false)
	{
		if (!target || StaggerGate::Allowed() || suppressed) {
			return;
		}
		BuiltinPoise::DiagnosticState poise{};
		if (!BuiltinPoise::ReadDiagnosticState(target, poise) || poise.current <= 0.0F) {
			return;
		}
		const auto now = GetTickCount64();
		float direction = -1.0F;
		if (target) {
			target->GetGraphVariableFloat("staggerDirection", direction);
		}
		const auto* state = target->AsActorState();
		const auto* values = target->AsActorValueOwner();
		const auto callerText = Origin(caller);
		std::string context;
		std::string culprit = "none";
		std::string recentAttackerName;
		RE::FormID recentAttackerID = 0;
		{
			std::lock_guard lock(contextMutex);
			context = DescribeRecent(target);
			if (nextHit > 0 && hitCount > 0) {
				const auto& hit = recentHits[(nextHit - 1) % recentHits.size()];
				if (hit.target == target->GetFormID() && now - hit.tick <= kRecentWindowMs) {
					culprit = hit.weapon;
					recentAttackerName = hit.attackerName;
					recentAttackerID = hit.attacker;
				}
			}
			if (culprit == "none" && nextMagic > 0 && magicCount > 0) {
				const auto& magic = recentMagic[(nextMagic - 1) % recentMagic.size()];
				if (magic.target == target->GetFormID() && now - magic.tick <= kRecentWindowMs) {
					culprit = magic.spell != "<unnamed>" ? magic.spell : magic.effect;
				}
			}
		}
		const auto message = std::format(
			"[ROGUE STAGGER] timestamp={} entry={} event='{}' caller={} target='{}'/{:08X} attacker='{}'/{:08X} "
			"poise={:.3f}/{:.3f} immunity={:.3f}s debuffs={} blocking={} wardPower={:.3f} sprinting={} casting={} "
			"staggerMagnitude={:.3f} staggerDirection={:.3f} stack={}{}",
			Timestamp(), entry, event ? event : "", callerText, ActorName(target), target->GetFormID(),
			aggressor ? ActorName(aggressor) : (recentAttackerName.empty() ? "unknown" : recentAttackerName),
			aggressor ? aggressor->GetFormID() : recentAttackerID, poise.current, poise.maximum,
			poise.immunity, poise.debuffs, target->IsBlocking(), values->GetActorValue(RE::ActorValue::kWardPower),
			state && state->IsSprinting(), target->WhoIsCasting() != 0, magnitude, direction, StackTrace(), context);
		{
			std::lock_guard lock(contextMutex);
			latestAlert = std::format("[ROGUE STAGGER] {} | {} | {}", entry, callerText, culprit);
		}
		AppendLog(message);
	}

	void ObserveRoutine(RE::Actor* target, float magnitude, RE::Actor* aggressor, const void* caller, bool suppressed)
	{
		Report("Actor::TriggerStagger", target, aggressor, magnitude, nullptr, caller, suppressed);
	}

	bool NormalMovementEvent(std::string_view event)
	{
		constexpr std::array<std::string_view, 18> prefixes{
			"turn", "foot", "walk", "run", "sprint", "sneak", "idle", "land", "jump", "fall",
			"swim", "strafe", "move", "head", "weapondraw", "weaponsheathe", "equip", "unequip"
		};
		return std::any_of(prefixes.begin(), prefixes.end(), [&](std::string_view prefix) {
			return event.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), event.begin(),
				[](char expected, char actual) {
					return std::tolower(static_cast<unsigned char>(expected)) ==
						std::tolower(static_cast<unsigned char>(actual));
				});
		});
	}

	bool PlayerHasPositivePoise(RE::Actor*& player, BuiltinPoise::DiagnosticState& poise)
	{
		player = RE::PlayerCharacter::GetSingleton();
		return player && !StaggerGate::Allowed() &&
			BuiltinPoise::ReadDiagnosticState(player, poise) && poise.current > 0.0F;
	}

	void ReportPostHitGraphEvent(RE::Actor* target, const char* event, const char* payload, const char* source,
		const void* caller)
	{
		if (!target || !event || !target->IsPlayerRef()) {
			return;
		}
		const auto tick = GetTickCount64();
		const auto hitTick = playerHitTick.load(std::memory_order_relaxed);
		if (!hitTick || tick < hitTick || tick - hitTick > kPostHitWindowMs) {
			return;
		}
		BuiltinPoise::DiagnosticState poise{};
		if (!PlayerHasPositivePoise(target, poise)) {
			return;
		}
		const auto* values = target->AsActorValueOwner();
		const auto callerText = Origin(caller);
		float magnitude = -1.0F;
		float direction = -1.0F;
		target->GetGraphVariableFloat("staggerMagnitude", magnitude);
		target->GetGraphVariableFloat("staggerDirection", direction);
		std::string context;
		{
			std::lock_guard lock(contextMutex);
			context = DescribeRecent(target);
		}
		const auto text = std::format(
			"[POST-HIT GRAPH] timestamp={} ageMs={} source={} event='{}' payload='{}' caller={} target='{}'/{:08X} "
			"poise={:.3f}/{:.3f} "
			"blocking={} attacking={} casting={} wardPower={:.3f} staggerMagnitude={:.3f} staggerDirection={:.3f} "
			"stack={}{}",
			Timestamp(), tick - hitTick, source, event, payload ? payload : "", callerText, ActorName(target), target->GetFormID(),
			poise.current, poise.maximum, target->IsBlocking(), target->IsAttacking(), target->WhoIsCasting() != 0,
			values->GetActorValue(RE::ActorValue::kWardPower), magnitude, direction, StackTrace(), context);
		AppendLog(text);
		if (!NormalMovementEvent(event)) {
			std::lock_guard lock(contextMutex);
			latestAlert = std::format("[POST-HIT EVENT: {} | {}]", event, callerText);
		}
	}

	std::string VectorText(const RE::hkVector4& vector)
	{
		alignas(16) float components[4]{};
		_mm_store_ps(components, vector.quad);
		return std::format("({:.3f},{:.3f},{:.3f},{:.3f})",
			components[0], components[1], components[2], components[3]);
	}

	void ReportPhysics(const char* entry, const void* caller, float force, float stagger, float pushBack,
		const RE::hkVector4* velocity = nullptr, std::string_view details = {})
	{
		const auto tick = GetTickCount64();
		const auto hitTick = playerHitTick.load(std::memory_order_relaxed);
		if (!hitTick || tick < hitTick || tick - hitTick > kPostHitWindowMs) {
			return;
		}
		RE::Actor* player = nullptr;
		BuiltinPoise::DiagnosticState poise{};
		if (!PlayerHasPositivePoise(player, poise)) {
			return;
		}
		const auto callerText = Origin(caller);
		std::string velocityText;
		if (velocity) {
			velocityText = std::format(" velocity={}", VectorText(*velocity));
		}
		if (const auto* controller = player->GetCharController()) {
			velocityText += std::format(
				" initialVelocity={} velocityTime={:.6f} outVelocity={} velocityMod={} pushDelta={}",
				VectorText(controller->initialVelocity), controller->velocityTime,
				VectorText(controller->outVelocity), VectorText(controller->velocityMod),
				VectorText(controller->pushDelta));
		}
		const auto text = std::format(
			"[POST-HIT PHYSICS] timestamp={} ageMs={} entry={} caller={} target='{}'/{:08X} poise={:.3f}/{:.3f} "
			"force={:.3f} hitDataStagger={:.3f} hitDataPushBack={:.3f}{} details='{}' stack={}",
			Timestamp(), tick - hitTick, entry, callerText, ActorName(player), player->GetFormID(),
			poise.current, poise.maximum, force, stagger, pushBack, velocityText, details, StackTrace());
		AppendLog(text);
		{
			std::lock_guard lock(contextMutex);
			latestAlert = std::format("[POST-HIT EVENT: {} | {}]", entry, callerText);
		}
	}

	using NotifyFunction = bool (*)(RE::IAnimationGraphManagerHolder*, const RE::BSFixedString&);

	template <class T>
	struct NotifyObserver
	{
		static inline NotifyFunction original = nullptr;

		static bool Thunk(RE::IAnimationGraphManagerHolder* holder, const RE::BSFixedString& event)
		{
			const auto* name = event.c_str();
			auto* target = holder ? static_cast<RE::Actor*>(holder) : nullptr;
			if (target && name) {
				ReportPostHitGraphEvent(target, name, "", "NotifyAnimationGraph", _ReturnAddress());
			}
			if (target && name && (StaggerGate::IsStaggerStartEvent(name) || StaggerGate::IsWardBreakEvent(name))) {
				float magnitude = -1.0F;
				target->GetGraphVariableFloat("staggerMagnitude", magnitude);
				Report("NotifyAnimationGraph", target, nullptr, magnitude, name, _ReturnAddress(),
					BuiltinPoise::ShouldSuppressStagger(target));
			}
			return original(holder, event);
		}
	};

	struct PlayerTag { static constexpr auto name = "player"; };
	struct CharacterTag { static constexpr auto name = "character"; };

	class PlayerGraphEventObserver final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* event,
			RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
		{
			if (event && event->holder) {
				auto* target = event->holder->As<RE::Actor>();
				ReportPostHitGraphEvent(target, event->tag.c_str(), event->payload.c_str(),
					"ProcessEvent", _ReturnAddress());
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	PlayerGraphEventObserver playerGraphEventObserver;

	void RefreshPlayerGraphSink(RE::Actor* player)
	{
		RE::BSAnimationGraphManagerPtr manager;
		if (!player->GetAnimationGraphManager(manager) || !manager || manager->graphs.empty()) {
			logger::warn("Rogue stagger player graph event sink unavailable; NotifyAnimationGraph observation remains active.");
			return;
		}
		for (const auto& graph : manager->graphs) {
			graph->GetEventSource<RE::BSAnimationGraphEvent>()->AddEventSink(&playerGraphEventObserver);
		}
	}

	using HavokImpulseFunction = RE::BSScript::Variable (*)(RE::SkyrimScript::DelayFunctor*);
	struct ApplyHavokImpulseObserver
	{
		static inline HavokImpulseFunction original = nullptr;

		static RE::BSScript::Variable Thunk(RE::SkyrimScript::DelayFunctor* functor)
		{
			ReportPhysics("ApplyHavokImpulse", _ReturnAddress(), 0.0F, 0.0F, 0.0F);
			return original(functor);
		}
	};

	bool IsExecutable(const void* address)
	{
		MEMORY_BASIC_INFORMATION memory{};
		return address && VirtualQuery(address, &memory, sizeof(memory)) == sizeof(memory) &&
			memory.State == MEM_COMMIT && !(memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
			(memory.Protect & 0xFF) >= PAGE_EXECUTE && (memory.Protect & 0xFF) <= PAGE_EXECUTE_WRITECOPY;
	}

	using HitPhysicsFunction = void (*)(RE::Actor*, RE::Actor*, float, float,
		const RE::NiPoint3*, const RE::NiPoint3*, bool, RE::HitData*);
	HitPhysicsFunction originalHitPhysics = nullptr;

	void ObserveHitPhysics(RE::Actor* target, RE::Actor* aggressor, float damage, float force,
		const RE::NiPoint3* direction, const RE::NiPoint3* position, bool useHitDirection, RE::HitData* data)
	{
		if (target && target->IsPlayerRef()) {
			ReportPhysics("Actor::hit-physics/Havok", _ReturnAddress(), force,
				data ? data->stagger : 0.0F, data ? data->pushBack : 0.0F);
		}
		originalHitPhysics(target, aggressor, damage, force, direction, position, useHitDirection, data);
	}

	void InstallHitPhysicsObserver()
	{
		const auto base = REL::Module::get().base();
		const auto site = base + 0x6CAF17;
		constexpr std::array<std::uint8_t, 12> arguments{
			0xF3, 0x41, 0x0F, 0x10, 0x55, 0x50, 0x48, 0x8B, 0xD3, 0x48, 0x8B, 0xCE
		};
		const auto destination = HookCall::ExecutableTarget(site);
		if (destination != base + 0x6CECB0 ||
			std::memcmp(reinterpret_cast<const void*>(site - arguments.size()), arguments.data(), arguments.size()) != 0) {
			logger::error("Rogue stagger native hit-physics observer validation FAILED; call left unchanged.");
			return;
		}
		originalHitPhysics = reinterpret_cast<HitPhysicsFunction>(destination);
		SKSE::GetTrampoline().write_call<5>(site, ObserveHitPhysics);
		logger::info("Rogue stagger native hit-physics observer installed.");
	}

	using HitVelocityFunction = void (*)(RE::bhkCharacterController*, const RE::NiPoint3&, float);
	HitVelocityFunction originalHitVelocity = nullptr;

	void ObserveHitVelocity(RE::bhkCharacterController* controller, const RE::NiPoint3& input, float duration)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		const bool observed = player && player->GetCharController() == controller;
		const auto* caller = _ReturnAddress();
		const auto details = std::format("input=({:.6f},{:.6f},{:.6f}) duration={:.6f}",
			input.x, input.y, input.z, duration);
		if (observed) {
			ReportPhysics("Actor::hit-velocity/before", caller, 0.0F, 0.0F, 0.0F, nullptr, details);
		}
		originalHitVelocity(controller, input, duration);
		if (observed) {
			ReportPhysics("Actor::hit-velocity/after", caller, 0.0F, 0.0F, 0.0F, nullptr, details);
		}
	}

	void InstallHitVelocityObserver()
	{
		const auto base = REL::Module::get().base();
		const auto site = base + 0x6CB36D;
		constexpr std::array<std::uint8_t, 7> arguments{ 0x48, 0x8D, 0x55, 0x00, 0x48, 0x8B, 0xC8 };
		const auto destination = HookCall::ExecutableTarget(site);
		if (destination != base + 0x1065F50 ||
			std::memcmp(reinterpret_cast<const void*>(site - arguments.size()), arguments.data(), arguments.size()) != 0) {
			logger::error("Rogue stagger native hit-velocity observer validation FAILED; call left unchanged.");
			return;
		}
		originalHitVelocity = reinterpret_cast<HitVelocityFunction>(destination);
		SKSE::GetTrampoline().write_call<5>(site, ObserveHitVelocity);
		logger::info("Rogue stagger native hit-velocity observer installed at SkyrimSE.exe+0x6CB36D.");
	}

	template <class T>
	struct ControllerVelocityObserver
	{
		static inline void (*original)(RE::bhkCharacterController*, const RE::hkVector4&) = nullptr;

		static void Thunk(RE::bhkCharacterController* controller, const RE::hkVector4& velocity)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (player && player->GetCharController() == controller) {
				ReportPhysics("bhkCharacterController::SetLinearVelocityImpl", _ReturnAddress(),
					velocity.Length3(), 0.0F, 0.0F, &velocity);
			}
			original(controller, velocity);
		}
	};

	struct ProxyControllerTag {};
	struct RigidBodyControllerTag {};

	template <class T>
	void InstallControllerObserver(REL::Relocation<std::uintptr_t>& vtable)
	{
		const auto slot = vtable.address() + 7 * sizeof(std::uintptr_t);
		const auto current = *reinterpret_cast<const std::uintptr_t*>(slot);
		if (!IsExecutable(reinterpret_cast<const void*>(current)) ||
			current == reinterpret_cast<std::uintptr_t>(&ControllerVelocityObserver<T>::Thunk)) {
			logger::error("Rogue stagger character-controller velocity observer validation FAILED.");
			return;
		}
		ControllerVelocityObserver<T>::original =
			reinterpret_cast<decltype(ControllerVelocityObserver<T>::original)>(current);
		vtable.write_vfunc(7, ControllerVelocityObserver<T>::Thunk);
		logger::info("Rogue stagger character-controller velocity observer installed.");
	}

	template <class T>
	void InstallNotifyObserver(REL::Relocation<std::uintptr_t>& vtable)
	{
		const auto slot = vtable.address() + sizeof(std::uintptr_t);
		const auto current = *reinterpret_cast<const std::uintptr_t*>(slot);
		NotifyObserver<T>::original = reinterpret_cast<NotifyFunction>(current);
		if (!IsExecutable(reinterpret_cast<const void*>(current)) ||
			current == reinterpret_cast<std::uintptr_t>(&NotifyObserver<T>::Thunk)) {
			logger::warn("Rogue stagger {} NotifyAnimationGraph observer not installed.", T::name);
			return;
		}
		vtable.write_vfunc(1, NotifyObserver<T>::Thunk);
		logger::info("Rogue stagger {} NotifyAnimationGraph observer installed: {}",
			T::name, *reinterpret_cast<const std::uintptr_t*>(slot) ==
				reinterpret_cast<std::uintptr_t>(&NotifyObserver<T>::Thunk) ? "yes" : "FAILED");
	}

	void InstallHavokImpulseObserver()
	{
		REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_SkyrimScript____ApplyHavokImpulseFunctor[0] };
		const auto slot = vtable.address() + sizeof(std::uintptr_t);
		const auto current = *reinterpret_cast<const std::uintptr_t*>(slot);
		ApplyHavokImpulseObserver::original = reinterpret_cast<HavokImpulseFunction>(current);
		if (!IsExecutable(reinterpret_cast<const void*>(current))) {
			logger::warn("Rogue stagger ApplyHavokImpulse observer not installed.");
			return;
		}
		vtable.write_vfunc(1, ApplyHavokImpulseObserver::Thunk);
		logger::info("Rogue stagger ApplyHavokImpulse observer installed: {}",
			*reinterpret_cast<const std::uintptr_t*>(slot) ==
				reinterpret_cast<std::uintptr_t>(&ApplyHavokImpulseObserver::Thunk) ? "yes" : "FAILED");
	}
}

void RogueStaggerCapture::Install()
{
	if (REL::Module::get().version() != SKSE::RUNTIME_SSE_1_7_104 || !BuiltinPoise::IsInstalled()) {
		logger::error("Rogue stagger diagnostics require validated Skyrim 1.7.104.0 built-in poise hooks.");
		return;
	}
	logPath = ResolveLogPath();
	StaggerSuppression::SetRoutineObserver(ObserveRoutine);
	if (const auto index = StaggerSuppression::HolderVTableIndex(); index && *index < RE::VTABLE_Character.size()) {
		REL::Relocation<std::uintptr_t> player{ RE::VTABLE_PlayerCharacter[*index] };
		REL::Relocation<std::uintptr_t> character{ RE::VTABLE_Character[*index] };
		InstallNotifyObserver<PlayerTag>(player);
		InstallNotifyObserver<CharacterTag>(character);
	} else {
		logger::error("Rogue stagger animation-graph holder vtable not found; NotifyAnimationGraph observers not installed.");
	}
	InstallHavokImpulseObserver();
	InstallHitPhysicsObserver();
	InstallHitVelocityObserver();
	REL::Relocation<std::uintptr_t> proxyController{ RE::VTABLE_bhkCharProxyController[1] };
	REL::Relocation<std::uintptr_t> rigidBodyController{ RE::VTABLE_bhkCharRigidBodyController[0] };
	InstallControllerObserver<ProxyControllerTag>(proxyController);
	InstallControllerObserver<RigidBodyControllerTag>(rigidBodyController);
	if (auto* currentPlayer = RE::PlayerCharacter::GetSingleton(); currentPlayer) {
		RefreshPlayerGraphSink(currentPlayer);
	}
	logger::warn("Rogue stagger diagnostic active; uncapped anomaly-only log={}", logPath.string());
}

void RogueStaggerCapture::NotePhysicalHit(RE::Actor* target, RE::Actor* aggressor, const RE::HitData* data)
{
	if (!data) {
		return;
	}
	RecentHit hit;
	hit.tick = GetTickCount64();
	hit.target = target ? target->GetFormID() : 0;
	hit.attacker = aggressor ? aggressor->GetFormID() : 0;
	hit.attackerName = aggressor ? ActorName(aggressor) : "unknown";
	hit.weapon = data->weapon ? SafeName(data->weapon) : "unarmed";
	hit.weaponID = data->weapon ? data->weapon->GetFormID() : 0;
	hit.staggerBefore = data->stagger;
	hit.staggerAfter = data->stagger;
	hit.pushBack = data->pushBack;
	hit.flags = HitFlags(*data);
	{
		std::lock_guard lock(contextMutex);
		recentHits[nextHit % recentHits.size()] = std::move(hit);
		++nextHit;
		++hitCount;
	}
	if (target && target->IsPlayerRef()) {
		RefreshPlayerGraphSink(target);
		playerHitTick.store(hit.tick, std::memory_order_relaxed);
		ReportPhysics("HitData::incoming", _ReturnAddress(), data->pushBack, data->stagger, data->pushBack,
			nullptr, std::format("physicalDamage={:.3f} reflectedDamage={:.3f} flags={:08X}",
				data->physicalDamage, data->reflectedDamage, static_cast<std::uint32_t>(data->flags.underlying())));
	}
}

void RogueStaggerCapture::NotePhysicalHitComplete(RE::Actor* target, const RE::HitData* data)
{
	if (!data) {
		return;
	}
	const auto targetID = target ? target->GetFormID() : 0;
	{
		std::lock_guard lock(contextMutex);
		for (std::size_t i = 0, count = (std::min)(hitCount, recentHits.size()); i < count; ++i) {
			auto& hit = recentHits[(nextHit + recentHits.size() - 1 - i) % recentHits.size()];
			if (hit.target == targetID) {
				hit.staggerAfter = data->stagger;
				hit.pushBack = data->pushBack;
				hit.flags = HitFlags(*data);
				break;
			}
		}
	}
	if (target && target->IsPlayerRef()) {
		ReportPhysics("HitData::post-handler", _ReturnAddress(), data->pushBack, data->stagger, data->pushBack,
			nullptr, std::format("physicalDamage={:.3f} reflectedDamage={:.3f} flags={:08X}",
				data->physicalDamage, data->reflectedDamage, static_cast<std::uint32_t>(data->flags.underlying())));
	}
}

void RogueStaggerCapture::NoteMagicEffect(RE::MagicTarget* target, RE::MagicTarget::AddTargetData* data)
{
	if (!target || !data) {
		return;
	}
	auto* victim = target->GetTargetStatsObject() ? target->GetTargetStatsObject()->As<RE::Actor>() : nullptr;
	auto* caster = data->caster ? data->caster->As<RE::Actor>() : nullptr;
	RecentMagic magic;
	magic.tick = GetTickCount64();
	magic.target = victim ? victim->GetFormID() : 0;
	magic.caster = caster ? caster->GetFormID() : 0;
	magic.magnitude = data->magnitude;
	magic.area = data->areaTarget;
	magic.spellID = data->magicItem ? data->magicItem->GetFormID() : 0;
	magic.spell = data->magicItem ? SafeName(data->magicItem) : "none";
	magic.effectID = data->effect && data->effect->baseEffect ? data->effect->baseEffect->GetFormID() : 0;
	magic.effect = data->effect && data->effect->baseEffect ? SafeName(data->effect->baseEffect) : "none";
	auto* source = data->source;
	magic.sourceID = source ? source->GetFormID() : 0;
	magic.source = source ? SafeName(source) : "none";
	auto* effectExplosion = data->effect && data->effect->baseEffect ? data->effect->baseEffect->data.explosion : nullptr;
	auto* sourceExplosion = source ? source->As<RE::BGSExplosion>() : nullptr;
	auto* explosion = sourceExplosion ? sourceExplosion : effectExplosion;
	if (explosion) {
		magic.explosion = true;
		magic.sourceID = explosion->GetFormID();
		magic.source = SafeName(explosion);
		magic.explosionForce = explosion->data.force;
		magic.explosionDamage = explosion->data.damage;
	}
	auto* enchantment = data->magicItem ? data->magicItem->As<RE::EnchantmentItem>() : nullptr;
	if (enchantment) {
		magic.enchantmentID = enchantment->GetFormID();
		magic.enchantment = SafeName(enchantment);
	}
	std::lock_guard lock(contextMutex);
	recentMagic[nextMagic % recentMagic.size()] = std::move(magic);
	++nextMagic;
	++magicCount;
}

std::string RogueStaggerCapture::LatestAlert()
{
	std::lock_guard lock(contextMutex);
	return latestAlert;
}
#endif
