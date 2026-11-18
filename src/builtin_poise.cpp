#include "builtin_poise.h"
#include "creature_tier.h"
#include "hook_call.h"
#include "stagger_gate.h"
#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
#include "stagger_diagnostic.h"
#endif
#ifdef ARPGHUD_MAGIC_GAMEPLAY
#include "magic_gameplay.h"
#endif

#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
	bool IsLoadedExecutableAddress(std::uintptr_t address)
	{
		if (!address) {
			return false;
		}

		MEMORY_BASIC_INFORMATION memory{};
		if (VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory) ||
			memory.State != MEM_COMMIT || memory.Type != MEM_IMAGE || !memory.AllocationBase ||
			(memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
			return false;
		}

		switch (memory.Protect & 0xFF) {
		case PAGE_EXECUTE:
		case PAGE_EXECUTE_READ:
		case PAGE_EXECUTE_READWRITE:
		case PAGE_EXECUTE_WRITECOPY:
			break;
		default:
			return false;
		}

		HMODULE owner = nullptr;
		if (!GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(address), &owner)) {
			return false;
		}
		return reinterpret_cast<std::uintptr_t>(owner) ==
			reinterpret_cast<std::uintptr_t>(memory.AllocationBase);
	}

	std::string DescribeAddress(std::uintptr_t address, std::uintptr_t executableBase)
	{
		if (!address) {
			return "null";
		}

		MEMORY_BASIC_INFORMATION memory{};
		if (VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory) ||
			memory.State != MEM_COMMIT) {
			return "unmapped";
		}
		if (memory.Type != MEM_IMAGE || !memory.AllocationBase) {
			return std::format("non-image allocation at 0x{:X}, type=0x{:X}",
				reinterpret_cast<std::uintptr_t>(memory.AllocationBase), memory.Type);
		}

		wchar_t path[MAX_PATH]{};
		const auto moduleHandle = reinterpret_cast<HMODULE>(memory.AllocationBase);
		const auto pathLength = GetModuleFileNameW(moduleHandle, path, static_cast<DWORD>(std::size(path)));
		std::string owner = "image module (path unavailable)";
		if (pathLength > 0 && pathLength < std::size(path)) {
			const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, path, static_cast<int>(pathLength),
				nullptr, 0, nullptr, nullptr);
			if (utf8Length > 0) {
				owner.resize(static_cast<std::size_t>(utf8Length));
				if (WideCharToMultiByte(CP_UTF8, 0, path, static_cast<int>(pathLength),
						owner.data(), utf8Length, nullptr, nullptr) == 0) {
					owner = "image module (path conversion failed)";
				}
			}
		}

		const auto ownerBase = reinterpret_cast<std::uintptr_t>(memory.AllocationBase);
		if (ownerBase == executableBase) {
			return std::format("{}+0x{:X}", owner, address - ownerBase);
		}
		return owner;
	}

	std::mutex stateMutex;
	BuiltinPoise::Settings configuration;
	BuiltinPoise::State state;
	bool installed = false;
	bool inGame = false;
	// Mirrors of the guarded state above so the stagger hooks never take stateMutex.
	std::atomic<bool> gateActive{ false };
	std::atomic<bool> gateSuppress{ false };
	std::atomic<bool> gateNpc{ false };
	std::atomic<bool> gateCreature{ false };
	std::atomic<bool> gateTimedBash{ false };
	std::atomic<bool> gateDebuffs{ false };
	std::atomic<bool> reactiveBarsEnabled{ false };
	// Gameplay-clock time of the last hostile shock hit on the player; far in the past when none.
	constexpr double kNoShock = -1.0e9;
	constexpr double kShockHitSeconds = 1.5;
	double lastShockHit = kNoShock;
	// The bleed keyword list is read by the effect scan without stateMutex; it has its own tiny lock.
	std::mutex keywordMutex;
	std::string bleedKeywords;
	float equipmentRefresh = 0.0F;
	std::uint64_t stateGeneration = 0;
	double gameplaySeconds = 0.0;
	struct NPCState
	{
		RE::ActorHandle handle;
		BuiltinPoise::State poise;
		BuiltinPoise::CreatureTier tier = BuiltinPoise::CreatureTier::Standard;
		float equipmentRefresh = 0.0F;
		double lastUpdate = 0.0;
		// Gameplay-clock time up to which the poise has been advanced; negative until first set.
		double lastAdvance = -1.0;
	};
	std::unordered_map<RE::FormID, NPCState> npcStates;
	struct LastTarget
	{
		RE::FormID id = 0;
		RE::ActorHandle handle;
		std::string name;
		BuiltinPoise::CreatureTier tier = BuiltinPoise::CreatureTier::Standard;
		double lastHit = 0.0;
		// Poise values captured at the last hit (telemetry only), so the overlay card survives the target's death.
		BuiltinPoise::ActorTelemetry view;
		bool hasView = false;
	};
	LastTarget lastTarget;
	constexpr double kTargetBarSeconds = 4.0;
	// While live telemetry is on, the dev overlay keeps the last-hit target visible this long.
	constexpr double kTelemetryTargetSeconds = 15.0;
	std::atomic<bool> telemetryEnabled{ false };
	BuiltinPoise::HitLog hitLog;
	BuiltinPoise::WeaponBenchmark weaponBenchmark;
	constexpr std::size_t kMaxTrackedNPCs = 4096;
	bool npcLimitLogged = false;
	float cleanupTimer = 0.0F;
	float creatureRefreshTimer = 0.0F;
	bool npcInstalled = false;
	// Built once before the hooks go live and read-only afterwards, so lookups need no lock.
	std::unordered_map<RE::FormID, BuiltinPoise::CreatureTier> raceTiers;

	// Requires stateMutex to be held.
	void PublishGate()
	{
		gateActive.store(installed && inGame && configuration.enabled, std::memory_order_relaxed);
		gateSuppress.store(configuration.suppressVanillaStaggers, std::memory_order_relaxed);
		gateNpc.store(npcInstalled && configuration.npcEnabled, std::memory_order_relaxed);
		gateCreature.store(npcInstalled && configuration.enableCreaturePoise, std::memory_order_relaxed);
		gateTimedBash.store(configuration.allowTimedBashStaggers, std::memory_order_relaxed);
		gateDebuffs.store(configuration.enablePoiseDebuffs, std::memory_order_relaxed);
	}

	// Requires stateMutex to be held.
	BuiltinPoise::ActorTelemetry DescribePoise(const BuiltinPoise::State& poise, std::string name, std::string tier)
	{
		BuiltinPoise::ActorTelemetry result;
		const auto fx = BuiltinPoise::ResolveDebuffEffects(configuration, poise.debuffs);
		result.name = std::move(name);
		result.tier = std::move(tier);
		result.current = poise.current;
		result.maximum = poise.maximum;
		result.ceilingRatio = fx.ceilingRatio;
		result.ceilingPoise = poise.maximum * result.ceilingRatio;
		result.regenRate = fx.sapped ? 0.0F : configuration.regenRatePerSecond * poise.regenMultiplier * fx.regenRateMult;
		result.regenDelay = poise.regenDelay;
		result.regenDelayMax = configuration.regenDelaySeconds + fx.delayBonus;
		result.immunity = poise.immunity;
		result.fleshBonus = poise.fleshBonusPoise;
		result.lightArmorBonus = poise.lightArmorBonusPoise;
		result.debuffs = BuiltinPoise::BuildDebuffList(configuration, poise.debuffs);
		return result;
	}

	const char* TierLabel(BuiltinPoise::CreatureTier tier);

	// Requires stateMutex to be held. The poise is the target's state right after the hit, for the telemetry card.
	void NoteTarget(RE::Actor* actor, RE::FormID id, const RE::ActorHandle& handle, BuiltinPoise::CreatureTier tier,
		const BuiltinPoise::State* poise = nullptr)
	{
		lastTarget.id = id;
		lastTarget.handle = handle;
		lastTarget.tier = tier;
		lastTarget.lastHit = gameplaySeconds;
		const char* name = actor ? actor->GetName() : nullptr;
		lastTarget.name = name ? name : "";
		lastTarget.hasView = false;
		if (poise && telemetryEnabled.load(std::memory_order_relaxed)) {
			lastTarget.view = DescribePoise(*poise, lastTarget.name, TierLabel(tier));
			lastTarget.hasView = true;
		}
	}

	const char* TierLabel(BuiltinPoise::CreatureTier tier)
	{
		switch (tier) {
		case BuiltinPoise::CreatureTier::Standard: return "NPC";
		case BuiltinPoise::CreatureTier::Light: return "Light";
		case BuiltinPoise::CreatureTier::Medium: return "Medium";
		case BuiltinPoise::CreatureTier::Heavy: return "Heavy";
		case BuiltinPoise::CreatureTier::Boss: return "Boss";
		default: return "Unknown";
		}
	}

	// Advances an NPC/creature entry by the gameplay time elapsed since it last advanced. The per-actor update
	// hook is not guaranteed to run for every tracked actor, so hits catch up the entry themselves.
	// Requires stateMutex to be held.
	void CatchUp(NPCState& entry)
	{
		if (entry.lastAdvance < 0.0) {
			entry.lastAdvance = gameplaySeconds;
			return;
		}
		const double elapsed = gameplaySeconds - entry.lastAdvance;
		entry.lastAdvance = gameplaySeconds;
		if (elapsed > 0.0) {
			entry.poise.Advance(configuration, static_cast<float>((std::min)(elapsed, 3600.0)));
		}
	}

	using HitFunction = void (*)(RE::Actor*, RE::HitData*);
	using UpdateFunction = void (*)(RE::PlayerCharacter*, float);
	using NPCUpdateFunction = void (*)(RE::Character*, float);
	using StaggerFunction = void (*)(RE::Actor*, float, RE::Actor*);
	HitFunction originalHit = nullptr;
	UpdateFunction originalUpdate = nullptr;
	NPCUpdateFunction originalNPCUpdate = nullptr;
	StaggerFunction directionalStagger = nullptr;

	// Shields are tracked separately and never set a class flag or add Light Armor rating.
	BuiltinPoise::ArmorProfile ScanArmor(RE::Actor* actor)
	{
		BuiltinPoise::ArmorProfile profile;
		float weightTotal = 0.0F;
		float shieldWeightTotal = 0.0F;
		float ratingTotal = 0.0F;
		for (const auto& [item, entry] : actor->GetInventory()) {
			if (!item || !entry.second || !entry.second->IsWorn()) {
				continue;
			}
			auto* armor = item->As<RE::TESObjectARMO>();
			const bool shield = armor && armor->IsShield();
			if (!armor || (!armor->IsLightArmor() && !armor->IsHeavyArmor() && !shield)) {
				continue;
			}
			const float weight = armor->GetWeight();
			if (std::isfinite(weight) && weight > 0.0F) {
				if (shield) {
					shieldWeightTotal += (std::min)(weight, 1000.0F);
				} else {
					weightTotal += (std::min)(weight, 1000.0F);
				}
			}
			if (shield) {
				continue;
			}
			if (armor->IsHeavyArmor()) {
				profile.hasHeavy = true;
			} else {
				profile.hasLight = true;
				float rating = armor->GetArmorRating();
				if (!(rating > 0.0F) && armor->templateArmor) {
					rating = armor->templateArmor->GetArmorRating();
				}
				if (std::isfinite(rating) && rating > 0.0F) {
					ratingTotal += (std::min)(rating, 500.0F);
				}
			}
		}
		profile.weight = (std::min)(weightTotal, 10000.0F);
		profile.shieldWeight = (std::min)(shieldWeightTotal, 10000.0F);
		profile.lightRating = (std::min)(ratingTotal, 2000.0F);
		return profile;
	}

	struct WeaponInfo
	{
		bool armed = false;
		float weight = 0.0F;
		float baseDamage = 0.0F;
		bool twoHanded = false;
		bool ranged = false;
	};

	// The right-hand weapon the telemetry benchmark is based on; staves and empty hands count as unarmed.
	WeaponInfo ScanWeapon(RE::Actor* actor)
	{
		WeaponInfo info;
		auto* object = actor->GetEquippedObject(false);
		const auto* weapon = object ? object->As<RE::TESObjectWEAP>() : nullptr;
		if (!weapon || weapon->GetWeaponType() == RE::WEAPON_TYPE::kStaff) {
			return info;
		}
		const auto type = weapon->GetWeaponType();
		info.armed = true;
		info.weight = weapon->GetWeight();
		info.baseDamage = static_cast<float>(weapon->GetAttackDamage());
		info.twoHanded = type == RE::WEAPON_TYPE::kTwoHandSword || type == RE::WEAPON_TYPE::kTwoHandAxe;
		info.ranged = type == RE::WEAPON_TYPE::kBow || type == RE::WEAPON_TYPE::kCrossbow;
		return info;
	}

	std::string ActorLabel(RE::Actor* actor)
	{
		if (!actor) {
			return "Unknown";
		}
		if (actor->IsPlayerRef()) {
			return "Player";
		}
		const char* name = actor->GetName();
		return name && *name ? name : "Unknown";
	}

	// Requires stateMutex to be held.
	void RecordHit(bool outgoing, std::string attacker, std::string target, const BuiltinPoise::HitBreakdown& breakdown,
		const BuiltinPoise::HitResult& result)
	{
		BuiltinPoise::HitRecord record;
		record.outgoing = outgoing;
		record.attacker = std::move(attacker);
		record.target = std::move(target);
		record.type = breakdown.typeLabel;
		record.baseDamage = breakdown.baseDamage;
		record.rawBonus = breakdown.rawBonus;
		record.rawCapped = breakdown.rawCapped;
		for (std::size_t i = 0; i < breakdown.tagCount; ++i) {
			record.tags.emplace_back(breakdown.tags[i].label, breakdown.tags[i].factor);
		}
		record.finalDamage = breakdown.finalDamage;
		record.broke = result.broke;
		record.immune = result.absorbed;
		hitLog.Push(std::move(record));
	}

	// Commanded actors (summons, reanimated and other followers) get a poise bonus.
	float SummonScale(const BuiltinPoise::Settings& settings, RE::Actor* actor)
	{
		return actor && !actor->IsPlayerRef() && actor->IsCommandedActor() ? settings.summonPoiseMultiplier : 1.0F;
	}

	enum class BlockKind { none, shield, weapon };

	// What the actor is actively blocking with; a shield wins over a held weapon.
	BlockKind CurrentBlock(RE::Actor* actor)
	{
		if (!actor || !actor->IsBlocking()) {
			return BlockKind::none;
		}
		auto kind = BlockKind::none;
		for (const bool left : { true, false }) {
			auto* object = actor->GetEquippedObject(left);
			const auto* shield = object ? object->As<RE::TESObjectARMO>() : nullptr;
			const auto* weapon = object ? object->As<RE::TESObjectWEAP>() : nullptr;
			if (shield && shield->IsShield()) {
				return BlockKind::shield;
			}
			if (weapon && weapon->GetWeaponType() != RE::WEAPON_TYPE::kStaff) {
				kind = BlockKind::weapon;
			}
		}
		return kind;
	}

	float BlockedMagicScale(const BuiltinPoise::Settings& settings, BlockKind kind)
	{
		switch (kind) {
		case BlockKind::shield: return settings.shieldBlockedMagicMultiplier;
		case BlockKind::weapon: return settings.weaponBlockedMagicMultiplier;
		default: return 1.0F;
		}
	}

	using BuiltinPoise::FleshRating;

