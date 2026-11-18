#include "magic_gameplay.h"

#ifdef ARPGHUD_MAGIC_GAMEPLAY
#include "builtin_poise.h"
#include "hook_call.h"
#include "magic_poise_math.h"

#include <array>
#include <atomic>
#include <cstring>
#include <unordered_map>

namespace
{
	using AddTargetFunction = bool (*)(RE::MagicTarget*, RE::MagicTarget::AddTargetData*);
	using SpellCollisionFunction = void (*)(RE::Projectile*, RE::TESObjectREFR*, const void*, float, float);
	using CollisionFunction = bool (*)(RE::Projectile*, RE::TESObjectREFR*, const void*, const void*,
		std::int32_t, std::int32_t, bool*);
	AddTargetFunction originalAddTarget = nullptr;
	SpellCollisionFunction originalSpellCollision = nullptr;
	CollisionFunction originalCollision = nullptr;
	thread_local RE::Projectile* currentProjectile = nullptr;
	thread_local RE::TESObjectREFR* currentTarget = nullptr;

	struct Impact
	{
		RE::ProjectileHandle projectile;
		RE::ActorHandle target;
		RE::ActorHandle attacker;
		std::uint64_t generation = 0;
		float magnitude = 0.0F;
		RE::ActorValue resistance = RE::ActorValue::kNone;
		bool concentration = false;
		// The spell's casting type is kConcentration (Flames, Frostbite, Sparks); distinct from the pooling flag above,
		// which also covers cloak ticks.
		bool channeled = false;
		bool ward = false;
		// Cloak, hazard and explosion effects reach the target without a spell projectile.
		bool projectileless = false;
		std::uint32_t spell = 0;
	};

	struct ProjectileHits
	{
		RE::ProjectileHandle handle;
		std::unordered_set<std::uint32_t> targets;
	};
	struct Pool
	{
		RE::ActorHandle target;
		RE::ActorHandle attacker;
		MagicPoise::ConcentrationPool damage;
	};
	std::unordered_map<std::uint32_t, ProjectileHits> bursts;
	std::unordered_map<std::uint32_t, Pool> pools;
	// A direct projectile hit and its explosion are the same spell on the same target, so only the first counts.
	std::unordered_map<std::uint64_t, double> recentSpellHits;
	double magicClock = 0.0;
	constexpr double kSpellHitWindowSeconds = 1.5;
	constexpr std::size_t kMaxProjectiles = 4096;
	constexpr std::size_t kMaxPools = 4096;
	constexpr float kMinimumProjectilelessBurstMagnitude = 8.0F;
	float cleanupRemaining = 1.0F;
	std::atomic_bool advancePending{ false };
	unsigned appliedLogCount = 0;

	// Health damage (including health/magicka dual-value drains) that resists as fire, frost or shock.
	bool ElementalHealthEffect(const RE::EffectSetting* effect)
	{
		if (!effect || !effect->IsHostile()) {
			return false;
		}
		const bool health = effect->data.primaryAV == RE::ActorValue::kHealth ||
			(effect->data.archetype == RE::EffectArchetype::kDualValueModifier &&
			 effect->data.secondaryAV == RE::ActorValue::kHealth);
		if (!health) {
			return false;
		}
		const auto resist = effect->data.resistVariable;
		return resist == RE::ActorValue::kResistFire || resist == RE::ActorValue::kResistFrost ||
			resist == RE::ActorValue::kResistShock;
	}

	// Returns true when this spell already hit this target within the window; otherwise records the hit.
	bool RecentlyHit(std::uint32_t spell, std::uint32_t target)
	{
		if (spell == 0 || target == 0) {
			return false;
		}
		const auto key = (static_cast<std::uint64_t>(spell) << 32) | target;
		const auto [it, inserted] = recentSpellHits.try_emplace(key, magicClock);
		if (inserted) {
			return false;
		}
		if (magicClock - it->second < kSpellHitWindowSeconds) {
			return true;
		}
		it->second = magicClock;
		return false;
	}

	Impact MakeImpact(RE::Projectile* projectile, RE::Actor* target)
	{
		Impact impact;
		if (!projectile || !target) {
			return impact;
		}
		impact.projectile = RE::ProjectileHandle(projectile);
		impact.target = RE::ActorHandle(target);
		const auto shooter = projectile->GetProjectileRuntimeData().shooter.get();
		auto* actor = shooter ? shooter->As<RE::Actor>() : nullptr;
		if (actor) {
			impact.attacker = RE::ActorHandle(actor);
		}
		impact.generation = BuiltinPoise::Generation();
		auto* spell = projectile->GetProjectileRuntimeData().spell;
		impact.spell = spell ? spell->GetFormID() : 0;
		impact.concentration = spell && spell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration;
		impact.channeled = impact.concentration;
		return impact;
	}