#ifdef ARPGHUD_MAGIC_GAMEPLAY
	FleshRating ActiveFlesh(RE::Actor* actor)
	{
		FleshRating result;
		auto* target = actor->GetMagicTarget();
		auto* effects = target ? target->GetActiveEffectList() : nullptr;
		if (!effects) {
			return result;
		}
		for (const auto* active : *effects) {
			if (!active || active->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled) ||
				active->conditionStatus == RE::ActiveEffect::ConditionStatus::kFalse || !active->effect ||
				!active->effect->baseEffect) {
				continue;
			}
			const auto* effect = active->effect->baseEffect;
			if (active->spell && active->spell->GetFormID() == 0xCDB70 &&
				effect->GetFormID() == 0xCDB75) {
				result.dragonhide = true;
			} else if (effect->data.associatedSkill == RE::ActorValue::kAlteration &&
				effect->data.primaryAV == RE::ActorValue::kDamageResist &&
				(effect->data.archetype == RE::EffectArchetype::kValueModifier ||
				 effect->data.archetype == RE::EffectArchetype::kPeakValueModifier) &&
				std::isfinite(active->magnitude) && active->magnitude > 0.0F) {
				result.armor += active->magnitude;
			}
		}
		return result;
	}
#else
	// Flesh spells only affect poise when the magic gameplay hooks are built in.
	FleshRating ActiveFlesh(RE::Actor*)
	{
		return {};
	}
#endif

	BuiltinPoise::MaximumBreakdown MaximumPoise(const BuiltinPoise::Settings& settings, BuiltinPoise::CreatureTier tier,
		const BuiltinPoise::ArmorProfile& armor, const FleshRating& flesh, RE::Actor* actor)
	{
		return BuiltinPoise::ComputeMaximumPoise(settings, tier, armor, flesh, SummonScale(settings, actor), !actor || actor->IsPlayerRef());
	}

	// Reads hostile Sapped, Enfeebled, Numbed, Hemorrhaging and Scorched sources from the active effect list, mirroring
	// ActiveFlesh. Skipped entirely while the master switch is off, except for the player while the reactive bar
	// effects need the raw result.
	BuiltinPoise::PoiseDebuffs ScanDebuffs(RE::Actor* actor)
	{
		BuiltinPoise::PoiseDebuffs result;
		if (!actor || (!gateDebuffs.load(std::memory_order_relaxed) &&
				!(reactiveBarsEnabled.load(std::memory_order_relaxed) && actor->IsPlayerRef()))) {
			return result;
		}
		auto* target = actor->GetMagicTarget();
		auto* effects = target ? target->GetActiveEffectList() : nullptr;
		if (!effects) {
			return result;
		}
		for (const auto* active : *effects) {
			if (!active || active->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled) ||
				active->conditionStatus == RE::ActiveEffect::ConditionStatus::kFalse || !active->effect ||
				!active->effect->baseEffect) {
				continue;
			}
			const auto* effect = active->effect->baseEffect;
			const char* editorID = effect->GetFormEditorID();
			const char* effectName = effect->GetFullName();
			const char* spellName = active->spell ? active->spell->GetName() : nullptr;
			BuiltinPoise::EffectFacts facts;
			if (const auto* file = effect->GetFile(0)) {
				facts.plugin = file->GetFilename();
				facts.localFormID = effect->GetLocalFormID();
			}
			{
				std::lock_guard lock(keywordMutex);
				for (const char* text : { editorID, effectName, spellName }) {
					if (text && *text && BuiltinPoise::MatchesKeywordList(bleedKeywords, text)) {
						facts.bleedMatch = true;
						break;
					}
				}
			}
			// Vanilla bleed (PerkBleedingDamage) is flagged detrimental only, so bleed effects are also accepted on that
			// flag. A null caster is fine; only an effect the actor cast on itself is ignored.
			const bool bleedCandidate = facts.bleedMatch || BuiltinPoise::IsVanillaBleedEffect(facts.plugin, facts.localFormID);
			const bool spellHostile = active->spell && active->spell->IsHostile();
			facts.hostile = BuiltinPoise::EffectIsHostile(effect->IsHostile(), spellHostile, effect->IsDetrimental(),
				bleedCandidate);
			if (!facts.hostile) {
				continue;
			}
			const auto caster = active->caster.get();
			facts.selfCast = caster && caster.get() == actor;
			facts.kind = effect->data.archetype == RE::EffectArchetype::kAbsorb ?
				BuiltinPoise::EffectKind::absorb : BuiltinPoise::EffectKind::other;
			switch (effect->data.resistVariable) {
			case RE::ActorValue::kPoisonResist: facts.resist = BuiltinPoise::EffectResist::poison; break;
			case RE::ActorValue::kResistFire: facts.resist = BuiltinPoise::EffectResist::fire; break;
			case RE::ActorValue::kResistFrost: facts.resist = BuiltinPoise::EffectResist::frost; break;
			case RE::ActorValue::kNone: break;
			default: facts.resist = BuiltinPoise::EffectResist::other; break;
			}
			facts.healthDamage = effect->data.primaryAV == RE::ActorValue::kHealth ||
				(effect->data.archetype == RE::EffectArchetype::kDualValueModifier &&
				 effect->data.secondaryAV == RE::ActorValue::kHealth);
			facts.harmfulPoisonKeyword = effect->HasKeywordString("MagicAlchHarmful");
			facts.detrimental = effect->IsDetrimental();
			facts.duration = active->duration;
			facts.magnitude = active->magnitude;
			const char* label = effectName && *effectName ? effectName : spellName && *spellName ? spellName :
				editorID && *editorID ? editorID : "Unknown effect";
			// Dev aid: while telemetry is on, log each distinct hostile effect seen on an NPC once, with the facts the
			// classifier used, so a missing debuff can be told apart from a missing scan.
			if (!actor->IsPlayerRef() && telemetryEnabled.load(std::memory_order_relaxed)) {
				static std::unordered_set<std::uint64_t> logged;
				const auto key = (static_cast<std::uint64_t>(actor->GetFormID()) << 32) | effect->GetFormID();
				std::lock_guard lock(keywordMutex);
				if (logged.size() < 200 && logged.insert(key).second) {
					BuiltinPoise::PoiseDebuffs probe;
					BuiltinPoise::AccumulateDebuff(probe, facts, label);
					logger::info("Poise debuff scan: actor={:08X} effect='{}' form={}:{:06X} archetype={} resist={} effectHostile={} spellHostile={} detrimental={} bleedCandidate={} duration={} magnitude={} healthDamage={} selfCast={} -> sapped={} enfeebled={} numbed={} hemorrhaging={} scorched={}.",
						actor->GetFormID(), label, facts.plugin, facts.localFormID,
						static_cast<int>(effect->data.archetype), static_cast<int>(effect->data.resistVariable),
						effect->IsHostile(), spellHostile, effect->IsDetrimental(), bleedCandidate, facts.duration,
						facts.magnitude, facts.healthDamage, facts.selfCast,
						probe.Has(BuiltinPoise::DebuffId::sapped), probe.Has(BuiltinPoise::DebuffId::enfeebled),
						probe.Has(BuiltinPoise::DebuffId::numbed), probe.Has(BuiltinPoise::DebuffId::hemorrhaging),
						probe.Has(BuiltinPoise::DebuffId::scorched));
				}
			}
			BuiltinPoise::AccumulateDebuff(result, facts, label);
		}
		return result;
	}

	void BuildRaceTable()
	{
		raceTiers.clear();
		auto* handler = RE::TESDataHandler::GetSingleton();
		if (!handler) {
			logger::warn("Creature poise race table not built: data handler unavailable; creatures fall back to the unmodified tier.");
			return;
		}
		std::array<std::size_t, 7> counts{};
		std::size_t missingIDs = 0;
		for (auto* race : handler->GetFormArray<RE::TESRace>()) {
			if (!race) {
				continue;
			}
			const char* editorID = race->GetFormEditorID();
			if (!editorID || !*editorID) {
				++missingIDs;
			}
			BuiltinPoise::RaceTraits traits;
			traits.npc = race->HasKeywordString("ActorTypeNPC");
			traits.dragon = race->HasKeywordString("ActorTypeDragon");
			traits.giant = race->HasKeywordString("ActorTypeGiant");
			const auto tier = BuiltinPoise::ClassifyRace(editorID ? editorID : "", traits);
			raceTiers.emplace(race->GetFormID(), tier);
			++counts[static_cast<std::size_t>(tier)];
		}
		logger::info("Creature poise race table built: {} races (standard={}, excluded={}, unknown={}, light={}, medium={}, heavy={}, boss={}, without editor ID={}).",
			raceTiers.size(), counts[0], counts[1], counts[2], counts[3], counts[4], counts[5], counts[6], missingIDs);
	}

	// Race, NPC keyword and display name only; takes no locks and ignores live state such as flying or death.
	BuiltinPoise::CreatureTier ClassifyActor(RE::Actor* actor)
	{
		const bool npc = actor->HasKeywordString("ActorTypeNPC");
		auto raceTier = npc ? BuiltinPoise::CreatureTier::Standard : BuiltinPoise::CreatureTier::Unknown;
		if (const auto* race = actor->GetRace()) {
			if (const auto it = raceTiers.find(race->GetFormID()); it != raceTiers.end()) {
				raceTier = it->second;
			}
		}
		return BuiltinPoise::ResolveActorTier(raceTier, npc, actor->GetName() ? actor->GetName() : "",
			gateCreature.load(std::memory_order_relaxed));
	}

	// Nullopt when the actor is not managed by poise right now (player, downed, ambient critter, airborne creature).
	std::optional<BuiltinPoise::CreatureTier> TrackedTier(RE::Actor* actor)
	{
		if (!actor || actor->IsPlayerRef() || !actor->Is3DLoaded() ||
			actor->IsDead() || actor->AsActorState()->IsBleedingOut() || actor->IsInKillMove()) {
			return std::nullopt;
		}
		const auto tier = ClassifyActor(actor);
		if (tier == BuiltinPoise::CreatureTier::Excluded ||
			(BuiltinPoise::IsCreatureTier(tier) && actor->AsActorState()->IsFlying())) {
			return std::nullopt;
		}
		return tier;
	}

	const char* TierName(BuiltinPoise::CreatureTier tier)
	{
		switch (tier) {
		case BuiltinPoise::CreatureTier::Standard: return "standard";
		case BuiltinPoise::CreatureTier::Excluded: return "excluded";
		case BuiltinPoise::CreatureTier::Unknown: return "unknown";
		case BuiltinPoise::CreatureTier::Light: return "light";
		case BuiltinPoise::CreatureTier::Medium: return "medium";
		case BuiltinPoise::CreatureTier::Heavy: return "heavy";
		case BuiltinPoise::CreatureTier::Boss: return "boss";
		}
		return "?";
	}

	void CleanupNPCs()
	{
		std::vector<std::pair<RE::FormID, RE::ActorHandle>> snapshot;
		double now = 0.0;
		{
			std::lock_guard lock(stateMutex);
			now = gameplaySeconds;
			for (const auto& [id, entry] : npcStates) {
				snapshot.emplace_back(id, entry.handle);
			}
		}
		for (const auto& [id, handle] : snapshot) {
			auto actor = handle.get();
			const bool invalid = !actor || !TrackedTier(actor.get());
			std::lock_guard lock(stateMutex);
			const auto it = npcStates.find(id);
			if (it != npcStates.end() && it->second.handle == handle &&
				(invalid || now - it->second.lastUpdate > 60.0)) {
				npcStates.erase(it);
			}
		}
		std::lock_guard lock(stateMutex);
		if (npcStates.size() < kMaxTrackedNPCs) {
			npcLimitLogged = false;
		}
	}

	void CharacterUpdate(RE::Character* actor, float delta)
	{
		originalNPCUpdate(actor, delta);
		if (!actor || !std::isfinite(delta) || delta <= 0.0F) {
			return;
		}
		const auto id = actor->GetFormID();
		{
			std::lock_guard lock(stateMutex);
			if (!npcStates.contains(id)) {
				return;
			}
		}
		const RE::ActorHandle handle(actor);
		const auto tier = TrackedTier(actor);
		auto* ui = RE::UI::GetSingleton();
		if (!ui || ui->GameIsPaused()) {
			return;
		}
		bool refresh = false;
		{
			std::lock_guard lock(stateMutex);
			auto it = npcStates.find(id);
			if (it == npcStates.end()) {
				return;
			}
			if (!installed || !inGame || !configuration.enabled || !tier ||
				!BuiltinPoise::TierEnabled(configuration, *tier) ||
				it->second.tier != *tier || it->second.handle != handle) {
				npcStates.erase(it);
				return;
			}
			it->second.lastUpdate = gameplaySeconds;
			delta = (std::min)(delta, 0.25F);
			it->second.equipmentRefresh -= delta;
			refresh = it->second.equipmentRefresh <= 0.0F;
			if (refresh) {
				it->second.equipmentRefresh = 0.5F;
			}
		}
		const auto armor = refresh ? ScanArmor(actor) : BuiltinPoise::ArmorProfile{};
		const auto flesh = refresh ? ActiveFlesh(actor) : FleshRating{};
		const auto debuffs = refresh ? ScanDebuffs(actor) : BuiltinPoise::PoiseDebuffs{};
		std::lock_guard lock(stateMutex);
		auto it = npcStates.find(id);
		if (it == npcStates.end() || it->second.handle != handle || !inGame ||
			!configuration.enabled || !BuiltinPoise::TierEnabled(configuration, *tier)) {
			return;
		}
		if (refresh) {
			it->second.poise.SetMaximum(MaximumPoise(configuration, *tier, armor, flesh, actor));
			it->second.poise.SetDebuffs(debuffs);
			it->second.poise.ApplyTier(configuration, *tier);
		}
		it->second.lastUpdate = gameplaySeconds;
		CatchUp(it->second);
#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
		static bool tickLogged = false;
		if (!tickLogged && BuiltinPoise::IsCreatureTier(*tier)) {
			tickLogged = true;
			logger::info("Creature poise: Character update hook is ticking tracked creature {:08X} (tier {}).", id, TierName(*tier));
		}
#endif
	}

	bool Active()
	{
		std::lock_guard lock(stateMutex);
		return installed && inGame && configuration.enabled;
	}

	// Creatures are not reliably ticked by the per-actor update hook, so their maximum poise (armor, flesh spells,
	// summon status) is refreshed and untrackable entries (airborne, dead, retiered) are dropped from the player's update.
	void RefreshCreatureEntries()
	{
		struct Item
		{
			RE::FormID id;
			RE::ActorHandle handle;
			BuiltinPoise::CreatureTier tier;
		};
		std::vector<Item> items;
		{
			std::lock_guard lock(stateMutex);
			for (const auto& [id, entry] : npcStates) {
				if (BuiltinPoise::IsCreatureTier(entry.tier)) {
					items.push_back({ id, entry.handle, entry.tier });
				}
			}
		}
		for (const auto& item : items) {
			auto actor = item.handle.get();
			const auto tier = actor ? TrackedTier(actor.get()) : std::nullopt;
			const bool valid = tier && *tier == item.tier;
			const auto armor = valid ? ScanArmor(actor.get()) : BuiltinPoise::ArmorProfile{};
			const auto flesh = valid ? ActiveFlesh(actor.get()) : FleshRating{};
			const auto debuffs = valid ? ScanDebuffs(actor.get()) : BuiltinPoise::PoiseDebuffs{};
			std::lock_guard lock(stateMutex);
			const auto it = npcStates.find(item.id);
			if (it == npcStates.end() || it->second.handle != item.handle) {
				continue;
			}
			if (!valid || !installed || !inGame || !configuration.enabled ||
				!BuiltinPoise::TierEnabled(configuration, item.tier)) {
				npcStates.erase(it);
				continue;
			}
			it->second.poise.SetMaximum(MaximumPoise(configuration, item.tier, armor, flesh, actor.get()));
			it->second.poise.SetDebuffs(debuffs);
			it->second.poise.ApplyTier(configuration, item.tier);
			CatchUp(it->second);
		}
	}

	void PlayerUpdate(RE::PlayerCharacter* player, float delta)
	{
		originalUpdate(player, delta);
		if (!player || !Active()) {
			return;
		}
		if (player->IsDead()) {
			std::lock_guard lock(stateMutex);
			if (state.maximum > 0.0F) {
				state.Reset();
				++stateGeneration;
			}
			return;
		}
		if (player->IsInKillMove()) {
			return;
		}
		auto* ui = RE::UI::GetSingleton();
		if (!ui || ui->GameIsPaused() || !std::isfinite(delta) || delta <= 0.0F) {
			return;
		}
		delta = (std::min)(delta, 0.25F);
		bool refresh = false;
		bool cleanup = false;
		bool creatureRefresh = false;
		{
			std::lock_guard lock(stateMutex);
			gameplaySeconds += delta;
			cleanupTimer -= delta;
			if (cleanupTimer <= 0.0F) {
				cleanupTimer = 10.0F;
				cleanup = true;
			}
			creatureRefreshTimer -= delta;
			if (creatureRefreshTimer <= 0.0F) {
				creatureRefreshTimer = 0.5F;
				creatureRefresh = true;
			}
			equipmentRefresh -= delta;
			if (equipmentRefresh <= 0.0F || state.maximum <= 0.0F) {
				equipmentRefresh = 0.5F;
				refresh = true;
			}
		}
		if (cleanup) {
			CleanupNPCs();
		}
		if (creatureRefresh) {
			RefreshCreatureEntries();
		}
#ifdef ARPGHUD_MAGIC_GAMEPLAY
		MagicGameplay::ScheduleAdvance(delta);
#endif
		const auto armor = refresh ? ScanArmor(player) : BuiltinPoise::ArmorProfile{};
		const auto flesh = refresh ? ActiveFlesh(player) : FleshRating{};
		const auto debuffs = refresh ? ScanDebuffs(player) : BuiltinPoise::PoiseDebuffs{};
		const bool telemetry = refresh && telemetryEnabled.load(std::memory_order_relaxed);
		const auto weapon = telemetry ? ScanWeapon(player) : WeaponInfo{};
		std::lock_guard lock(stateMutex);
		if (!installed || !inGame || !configuration.enabled) {
			return;
		}
		if (refresh) {
			state.SetMaximum(MaximumPoise(configuration, BuiltinPoise::CreatureTier::Standard, armor, flesh, nullptr));
			state.SetDebuffs(debuffs);
		}
		if (telemetry) {
			weaponBenchmark = BuiltinPoise::ComputeWeaponBenchmark(configuration, weapon.armed, weapon.weight,
				weapon.baseDamage, weapon.twoHanded, weapon.ranged);
		}
		state.Advance(configuration, delta);
	}

	bool ClassifyHit(RE::Actor* target, RE::Actor* aggressor, RE::HitData* data, BuiltinPoise::Hit& hit)
	{
		if (!data || data->flags.any(RE::HitData::Flag::kPredictDamage) ||
			data->flags.any(RE::HitData::Flag::kExplosion)) {
			return false;
		}
		auto* weapon = data->weapon;
		const auto source = data->sourceRef.get();
		auto* projectile = source ? source->AsProjectile() : nullptr;
		if (source && !projectile) {
			return false;
		}
		if (projectile) {
			const auto& projectileData = projectile->GetProjectileRuntimeData();
			if (!projectileData.ammoSource || !projectileData.weaponSource) {
				return false;
			}
			weapon = projectileData.weaponSource;
			hit.type = BuiltinPoise::HitType::projectile;
		} else if (!aggressor->HasKeywordString("ActorTypeNPC")) {
			// Creatures usually carry a race unarmed weapon in HitData, so classify by attacker rather than weapon.
			hit.type = BuiltinPoise::HitType::creature;
			weapon = nullptr;
		} else if (data->flags.any(RE::HitData::Flag::kBash)) {
			hit.type = BuiltinPoise::HitType::bash;
			// Bashing with a shield equipped uses the shield; otherwise it is a weapon pommel bash.
			const auto* shield = [&]() -> const RE::TESObjectARMO* {
				for (const bool left : { true, false }) {
					auto* object = aggressor->GetEquippedObject(left);
					const auto* armor = object ? object->As<RE::TESObjectARMO>() : nullptr;
					if (armor && armor->IsShield()) {
						return armor;
					}
				}
				return nullptr;
			}();
			hit.shieldBash = shield != nullptr;
			if (!weapon) {
				if (shield) {
					hit.weaponWeight = shield->GetWeight();
				}
				if (hit.weaponWeight <= 0.0F) {
					auto* right = aggressor->GetEquippedObject(false);
					weapon = right ? right->As<RE::TESObjectWEAP>() : nullptr;
				}
			}
		} else if (weapon) {
			if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kStaff) {
				return false;
			}
			hit.type = BuiltinPoise::HitType::melee;
		} else if (data->skill == RE::ActorValue::kNone) {
			hit.type = BuiltinPoise::HitType::creature;
		} else {
			return false;
		}
		if (weapon) {
			hit.weaponWeight = weapon->GetWeight();
			const auto type = weapon->GetWeaponType();
			hit.twoHanded = type == RE::WEAPON_TYPE::kTwoHandSword || type == RE::WEAPON_TYPE::kTwoHandAxe;
		}
		hit.powerAttack = data->flags.any(RE::HitData::Flag::kPowerAttack);
		hit.physicalDamage = data->physicalDamage;
		hit.attackerTier = hit.type == BuiltinPoise::HitType::creature ? ClassifyActor(aggressor) :
			BuiltinPoise::CreatureTier::Standard;
		hit.blocked = false;
		hit.shieldBlocked = false;
		if (data->flags.any(RE::HitData::Flag::kBlocked)) {
			for (const bool left : { true, false }) {
				auto* object = target->GetEquippedObject(left);
				const auto* shield = object ? object->As<RE::TESObjectARMO>() : nullptr;
				const auto* heldWeapon = object ? object->As<RE::TESObjectWEAP>() : nullptr;
				if (shield && shield->IsShield()) {
					hit.blocked = true;
					hit.shieldBlocked = true;
					break;
				}
				if (heldWeapon && heldWeapon->GetWeaponType() != RE::WEAPON_TYPE::kStaff) {
					hit.blocked = true;
				}
			}
		}
		return true;
	}

	void ProcessHit(RE::Actor* target, RE::HitData* data)
	{
		BuiltinPoise::HitResult result;
		RE::NiPointer<RE::Actor> aggressor;
		std::uint64_t generation = 0;
		RE::ActorHandle targetHandle;
		RE::FormID targetID = 0;
		const bool player = target && target->IsPlayerRef();
		const auto trackedTier = player ? std::optional<BuiltinPoise::CreatureTier>{} : TrackedTier(target);
		const bool eligible = player ? !target->IsDead() && !target->AsActorState()->IsBleedingOut() &&
			!target->IsInKillMove() && target->Is3DLoaded() : trackedTier.has_value();
		const auto tier = trackedTier.value_or(BuiltinPoise::CreatureTier::Standard);
#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
		StaggerDiagnostic::HitSample sample;
		RE::NiPointer<RE::Actor> sampleAggressor = data ? data->aggressor.get() : RE::NiPointer<RE::Actor>{};
		sample.target = target;
		sample.aggressor = sampleAggressor.get();
		sample.data = data;
		sample.incomingStagger = data ? data->stagger : 0.0F;
		sample.eligible = eligible;
		StaggerDiagnostic::OnDamageSample(sample);
#endif
		if (eligible && data && Active()) {
			aggressor = data->aggressor.get();
			BuiltinPoise::Hit hit;
			if (aggressor && aggressor.get() != target && ClassifyHit(target, aggressor.get(), data, hit)) {
#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
				sample.classified = true;
				sample.hitType = static_cast<int>(hit.type);
#endif
				const auto id = target->GetFormID();
				targetID = id;
				targetHandle = RE::ActorHandle(target);
				bool needsArmor = false;
				if (!player) {
					std::lock_guard lock(stateMutex);
					const auto it = npcStates.find(id);
					needsArmor = it == npcStates.end() || it->second.handle != targetHandle ||
						it->second.tier != tier || it->second.poise.maximum <= 0.0F;
				}
				const auto armor = needsArmor ? ScanArmor(target) : BuiltinPoise::ArmorProfile{};
				const auto flesh = needsArmor ? ActiveFlesh(target) : FleshRating{};
				const auto debuffs = ScanDebuffs(target);
				const bool recordHit = telemetryEnabled.load(std::memory_order_relaxed) && (player || aggressor->IsPlayerRef());
				BuiltinPoise::HitBreakdown breakdown;
				BuiltinPoise::HitBreakdown* breakdownOut = recordHit ? &breakdown : nullptr;
				const BuiltinPoise::State* hitPoise = nullptr;
				std::string attackerLabel;
				std::string targetLabel;
				if (recordHit) {
					attackerLabel = ActorLabel(aggressor.get());
					targetLabel = ActorLabel(target);
				}
				std::lock_guard lock(stateMutex);
				if (installed && inGame) {
					if (player) {
						state.SetDebuffs(debuffs);
#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
						sample.poiseBefore = state.current;
						sample.poiseMax = state.maximum;
#endif
						result = state.Apply(configuration, hit, breakdownOut);
#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
						sample.poiseAfter = state.current;
#endif
					} else if (npcInstalled && BuiltinPoise::TierEnabled(configuration, tier) && targetHandle && needsArmor &&
						(npcStates.contains(id) || npcStates.size() < kMaxTrackedNPCs)) {
						auto& entry = npcStates[id];
						if (entry.handle != targetHandle || entry.tier != tier) {
							entry = {};
							entry.handle = targetHandle;
							entry.tier = tier;
						}
						if (entry.poise.maximum <= 0.0F) {
							entry.poise.SetMaximum(MaximumPoise(configuration, tier, armor, flesh, target));
							entry.poise.ApplyTier(configuration, tier);
						}
						hitPoise = &entry.poise;
						entry.poise.SetDebuffs(debuffs);
						entry.lastUpdate = gameplaySeconds;
						CatchUp(entry);
#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
						sample.poiseBefore = entry.poise.current;
						sample.poiseMax = entry.poise.maximum;
#endif
						result = entry.poise.Apply(configuration, hit, breakdownOut);
#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
						sample.poiseAfter = entry.poise.current;
#endif
					} else if (npcInstalled && BuiltinPoise::TierEnabled(configuration, tier) && targetHandle) {
						auto it = npcStates.find(id);
						if (it != npcStates.end() && it->second.handle == targetHandle && it->second.tier == tier) {
							hitPoise = &it->second.poise;
							it->second.poise.SetDebuffs(debuffs);
							it->second.lastUpdate = gameplaySeconds;
							CatchUp(it->second);
#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
							sample.poiseBefore = it->second.poise.current;
							sample.poiseMax = it->second.poise.maximum;
#endif
							result = it->second.poise.Apply(configuration, hit, breakdownOut);
#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
							sample.poiseAfter = it->second.poise.current;
#endif
						} else if (needsArmor && npcStates.size() >= kMaxTrackedNPCs && !npcLimitLogged) {
							logger::warn("NPC poise tracking limit reached; additional actors retain vanilla reactions until space is available.");
							npcLimitLogged = true;
						}
					}
					generation = stateGeneration;
					if (result.handled) {
						data->stagger = 0.0F;
						if (!player) {
							NoteTarget(target, id, targetHandle, tier, hitPoise);
						}
						if (recordHit && breakdown.tagCount > 0) {
							RecordHit(!player, std::move(attackerLabel), std::move(targetLabel), breakdown, result);
						}
					}
				}
			}
		}
		{
			std::optional<StaggerGate::ScopedTimedBash> timedBash;
			if (data && data->flags.any(RE::HitData::Flag::kTimedBash)) {
				timedBash.emplace();
			}
			originalHit(target, data);
		}
#ifdef ARPGHUD_STAGGER_DIAGNOSTICS
		sample.handled = result.handled;
		sample.broke = result.broke;
		if (player || BuiltinPoise::IsCreatureTier(tier)) {
			StaggerDiagnostic::OnHit(sample);
		}
#endif
		bool stillActive = false;
		if (result.broke) {
			std::lock_guard lock(stateMutex);
			stillActive = installed && inGame && configuration.enabled && generation == stateGeneration;
			if (!player && stillActive) {
				const auto it = npcStates.find(targetID);
				stillActive = BuiltinPoise::TierEnabled(configuration, tier) && it != npcStates.end() &&
					it->second.handle == targetHandle && it->second.tier == tier;
			}
		}
		if (stillActive && target && !target->IsDead() && !target->AsActorState()->IsBleedingOut() &&
			!target->IsInKillMove() && target->Is3DLoaded()) {
			StaggerGate::ScopedAllowStagger allowStagger;
			directionalStagger(target, result.magnitude, aggressor.get());
		}
	}
}