	void Process(Impact impact)
	{
		if (!BuiltinPoise::IsGenerationActive(impact.generation)) {
			return;
		}
		auto target = impact.target.get();
		auto attacker = impact.attacker.get();
		if (!target || !attacker || target == attacker || !impact.target ||
			(!impact.projectile && !impact.projectileless)) {
			return;
		}
		auto* ui = RE::UI::GetSingleton();
		if (!ui || ui->GameIsPaused()) {
			return;
		}
		const auto settings = BuiltinPoise::GetSettings();
		// A hostile shock hit starts the HUD's brief shock effect even when channeled spells deal no poise damage.
		if (impact.resistance == RE::ActorValue::kResistShock && !impact.ward && target->IsPlayerRef()) {
			BuiltinPoise::NoteShockHit();
		}
		// Channeled spells may deal no direct poise damage, but the target is still tracked and selected, and its
		// debuffs scanned, so Flames still sets Scorched and Frostbite still sets Numbed.
		if ((impact.channeled || impact.concentration) && attacker->IsPlayerRef() && !target->IsPlayerRef()) {
			BuiltinPoise::NoteMagicTarget(target.get(), attacker.get());
		}
		if (impact.concentration && settings.concentrationMaxPoisePerInterval <= 0.0F) {
			return;
		}
		auto* values = target->AsActorValueOwner();
		const float elemental = impact.resistance == RE::ActorValue::kNone ? 0.0F :
			values->GetActorValue(impact.resistance);
		const float magic = values->GetActorValue(RE::ActorValue::kResistMagic);
		if (!std::isfinite(impact.magnitude) || impact.magnitude <= 0.0F ||
			!std::isfinite(elemental) || !std::isfinite(magic)) {
			logger::warn("Invalid elemental magic poise input; impact skipped.");
			return;
		}
		if (impact.projectileless && !impact.concentration &&
			impact.magnitude < kMinimumProjectilelessBurstMagnitude) {
			return;
		}
		MagicPoise::DamageInput input;
		input.magnitude = impact.magnitude;
		input.magnitudeMultiplier = impact.concentration ? settings.concentrationPoiseMultiplier : settings.magicBurstPoiseMultiplier;
		input.magicBurst = !impact.concentration;
		input.staggerMultiplier = settings.magicStaggerPoiseMultiplier;
		input.elementalResistance = elemental;
		input.magicResistance = magic;
		input.wardIntercepted = impact.ward;
		input.wardMultiplier = settings.wardMagicDamageMultiplier;
		input.concentrationSpell = impact.channeled;
		input.concentrationAllowed = settings.concentrationSpellsDealPoise;
		float damage = MagicPoise::Damage(input);
		// Spells cast by anyone but the player threaten posture without stunlocking the player.
		const bool npcCaster = !attacker->IsPlayerRef();
		if (npcCaster) {
			damage *= settings.npcMagicPoiseMultiplier;
		}
		if (!std::isfinite(damage) || damage <= 0.0F) {
			return;
		}
		BuiltinPoise::HitBreakdown breakdown;
		const BuiltinPoise::HitBreakdown* breakdownPtr = nullptr;
		if (BuiltinPoise::TelemetryEnabled()) {
			const auto explained = MagicPoise::Explain(input);
			breakdown.typeLabel = impact.concentration ? "Concentration" : "Magic Burst";
			breakdown.baseDamage = explained.raw;
			breakdown.Add("Elemental Resist", explained.elemental);
			breakdown.Add("Magic Resist", explained.magic);
			if (impact.ward) {
				breakdown.Add("Ward", explained.ward);
			}
			if (npcCaster) {
				breakdown.Add("NPC Magic", settings.npcMagicPoiseMultiplier);
			}
			breakdown.finalDamage = damage;
			breakdownPtr = &breakdown;
		}
		if (appliedLogCount < 12) {
			++appliedLogCount;
			logger::info("Magic poise impact: target={:08X}, caster={:08X}, projectile={:08X}, projectileless={}, ward={}, concentration={}, base={}, elemental resistance={}, magic resistance={}, poise damage={}.",
				target->GetFormID(), attacker->GetFormID(), impact.projectile.native_handle(), impact.projectileless,
				impact.ward, impact.concentration,
				impact.magnitude, elemental, magic, damage);
		}
		if (!impact.concentration) {
			const auto targetHandle = impact.target.native_handle();
			if (impact.projectileless) {
				if (RecentlyHit(impact.spell, targetHandle)) {
					return;
				}
				BuiltinPoise::ApplyMagicDamage(target.get(), attacker.get(), damage, impact.ward, breakdownPtr);
				return;
			}
			const auto projectile = impact.projectile.native_handle();
			auto it = bursts.find(projectile);
			if (it == bursts.end()) {
				if (bursts.size() >= kMaxProjectiles) {
					logger::warn("Magic poise projectile tracking limit reached; impact skipped.");
					return;
				}
				it = bursts.emplace(projectile, ProjectileHits{ impact.projectile, {} }).first;
			}
			if (!it->second.targets.insert(targetHandle).second) {
				return;
			}
			RecentlyHit(impact.spell, targetHandle);
			BuiltinPoise::ApplyMagicDamage(target.get(), attacker.get(), damage, impact.ward, breakdownPtr);
			return;
		}
		const auto key = impact.target.native_handle();
		auto it = pools.find(key);
		if (it == pools.end()) {
			if (pools.size() >= kMaxPools) {
				logger::warn("Magic poise concentration tracking limit reached; impact skipped.");
				return;
			}
			it = pools.emplace(key, Pool{ impact.target, impact.attacker, {} }).first;
		}
		it->second.attacker = impact.attacker;
		it->second.damage.Add(damage, settings.concentrationIntervalSeconds, settings.concentrationMaxPoisePerInterval);
	}