void BuiltinPoise::Install()
{
	const auto& module = REL::Module::get();
	if (module.version() != SKSE::RUNTIME_SSE_1_7_104) {
		logger::error("Built-in poise HitData hook and directional stagger routine not validated or installed: runtime {} is not verified; only Skyrim 1.7.104.0 is supported.", module.version().string());
		return;
	}
	if (GetModuleHandleW(L"ChocolatePoiseReforged.dll") || GetModuleHandleW(L"ChocolatePoise.dll")) {
		logger::warn("Built-in poise HitData hook and directional stagger routine not validated or installed: Chocolate Poise is loaded; preserving its behavior.");
		return;
	}
	const auto hitSite = REL::RelocationID(37673, 38627).address() + 0x4A8;
	const auto staggerSite = REL::RelocationID(36700, 37710).address();
	// The table must be complete before any hook can run, because the hooks read it without a lock.
	BuildRaceTable();
	REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_PlayerCharacter[0] };
	REL::Relocation<std::uintptr_t> npcVtable{ RE::VTABLE_Character[0] };
	const auto npcUpdateAddress = *reinterpret_cast<const std::uintptr_t*>(npcVtable.address() + 0xAD * sizeof(std::uintptr_t));
	const auto updateAddress = *reinterpret_cast<const std::uintptr_t*>(vtable.address() + 0xAD * sizeof(std::uintptr_t));
	const auto expectedPlayerUpdate = module.base() + 0x745200;
	const auto expectedNPCUpdate = module.base() + 0x67A7E0;
	constexpr std::array<std::uint8_t, 8> hitBytes{
		0x48, 0x8D, 0x54, 0x24, 0x50, 0x48, 0x8B, 0xCF
	};
	constexpr std::array<std::uint8_t, 11> staggerBytes{
		0x48, 0x8B, 0xC4, 0x57, 0x48, 0x81, 0xEC, 0xC0, 0x00, 0x00, 0x00
	};
	constexpr std::array<std::uint8_t, 10> hitStaggerBytes{
		0xF3, 0x41, 0x0F, 0x10, 0x45, 0x68, 0x41, 0x0F, 0x2F, 0xC1
	};
	constexpr std::array<std::uint8_t, 5> hitStaggerCall{ 0xE8, 0x5E, 0x4B, 0xFD, 0xFF };
	const auto hitHandler = module.base() + 0x6CA640;
	const auto previousHit = HookCall::ExecutableTarget(hitSite);
	const bool hitValid = std::memcmp(reinterpret_cast<const void*>(hitSite - 8), hitBytes.data(), hitBytes.size()) == 0 &&
		previousHit != 0 && previousHit != reinterpret_cast<std::uintptr_t>(&ProcessHit) &&
		std::memcmp(reinterpret_cast<const void*>(hitHandler + 0x9E5), hitStaggerBytes.data(), hitStaggerBytes.size()) == 0 &&
		std::memcmp(reinterpret_cast<const void*>(hitHandler + 0xA5D), hitStaggerCall.data(), hitStaggerCall.size()) == 0;
	const bool staggerValid = std::memcmp(reinterpret_cast<const void*>(staggerSite), staggerBytes.data(), staggerBytes.size()) == 0;
	const bool updateValid = updateAddress != reinterpret_cast<std::uintptr_t>(&PlayerUpdate) &&
		IsLoadedExecutableAddress(updateAddress);
	const bool npcUpdateValid = npcUpdateAddress != reinterpret_cast<std::uintptr_t>(&CharacterUpdate) &&
		IsLoadedExecutableAddress(npcUpdateAddress);
	logger::info("Built-in poise HitData call validation: {}.", hitValid ? "passed" : "FAILED");
	logger::info("Built-in poise directional stagger routine validation: {}.", staggerValid ? "passed" : "FAILED");
	logger::info("Built-in poise player update slot: vtable=0x{:X}, slot=0xAD, actual=0x{:X} [{}], executable-module validation {}.",
		vtable.address(), updateAddress, DescribeAddress(updateAddress, module.base()), updateValid ? "passed" : "FAILED");
	logger::info("Built-in poise NPC update slot: vtable=0x{:X}, slot=0xAD, actual=0x{:X} [{}], executable-module validation {}.",
		npcVtable.address(), npcUpdateAddress, DescribeAddress(npcUpdateAddress, module.base()),
		npcUpdateValid ? "passed" : "FAILED; NPC poise disabled");
	logger::info("Built-in poise downed-actor check: using runtime-adjusted ActorState life state.");
	if (!hitValid || !staggerValid || !updateValid) {
		logger::error("Built-in poise hooks not installed; preserving vanilla behavior and using the optional bridge.");
		return;
	}
	if (previousHit == hitHandler) {
		logger::info("Built-in poise HitData call targets the original vanilla handler.");
	} else {
		logger::warn("Built-in poise will chain existing executable HitData call destination {:X}; ABI and mod gameplay compatibility are not verified.", previousHit);
	}
	directionalStagger = reinterpret_cast<StaggerFunction>(staggerSite);
	originalHit = reinterpret_cast<HitFunction>(previousHit);
	originalUpdate = reinterpret_cast<UpdateFunction>(updateAddress);
	if (updateAddress != expectedPlayerUpdate) {
		logger::info("Built-in poise chaining existing player update destination 0x{:X} [{}].",
			updateAddress, DescribeAddress(updateAddress, module.base()));
	}
	SKSE::GetTrampoline().write_call<5>(hitSite, ProcessHit);
	vtable.write_vfunc(0xAD, PlayerUpdate);
	if (npcUpdateValid) {
		originalNPCUpdate = reinterpret_cast<NPCUpdateFunction>(npcUpdateAddress);
		if (npcUpdateAddress != expectedNPCUpdate) {
			logger::info("Built-in poise chaining existing NPC update destination 0x{:X} [{}].",
				npcUpdateAddress, DescribeAddress(npcUpdateAddress, module.base()));
		}
		npcVtable.write_vfunc(0xAD, CharacterUpdate);
		npcInstalled = *reinterpret_cast<const std::uintptr_t*>(npcVtable.address() + 0xAD * sizeof(std::uintptr_t)) ==
			reinterpret_cast<std::uintptr_t>(&CharacterUpdate);
		logger::info("Built-in poise NPC update hook installation: {}.", npcInstalled ? "passed" : "FAILED");
	}
#ifndef ARPGHUD_MAGIC_GAMEPLAY
	logger::warn("Built-in poise elemental magic and ward interception are not installed; vanilla magic behavior is unchanged.");
#endif
	std::int32_t displacement = 0;
	std::memcpy(&displacement, reinterpret_cast<const void*>(hitSite + 1), sizeof(displacement));
	const auto trampolineAddress = hitSite + 5 + displacement;
	constexpr std::array<std::uint8_t, 6> trampolineBytes{ 0xFF, 0x25, 0, 0, 0, 0 };
	std::uintptr_t trampolineTarget = 0;
	MEMORY_BASIC_INFORMATION memory{};
	const bool readable = VirtualQuery(reinterpret_cast<const void*>(trampolineAddress), &memory, sizeof(memory)) == sizeof(memory) &&
		memory.State == MEM_COMMIT && !(memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
		trampolineAddress >= reinterpret_cast<std::uintptr_t>(memory.BaseAddress) &&
		trampolineAddress - reinterpret_cast<std::uintptr_t>(memory.BaseAddress) <= memory.RegionSize &&
		memory.RegionSize - (trampolineAddress - reinterpret_cast<std::uintptr_t>(memory.BaseAddress)) >= 14;
	bool hitInstalled = false;
	if (*reinterpret_cast<const std::uint8_t*>(hitSite) == 0xE8 && readable &&
		std::memcmp(reinterpret_cast<const void*>(trampolineAddress), trampolineBytes.data(), trampolineBytes.size()) == 0) {
		std::memcpy(&trampolineTarget, reinterpret_cast<const void*>(trampolineAddress + 6), sizeof(trampolineTarget));
		hitInstalled = trampolineTarget == reinterpret_cast<std::uintptr_t>(&ProcessHit);
	}
	const bool updateInstalled =
		*reinterpret_cast<const std::uintptr_t*>(vtable.address() + 0xAD * sizeof(std::uintptr_t)) ==
		reinterpret_cast<std::uintptr_t>(&PlayerUpdate);
	if (!hitInstalled || !updateInstalled) {
		logger::error("Built-in poise hook installation failed: HitData={}, player update={}; hooks remain pass-through.", hitInstalled, updateInstalled);
		return;
	}
	{
		std::lock_guard lock(stateMutex);
		installed = true;
		PublishGate();
	}
	logger::info("Built-in poise HitData and player update hooks installed; validated directional stagger routine ready.");
}