	void Queue(Impact impact)
	{
		if ((!impact.projectile && !impact.projectileless) || !impact.target || !impact.attacker) {
			return;
		}
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) {
			logger::error("SKSE task interface unavailable; magic poise impact skipped.");
			return;
		}
		tasks->AddTask([impact]() { Process(impact); });
	}

	void QueueAbsorb(RE::ActorHandle targetHandle, RE::ActorHandle casterHandle, std::uint64_t generation)
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) {
			return;
		}
		tasks->AddTask([targetHandle, casterHandle, generation]() {
			if (!BuiltinPoise::IsGenerationActive(generation)) {
				return;
			}
			auto target = targetHandle.get();
			auto caster = casterHandle.get();
			if (target && caster) {
				BuiltinPoise::NoteMagicTarget(target.get(), caster.get(), true);
			}
		});
	}

	// Cloak, hazard and explosion effects carry no spell projectile; the caster is any actor, not just NPC-keyword ones.
	Impact MakeProjectilelessImpact(RE::MagicTarget* target, RE::MagicTarget::AddTargetData* data)
	{
		Impact impact;
		auto* victim = target->GetTargetStatsObject() ? target->GetTargetStatsObject()->As<RE::Actor>() : nullptr;
		auto* caster = data->caster ? data->caster->As<RE::Actor>() : nullptr;
		if (!victim || !caster || victim == caster) {
			return impact;
		}
		impact.target = RE::ActorHandle(victim);
		impact.attacker = RE::ActorHandle(caster);
		impact.generation = BuiltinPoise::Generation();
		impact.projectileless = true;
		impact.spell = data->magicItem ? data->magicItem->GetFormID() : 0;
		// Instant, non-area effects (cloaks) tick repeatedly and pool like concentration spells; explosions are one-shot.
		impact.concentration = data->castingSource == RE::MagicSystem::CastingSource::kInstant && !data->areaTarget;
		impact.channeled = data->magicItem &&
			data->magicItem->GetCastingType() == RE::MagicSystem::CastingType::kConcentration;
		impact.magnitude = data->magnitude;
		impact.resistance = data->effect->baseEffect->data.resistVariable;
		return impact;
	}

	bool AddTarget(RE::MagicTarget* target, RE::MagicTarget::AddTargetData* data)
	{
		Impact impact;
		// Hostile absorbs (Vampiric Drain, absorb enchantments) are not poise damage, but they set Sapped. Weapon
		// enchantments may leave no lasting effect to scan, so the hit itself is noted.
		RE::ActorHandle absorbTarget;
		RE::ActorHandle absorbCaster;
		std::uint64_t absorbGeneration = 0;
		if (target && data && data->effect && data->effect->baseEffect &&
			data->effect->baseEffect->HasArchetype(RE::EffectArchetype::kAbsorb) &&
			(data->effect->baseEffect->IsHostile() || (data->magicItem && data->magicItem->IsHostile()))) {
			auto* victim = target->GetTargetStatsObject() ? target->GetTargetStatsObject()->As<RE::Actor>() : nullptr;
			auto* caster = data->caster ? data->caster->As<RE::Actor>() : nullptr;
			if (victim && caster && victim != caster) {
				absorbTarget = RE::ActorHandle(victim);
				absorbCaster = RE::ActorHandle(caster);
				absorbGeneration = BuiltinPoise::Generation();
			}
		}
		if (target && data && data->effect && ElementalHealthEffect(data->effect->baseEffect)) {
			if (currentProjectile && currentTarget) {
				if (target->GetTargetStatsObject() == currentTarget) {
					auto* actor = currentTarget->As<RE::Actor>();
					impact = MakeImpact(currentProjectile, actor);
					impact.magnitude = data->magnitude;
					impact.resistance = data->effect->baseEffect->data.resistVariable;
				}
			} else {
				impact = MakeProjectilelessImpact(target, data);
			}
		}
		const bool accepted = originalAddTarget(target, data);
		if (accepted && impact.target) {
			Queue(impact);
		}
		if (accepted && absorbTarget && absorbCaster) {
			QueueAbsorb(absorbTarget, absorbCaster, absorbGeneration);
		}
		return accepted;
	}

	void SpellCollision(RE::Projectile* projectile, RE::TESObjectREFR* target,
		const void* location, float arg4, float arg5)
	{
		auto* previousProjectile = currentProjectile;
		auto* previousTarget = currentTarget;
		currentProjectile = projectile;
		currentTarget = target;
		originalSpellCollision(projectile, target, location, arg4, arg5);
		currentProjectile = previousProjectile;
		currentTarget = previousTarget;
	}

	bool Collision(RE::Projectile* projectile, RE::TESObjectREFR* target,
		const void* location, const void* velocity, std::int32_t layer, std::int32_t arg6, bool* applied)
	{
		Impact impact;
		auto* actor = target ? target->As<RE::Actor>() : nullptr;
		auto* spell = projectile ? projectile->GetProjectileRuntimeData().spell : nullptr;
		if (layer == 29 && actor && spell && actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWardPower) > 0.0F) {
			for (const auto* effect : spell->effects) {
				if (effect && ElementalHealthEffect(effect->baseEffect)) {
					impact = MakeImpact(projectile, actor);
					impact.magnitude = effect->effectItem.magnitude;
					impact.resistance = effect->baseEffect->data.resistVariable;
					impact.ward = true;
					if (!std::isfinite(impact.magnitude) || impact.magnitude <= 0.0F) {
						impact.magnitude = 8.0F;
						logger::warn("Ward impact has no usable spell magnitude; using fallback 8.");
					}
					break;
				}
			}
		}
		const bool blocked = originalCollision(projectile, target, location, velocity, layer, arg6, applied);
		if (blocked && impact.target && applied && !*applied) {
			Queue(impact);
		}
		return blocked;
	}

	template <std::size_t N>
	bool InstallCall(std::uintptr_t site, const std::array<std::uint8_t, N>& prefix,
		std::uintptr_t vanilla, std::uintptr_t replacement, std::uintptr_t& previous, const char* name)
	{
		previous = HookCall::ExecutableTarget(site);
		if (std::memcmp(reinterpret_cast<const void*>(site - N), prefix.data(), N) != 0 ||
			!previous || previous == replacement) {
			logger::error("Magic poise {} hook conflict; call site left untouched.", name);
			return false;
		}
		if (previous != vanilla) {
			logger::warn("Magic poise {} chaining existing executable destination {:X}; ABI compatibility unverified.", name, previous);
		}
		return true;
	}
}