void BuiltinPoise::Configure(const Settings& settings)
{
	std::lock_guard lock(stateMutex);
	if (configuration.enabled != settings.enabled) {
		state.Reset();
		npcStates.clear();
		lastTarget = {};
		++stateGeneration;
	}
	if (configuration.npcEnabled != settings.npcEnabled ||
		configuration.enableCreaturePoise != settings.enableCreaturePoise) {
		npcStates.clear();
		lastTarget = {};
		++stateGeneration;
	}
	// Cached debuffs are cleared when the master switch flips; the next refresh rescans (or stays neutral when off).
	if (configuration.enablePoiseDebuffs != settings.enablePoiseDebuffs) {
		state.SetDebuffs({});
		for (auto& [id, entry] : npcStates) {
			entry.poise.SetDebuffs({});
		}
	}
	{
		std::lock_guard keywordLock(keywordMutex);
		bleedKeywords = settings.hemorrhagingKeywords;
	}
	configuration = settings;
	hitLog.Clear();
	equipmentRefresh = 0.0F;
	PublishGate();
	logger::info("Built-in poise configured: enabled={}, hooks={}, base={}, armor multiplier={}, regen delay={}, regen rate={}, immunity={}, suppress vanilla staggers={}.",
		settings.enabled, installed, settings.basePoise, settings.armorWeightMultiplier,
		settings.regenDelaySeconds, settings.regenRatePerSecond, settings.staggerImmunitySeconds,
		settings.suppressVanillaStaggers);
	logger::info("Creature poise configured: enabled={}, creature hit multiplier={}, tiers light={} medium={} heavy={} boss={}, boss regen multiplier={}.",
		settings.enableCreaturePoise, settings.creatureMultiplier, settings.lightCreaturePoiseMultiplier,
		settings.mediumCreaturePoiseMultiplier, settings.heavyCreaturePoiseMultiplier,
		settings.bossCreaturePoiseMultiplier, settings.bossCreatureRegenMultiplier);
	logger::info("Poise baseline configured: physical damage multiplier={} cap={}, light armor rating multiplier={} cap={}, flesh efficiency light={} heavy={}.",
		settings.physicalDamagePoiseMultiplier, settings.physicalDamagePoiseCap,
		settings.lightArmorRatingPoiseMultiplier, settings.lightArmorRatingPoiseCap,
		settings.lightArmorFleshMultiplier, settings.heavyArmorFleshMultiplier);
}

void BuiltinPoise::BeginGame()
{
#ifdef ARPGHUD_MAGIC_GAMEPLAY
	MagicGameplay::Reset();
#endif
	std::lock_guard lock(stateMutex);
	state.Reset();
	npcStates.clear();
	lastTarget = {};
	hitLog.Clear();
	weaponBenchmark = {};
	lastShockHit = kNoShock;
	equipmentRefresh = 0.0F;
	gameplaySeconds = 0.0;
	cleanupTimer = 0.0F;
	creatureRefreshTimer = 0.0F;
	inGame = true;
	++stateGeneration;
	PublishGate();
}

void BuiltinPoise::EndGame()
{
#ifdef ARPGHUD_MAGIC_GAMEPLAY
	MagicGameplay::Reset();
#endif
	std::lock_guard lock(stateMutex);
	inGame = false;
	state.Reset();
	npcStates.clear();
	lastTarget = {};
	hitLog.Clear();
	weaponBenchmark = {};
	lastShockHit = kNoShock;
	equipmentRefresh = 0.0F;
	++stateGeneration;
	PublishGate();
}

#ifdef ARPGHUD_MAGIC_GAMEPLAY
std::uint64_t BuiltinPoise::Generation()
{
	std::lock_guard lock(stateMutex);
	return stateGeneration;
}

bool BuiltinPoise::IsGenerationActive(std::uint64_t generation)
{
	std::lock_guard lock(stateMutex);
	return installed && inGame && configuration.enabled && generation == stateGeneration;
}

BuiltinPoise::Settings BuiltinPoise::GetSettings()
{
	std::lock_guard lock(stateMutex);
	return configuration;
}

void BuiltinPoise::ApplyMagicDamage(RE::Actor* target, RE::Actor* attacker, float damage, bool wardMitigated,
	const HitBreakdown* breakdown)
{
	if (!target || !attacker || target == attacker || !std::isfinite(damage) || damage <= 0.0F || !Active()) {
		return;
	}
	const bool player = target->IsPlayerRef();
	const auto trackedTier = player ? std::optional<CreatureTier>{} : TrackedTier(target);
	if (player ? (target->IsDead() || target->AsActorState()->IsBleedingOut() ||
			target->IsInKillMove() || !target->Is3DLoaded()) : !trackedTier) {
		return;
	}
	const auto tier = trackedTier.value_or(CreatureTier::Standard);
	const auto handle = RE::ActorHandle(target);
	const auto id = target->GetFormID();
	bool needsMaximum = false;
	{
		std::lock_guard lock(stateMutex);
		if (player) {
			needsMaximum = state.maximum <= 0.0F;
		} else {
			const auto it = npcStates.find(id);
			needsMaximum = it == npcStates.end() || it->second.handle != handle || it->second.tier != tier ||
				it->second.poise.maximum <= 0.0F;
		}
	}
	const auto armor = ScanArmor(target);
	const auto flesh = needsMaximum ? ActiveFlesh(target) : FleshRating{};
	const auto debuffs = ScanDebuffs(target);
	const auto blockKind = wardMitigated ? BlockKind::none : CurrentBlock(target);
	const bool recordHit = telemetryEnabled.load(std::memory_order_relaxed) && (player || attacker->IsPlayerRef());
	HitBreakdown record;
	std::string attackerLabel;
	std::string targetLabel;
	if (recordHit) {
		if (breakdown) {
			record = *breakdown;
		} else {
			record.typeLabel = "Magic";
			record.baseDamage = damage;
		}
		attackerLabel = ActorLabel(attacker);
		targetLabel = ActorLabel(target);
	}
	HitResult result;
	{
		std::lock_guard lock(stateMutex);
		if (!installed || !inGame || !configuration.enabled) {
			return;
		}
		const auto armorScale = MagicArmorPoiseScale(configuration, armor);
		damage *= armorScale.multiplier;
		if (!std::isfinite(damage) || damage <= 0.0F) {
			return;
		}
		if (recordHit) {
			record.Add(armorScale.label, armorScale.multiplier);
		}
		const float blockScale = BlockedMagicScale(configuration, blockKind);
		damage *= blockScale;
		if (recordHit) {
			if (blockKind != BlockKind::none) {
				record.Add(blockKind == BlockKind::shield ? "Shield Block" : "Weapon Block", blockScale);
			}
			record.finalDamage = damage;
		}
		if (player) {
			if (state.maximum <= 0.0F) {
				state.SetMaximum(MaximumPoise(configuration, CreatureTier::Standard, armor, flesh, nullptr));
			}
			state.SetDebuffs(debuffs);
			result = state.ApplyDamage(configuration, damage);
		} else if (npcInstalled && TierEnabled(configuration, tier) && handle) {
			auto it = npcStates.find(id);
			if (it == npcStates.end() && npcStates.size() >= kMaxTrackedNPCs) {
				if (!npcLimitLogged) {
					logger::warn("NPC poise tracking limit reached; magic poise hit skipped.");
					npcLimitLogged = true;
				}
				return;
			}
			auto& entry = npcStates[id];
			if (entry.handle != handle || entry.tier != tier) {
				entry = {};
				entry.handle = handle;
				entry.tier = tier;
			}
			if (entry.poise.maximum <= 0.0F) {
				entry.poise.SetMaximum(MaximumPoise(configuration, tier, armor, flesh, target));
				entry.poise.ApplyTier(configuration, tier);
			}
			entry.poise.SetDebuffs(debuffs);
			entry.lastUpdate = gameplaySeconds;
			CatchUp(entry);
			result = entry.poise.ApplyDamage(configuration, damage);
			if (result.handled) {
				NoteTarget(target, id, handle, tier, &entry.poise);
			}
		}
		if (recordHit && result.handled) {
			RecordHit(!player, std::move(attackerLabel), std::move(targetLabel), record, result);
		}
	}
	if (result.broke && directionalStagger) {
		StaggerGate::ScopedAllowStagger allowStagger;
		directionalStagger(target, result.magnitude, attacker);
	}
}

void BuiltinPoise::NoteMagicTarget(RE::Actor* target, RE::Actor* attacker, bool absorb)
{
	if (!target || !attacker || target == attacker || !Active()) {
		return;
	}
	if (target->IsPlayerRef()) {
		// A hostile absorb on the player (an enchanted weapon, Vampiric Drain) holds Sapped for a short window.
		if (absorb) {
			std::lock_guard lock(stateMutex);
			if (installed && inGame && configuration.enabled && state.maximum > 0.0F) {
				state.NoteSapped("Absorb");
			}
		}
		return;
	}
	if (!attacker->IsPlayerRef()) {
		return;
	}
	const auto trackedTier = TrackedTier(target);
	if (!trackedTier) {
		return;
	}
	const auto tier = *trackedTier;
	const auto handle = RE::ActorHandle(target);
	const auto id = target->GetFormID();
	bool needsMaximum = false;
	{
		std::lock_guard lock(stateMutex);
		const auto it = npcStates.find(id);
		needsMaximum = it == npcStates.end() || it->second.handle != handle || it->second.tier != tier ||
			it->second.poise.maximum <= 0.0F;
	}
	const auto armor = needsMaximum ? ScanArmor(target) : ArmorProfile{};
	const auto flesh = needsMaximum ? ActiveFlesh(target) : FleshRating{};
	const auto debuffs = ScanDebuffs(target);
	std::lock_guard lock(stateMutex);
	if (!installed || !inGame || !configuration.enabled || !npcInstalled || !TierEnabled(configuration, tier) || !handle) {
		return;
	}
	auto it = npcStates.find(id);
	if (it == npcStates.end() && npcStates.size() >= kMaxTrackedNPCs) {
		return;
	}
	auto& entry = npcStates[id];
	if (entry.handle != handle || entry.tier != tier) {
		entry = {};
		entry.handle = handle;
		entry.tier = tier;
	}
	if (entry.poise.maximum <= 0.0F) {
		entry.poise.SetMaximum(MaximumPoise(configuration, tier, armor, flesh, target));
		entry.poise.ApplyTier(configuration, tier);
	}
	entry.poise.SetDebuffs(debuffs);
	if (absorb) {
		entry.poise.NoteSapped("Absorb");
	}
	entry.lastUpdate = gameplaySeconds;
	CatchUp(entry);
	NoteTarget(target, id, handle, tier, &entry.poise);
}
#endif