void MagicGameplay::Install()
{
	const auto& module = REL::Module::get();
	if (module.version() != SKSE::RUNTIME_SSE_1_7_104) {
		logger::error("Magic poise hooks unavailable: runtime is not validated.");
		return;
	}
	const auto effectSite = REL::RelocationID(33742, 34526).address() + 0x20B;
	const auto spellSite = REL::ID(44205).address() + 0x1E1;
	const auto wardSite = REL::ID(44166).address() + 0x4DE;
	constexpr std::array<std::uint8_t, 6> effectPrefix{ 0x48, 0x8B, 0xD7, 0x49, 0x8B, 0xCE };
	constexpr std::array<std::uint8_t, 9> spellPrefix{ 0x4C, 0x8B, 0xC5, 0x49, 0x8B, 0xD6, 0x48, 0x8B, 0xCF };
	constexpr std::array<std::uint8_t, 16> wardPrefix{ 0x4C, 0x8D, 0x4C, 0x24, 0x68, 0x4C, 0x8D, 0x44,
		0x24, 0x58, 0x49, 0x8B, 0xD6, 0x49, 0x8B, 0xCC };
	std::uintptr_t effectPrevious = 0;
	std::uintptr_t spellPrevious = 0;
	std::uintptr_t wardPrevious = 0;
	const bool effectValid = InstallCall(effectSite, effectPrefix, module.base() + 0x5D30D0,
		reinterpret_cast<std::uintptr_t>(&AddTarget), effectPrevious, "effect");
	const bool spellValid = InstallCall(spellSite, spellPrefix, module.base() + 0x8010C0,
		reinterpret_cast<std::uintptr_t>(&SpellCollision), spellPrevious, "spell collision");
	const bool wardValid = InstallCall(wardSite, wardPrefix, module.base() + 0x800E00,
		reinterpret_cast<std::uintptr_t>(&Collision), wardPrevious, "ward collision");
	if (!effectValid || !spellValid || !wardValid) {
		logger::error("Magic poise hooks not installed; preserving existing spell handling.");
		return;
	}
	originalAddTarget = reinterpret_cast<AddTargetFunction>(effectPrevious);
	originalSpellCollision = reinterpret_cast<SpellCollisionFunction>(spellPrevious);
	originalCollision = reinterpret_cast<CollisionFunction>(wardPrevious);
	SKSE::GetTrampoline().write_call<5>(effectSite, AddTarget);
	SKSE::GetTrampoline().write_call<5>(spellSite, SpellCollision);
	SKSE::GetTrampoline().write_call<5>(wardSite, Collision);
	logger::info("Experimental magic poise body and ward hooks installed; ward chip uses spell record magnitude.");
	logger::warn("Magic stagger contribution is unavailable in this candidate; fMagicStaggerPoiseMultiplier has no effect.");
}