bool BuiltinPoise::IsInstalled()
{
	std::lock_guard lock(stateMutex);
	return installed;
}

bool BuiltinPoise::ShouldSuppressStagger(RE::Actor* actor)
{
	if (!actor || !gateActive.load(std::memory_order_relaxed) || !gateSuppress.load(std::memory_order_relaxed)) {
		return false;
	}
	bool managed = false;
	if (actor->IsPlayerRef()) {
		managed = !actor->IsDead() && !actor->AsActorState()->IsBleedingOut() &&
			!actor->IsInKillMove() && actor->Is3DLoaded();
	} else if (const auto tier = TrackedTier(actor)) {
		managed = *tier == CreatureTier::Standard ? gateNpc.load(std::memory_order_relaxed) :
			gateCreature.load(std::memory_order_relaxed);
	}
	const bool allowed = StaggerGate::Allowed() ||
		(StaggerGate::InTimedBash() && gateTimedBash.load(std::memory_order_relaxed));
	return StaggerGate::ShouldSuppress(true, managed, allowed);
}

std::string BuiltinPoise::DescribeActorClass(RE::Actor* actor)
{
	if (!actor) {
		return "null";
	}
	const auto* race = actor->GetRace();
	const char* editorID = race ? race->GetFormEditorID() : nullptr;
	const auto tier = TrackedTier(actor);
	return std::format("race={} tier={}", editorID && *editorID ? editorID : "?",
		tier ? TierName(*tier) : "untracked");
}

bool BuiltinPoise::Read(PoiseBridge::Values& values)
{
	std::lock_guard lock(stateMutex);
	values = {};
	if (!installed || !inGame || !configuration.enabled || state.maximum <= 0.0F) {
		return false;
	}
	values.current = state.current;
	values.maximum = state.maximum;
	return true;
}

void BuiltinPoise::SetTelemetryEnabled(bool enabled)
{
	std::lock_guard lock(stateMutex);
	telemetryEnabled.store(enabled, std::memory_order_relaxed);
	// Force the next player update to refresh the weapon benchmark immediately.
	equipmentRefresh = 0.0F;
	if (!enabled) {
		hitLog.Clear();
		weaponBenchmark = {};
	}
}

bool BuiltinPoise::TelemetryEnabled()
{
	return telemetryEnabled.load(std::memory_order_relaxed);
}

void BuiltinPoise::SetReactiveBarsEnabled(bool enabled)
{
	std::lock_guard lock(stateMutex);
	reactiveBarsEnabled.store(enabled, std::memory_order_relaxed);
	// Rescan the player's effects on the next update so the bars react immediately.
	equipmentRefresh = 0.0F;
	if (!enabled) {
		lastShockHit = kNoShock;
	}
}

void BuiltinPoise::NoteShockHit()
{
	if (!reactiveBarsEnabled.load(std::memory_order_relaxed)) {
		return;
	}
	std::lock_guard lock(stateMutex);
	lastShockHit = gameplaySeconds;
}

bool BuiltinPoise::ReadReactiveBars(ReactiveBarView& view)
{
	std::lock_guard lock(stateMutex);
	view = {};
	if (!reactiveBarsEnabled.load(std::memory_order_relaxed) || !installed || !inGame || !configuration.enabled ||
		state.maximum <= 0.0F) {
		return false;
	}
	const auto& debuffs = state.debuffs;
	view.healthPoison = debuffs.Has(DebuffId::enfeebled);
	view.healthFire = debuffs.Has(DebuffId::scorched);
	view.healthBleed = debuffs.Has(DebuffId::hemorrhaging);
	view.healthSapped = debuffs.Has(DebuffId::sapped);
	view.staminaFrost = debuffs.Has(DebuffId::numbed);
	view.barShock = gameplaySeconds - lastShockHit <= kShockHitSeconds;
	return true;
}

bool BuiltinPoise::ReadPlayerDebuffs(PlayerDebuffView& view)
{
	std::lock_guard lock(stateMutex);
	view = {};
	if (!installed || !inGame || !configuration.enabled || !configuration.enablePoiseDebuffs || state.maximum <= 0.0F) {
		return false;
	}
	const auto fx = ResolveDebuffEffects(configuration, state.debuffs);
	view.active = fx.active;
	view.ceilingRatio = fx.ceilingRatio;
	return true;
}

void BuiltinPoise::RefreshTelemetryTarget()
{
	if (!telemetryEnabled.load(std::memory_order_relaxed)) {
		return;
	}
	RE::ActorHandle handle;
	RE::FormID id = 0;
	{
		std::lock_guard lock(stateMutex);
		if (!installed || !inGame || !configuration.enabled || lastTarget.id == 0 ||
			gameplaySeconds - lastTarget.lastHit > kTelemetryTargetSeconds) {
			return;
		}
		id = lastTarget.id;
		handle = lastTarget.handle;
	}
	const auto actor = handle.get();
	if (!actor || actor->IsDead() || !actor->Is3DLoaded()) {
		return;
	}
	const auto debuffs = ScanDebuffs(actor.get());
	std::lock_guard lock(stateMutex);
	const auto it = npcStates.find(id);
	if (it != npcStates.end() && it->second.handle == handle) {
		it->second.poise.SetDebuffs(debuffs);
	}
}

bool BuiltinPoise::ReadTelemetry(TelemetrySnapshot& snapshot)
{
	snapshot = {};
	if (!telemetryEnabled.load(std::memory_order_relaxed)) {
		return false;
	}
	RE::ActorHandle targetHandle;
	{
		std::lock_guard lock(stateMutex);
		if (!installed || !inGame || !configuration.enabled || state.maximum <= 0.0F) {
			return false;
		}
		snapshot.player = DescribePoise(state, "Player", "Player");
		snapshot.benchmark = weaponBenchmark;
		for (std::size_t i = 0; i < hitLog.Size(); ++i) {
			snapshot.hits.push_back(hitLog.Newest(i));
		}
		if (lastTarget.id && gameplaySeconds - lastTarget.lastHit <= kTelemetryTargetSeconds) {
			const auto it = npcStates.find(lastTarget.id);
			if (it != npcStates.end() && it->second.handle == lastTarget.handle && it->second.poise.maximum > 0.0F) {
				CatchUp(it->second);
				lastTarget.view = DescribePoise(it->second.poise, lastTarget.name, TierLabel(lastTarget.tier));
				lastTarget.hasView = true;
				snapshot.target = lastTarget.view;
				snapshot.hasTarget = true;
				targetHandle = lastTarget.handle;
			} else if (lastTarget.hasView) {
				// The entry is gone because the target died, unloaded or stopped being tracked: keep the last
				// values on the card for the rest of the window instead of dropping it.
				snapshot.target = lastTarget.view;
				snapshot.target.stale = true;
				snapshot.hasTarget = true;
			}
		}
	}
	if (snapshot.hasTarget && targetHandle) {
		const auto actor = targetHandle.get();
		if (!actor || actor->IsDead() || !actor->Is3DLoaded()) {
			snapshot.target.stale = true;
		}
	}
	return true;
}

bool BuiltinPoise::ReadTarget(TargetValues& values)
{
	std::lock_guard lock(stateMutex);
	values = {};
	if (!installed || !inGame || !configuration.enabled || lastTarget.id == 0) {
		return false;
	}
	const double age = gameplaySeconds - lastTarget.lastHit;
	const auto it = npcStates.find(lastTarget.id);
	if (age > kTargetBarSeconds || it == npcStates.end() || it->second.handle != lastTarget.handle ||
		it->second.poise.maximum <= 0.0F) {
		return false;
	}
	CatchUp(it->second);
	values.name = lastTarget.name;
	values.tier = TierLabel(lastTarget.tier);
	values.current = it->second.poise.current;
	values.maximum = it->second.poise.maximum;
	values.immune = it->second.poise.immunity > 0.0F;
	return true;
}