namespace
{
	void Advance(float delta)
	{
		if (!std::isfinite(delta) || delta <= 0.0F) {
			return;
		}
		const auto settings = BuiltinPoise::GetSettings();
		if (!settings.enabled || settings.concentrationMaxPoisePerInterval <= 0.0F) {
			pools.clear();
		}
		for (auto it = pools.begin(); it != pools.end();) {
			const float damage = it->second.damage.Advance(delta, settings.concentrationMaxPoisePerInterval);
			if (damage > 0.0F) {
				auto target = it->second.target.get();
				auto attacker = it->second.attacker.get();
				if (target && attacker) {
					BuiltinPoise::HitBreakdown pooled;
					if (BuiltinPoise::TelemetryEnabled()) {
						pooled.typeLabel = "Concentration";
						pooled.baseDamage = damage;
						pooled.finalDamage = damage;
					}
					BuiltinPoise::ApplyMagicDamage(target.get(), attacker.get(), damage, false,
						BuiltinPoise::TelemetryEnabled() ? &pooled : nullptr);
				}
				it = pools.erase(it);
			} else if (!it->second.target.get() || !it->second.attacker.get()) {
				it = pools.erase(it);
			} else {
				++it;
			}
		}
		magicClock += delta;
		cleanupRemaining -= delta;
		if (cleanupRemaining <= 0.0F) {
			cleanupRemaining = 1.0F;
			std::erase_if(bursts, [](const auto& pair) { return !pair.second.handle.get(); });
			std::erase_if(recentSpellHits, [](const auto& pair) {
				return magicClock - pair.second >= kSpellHitWindowSeconds;
			});
		}
	}
}

void MagicGameplay::ScheduleAdvance(float delta)
{
	if (!std::isfinite(delta) || delta <= 0.0F || advancePending.exchange(true)) {
		return;
	}
	auto* tasks = SKSE::GetTaskInterface();
	if (!tasks) {
		advancePending.store(false);
		logger::error("SKSE task interface unavailable; magic poise concentration update skipped.");
		return;
	}
	const auto generation = BuiltinPoise::Generation();
	tasks->AddTask([delta, generation]() {
		if (BuiltinPoise::IsGenerationActive(generation)) {
			Advance(delta);
		}
		advancePending.store(false);
	});
}

void MagicGameplay::Reset()
{
	bursts.clear();
	pools.clear();
	recentSpellHits.clear();
	magicClock = 0.0;
	cleanupRemaining = 1.0F;
	appliedLogCount = 0;
}
#endif
