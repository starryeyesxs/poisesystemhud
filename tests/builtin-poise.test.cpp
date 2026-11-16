#include "../src/builtin_poise_state.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
	bool Near(float a, float b) { return std::abs(a - b) < 0.001F; }
}

int main()
{
	using namespace BuiltinPoise;
	Settings settings;
	assert(Near(settings.twoHandedMultiplier, 0.85F));
	assert(!settings.concentrationSpellsDealPoise);
	State state;
	Hit hit;
	assert(!state.Apply(settings, hit).handled);
	state.SetMaximum(100.0F);
	assert(Near(state.current, 100.0F));
	hit.weaponWeight = 20.0F;
	hit.twoHanded = true;
	hit.powerAttack = true;
	hit.blocked = true;
	auto result = state.Apply(settings, hit);
	assert(result.handled && !result.broke);
	assert(Near(state.current, 100.0F - 18.0F * 0.85F * 1.5F * 2.5F * 0.45F));
	state.Advance(settings, 2.0F);
	const float beforeRegen = state.current;
	state.Advance(settings, 1.5F);
	assert(Near(state.current, beforeRegen + 4.0F));
	const float percent = state.current / state.maximum;
	state.SetMaximum(200.0F);
	assert(Near(state.current, 200.0F * percent));

	for (const auto type : { HitType::melee, HitType::bash, HitType::creature, HitType::projectile }) {
		State sample;
		sample.SetMaximum(100.0F);
		Hit input;
		input.type = type;
		sample.Apply(settings, input);
		const float multiplier = type == HitType::bash ? 0.35F :
			type == HitType::creature ? 0.8F : type == HitType::projectile ? 0.8F : 1.0F;
		assert(Near(sample.current, 100.0F - 8.0F * multiplier * settings.unblockedPhysicalMultiplier));
	}
	// A shield bash is a full-strength posture hit; a weapon pommel bash is a light tap.
	State shieldBashed;
	shieldBashed.SetMaximum(100.0F);
	Hit shieldBash;
	shieldBash.type = HitType::bash;
	shieldBash.shieldBash = true;
	shieldBashed.Apply(settings, shieldBash);
	assert(Near(100.0F - shieldBashed.current, 8.0F * 1.0F * 2.5F));
	State pommelBashed;
	pommelBashed.SetMaximum(100.0F);
	shieldBash.shieldBash = false;
	pommelBashed.Apply(settings, shieldBash);
	assert(Near(100.0F - pommelBashed.current, 8.0F * 0.35F * 2.5F));
	assert(Near(settings.shieldBlockedMagicMultiplier, 0.4F) && Near(settings.weaponBlockedMagicMultiplier, 0.7F));
	assert(!settings.allowTimedBashStaggers);
	State physical;
	physical.SetMaximum(40.0F);
	Hit sword;
	sword.weaponWeight = 10.0F;
	assert(physical.Apply(settings, sword).handled);
	assert(Near(physical.current, 7.5F));
	physical.SetMaximum(40.0F);
	physical.current = 40.0F;
	sword.blocked = true;
	assert(physical.Apply(settings, sword).handled);
	assert(Near(physical.current, 40.0F - 13.0F * 2.5F * 0.45F));
	physical.current = 40.0F;
	sword.shieldBlocked = true;
	assert(physical.Apply(settings, sword).handled);
	assert(Near(physical.current, 40.0F - 13.0F * 2.5F * 0.25F));

	State magic;
	magic.SetMaximum(40.0F);
	assert(magic.ApplyDamage(settings, 25.0F).handled);
	assert(Near(magic.current, 15.0F));
	magic.Advance(settings, 2.0F);
	assert(Near(magic.current, 15.0F));
	auto magicBreak = magic.ApplyDamage(settings, 20.0F);
	assert(magicBreak.broke && Near(magic.current, 40.0F));
	assert(!magic.ApplyDamage(settings, 100.0F).broke);
	assert(Near(magic.current, 40.0F));
	magic.Advance(settings, settings.staggerImmunitySeconds);
	assert(magic.ApplyDamage(settings, 100.0F).broke);
	magic.Reset();
	assert(!magic.ApplyDamage(settings, 10.0F).handled);

	state.SetMaximum(10.0F);
	state.current = 1.0F;
	result = state.Apply(settings, hit);
	assert(result.broke && Near(state.current, state.maximum));
	assert(result.magnitude >= 0.25F && result.magnitude <= 1.0F);
	result = state.Apply(settings, hit);
	assert(result.handled && !result.broke && Near(state.current, state.maximum));
	state.Advance(settings, settings.staggerImmunitySeconds);
	assert(state.immunity == 0.0F);
	assert(state.Apply(settings, hit).handled);
	settings.enabled = false;
	assert(!state.Apply(settings, hit).handled);
	state.Reset();
	assert(state.maximum == 0.0F && state.current == 0.0F);
	state.SetMaximum(std::numeric_limits<float>::quiet_NaN());
	assert(state.maximum == 0.0F);

	settings.enabled = true;
	settings.staggerImmunitySeconds = 0.1F;
	state.SetMaximum(100.0F);
	std::mutex mutex;
	std::vector<std::thread> threads;
	for (int i = 0; i < 4; ++i) {
		threads.emplace_back([&] {
			for (int n = 0; n < 1000; ++n) {
				std::lock_guard lock(mutex);
				state.Apply(settings, Hit{});
				state.Advance(settings, 0.1F);
				assert(std::isfinite(state.current) && state.current >= 0.0F && state.current <= state.maximum);
			}
		});
	}
	for (auto& thread : threads) {
		thread.join();
	}

	// Creature tiers scale the base pool only.
	Settings tiers;
	assert(Near(tiers.creatureMultiplier, 0.8F));
	assert(Near(TierBasePoise(tiers, CreatureTier::Light), 30.0F));
	assert(Near(TierBasePoise(tiers, CreatureTier::Medium), 68.0F));
	assert(Near(TierBasePoise(tiers, CreatureTier::Heavy), 150.0F));
	assert(Near(TierBasePoise(tiers, CreatureTier::Boss), 240.0F));
	assert(Near(TierBasePoise(tiers, CreatureTier::Unknown), 40.0F));
	assert(Near(TierBasePoise(tiers, CreatureTier::Standard), 40.0F));
	assert(Near(TierRegenMultiplier(tiers, CreatureTier::Boss), 1.5F));
	assert(Near(TierRegenMultiplier(tiers, CreatureTier::Heavy), 1.0F));
	assert(IsCreatureTier(CreatureTier::Unknown) && IsCreatureTier(CreatureTier::Boss));
	assert(!IsCreatureTier(CreatureTier::Standard) && !IsCreatureTier(CreatureTier::Excluded));

	// Each toggle governs only its own actors, and excluded critters are never tracked.
	tiers.npcEnabled = false;
	assert(!TierEnabled(tiers, CreatureTier::Standard) && TierEnabled(tiers, CreatureTier::Boss));
	tiers.npcEnabled = true;
	tiers.enableCreaturePoise = false;
	assert(TierEnabled(tiers, CreatureTier::Standard) && !TierEnabled(tiers, CreatureTier::Light));
	assert(!TierEnabled(Settings{}, CreatureTier::Excluded));

	// Boss regeneration is faster than the standard rate.
	Settings regen;
	regen.regenDelaySeconds = 0.0F;
	State standard;
	standard.SetMaximum(100.0F);
	standard.current = 0.0F;
	State boss;
	boss.SetMaximum(100.0F);
	boss.current = 0.0F;
	boss.ApplyTier(regen, CreatureTier::Boss);
	standard.ApplyTier(regen, CreatureTier::Heavy);
	standard.Advance(regen, 1.0F);
	boss.Advance(regen, 1.0F);
	assert(Near(standard.current, 8.0F));
	assert(Near(boss.current, 12.0F));
	boss.SetMaximum(200.0F);
	assert(Near(boss.regenMultiplier, 1.5F));

	// A single large advance (hit-time catch-up) must clear immunity, so a follow-up hit can break again.
	Settings follow;
	State wolf;
	wolf.SetMaximum(30.0F);
	wolf.ApplyTier(follow, CreatureTier::Light);
	Hit heavyHit;
	heavyHit.weaponWeight = 16.0F;
	heavyHit.twoHanded = true;
	assert(wolf.Apply(follow, heavyHit).broke);
	assert(Near(wolf.immunity, 2.0F));
	assert(!wolf.Apply(follow, heavyHit).broke && Near(wolf.current, 30.0F));
	wolf.Advance(follow, 1.5F);
	assert(wolf.immunity > 0.0F && !wolf.Apply(follow, heavyHit).broke);
	wolf.Advance(follow, 5.0F);
	assert(Near(wolf.immunity, 0.0F));
	assert(wolf.Apply(follow, heavyHit).broke);

	// Standard NPCs keep the shorter post-break immunity; creature tiers use the creature value.
	State npcState;
	npcState.SetMaximum(30.0F);
	npcState.ApplyTier(follow, CreatureTier::Standard);
	assert(npcState.Apply(follow, heavyHit).broke && Near(npcState.immunity, 1.0F));
	State beast;
	beast.SetMaximum(30.0F);
	beast.ApplyTier(follow, CreatureTier::Unknown);
	assert(beast.Apply(follow, heavyHit).broke && Near(beast.immunity, 2.0F));

	// Outgoing creature damage scales by attacker tier; non-creature tiers are unscaled.
	Settings damage;
	assert(Near(CreatureDamageTierMultiplier(damage, CreatureTier::Light), 0.6F));
	assert(Near(CreatureDamageTierMultiplier(damage, CreatureTier::Medium), 1.2F));
	assert(Near(CreatureDamageTierMultiplier(damage, CreatureTier::Heavy), 1.375F));
	assert(Near(CreatureDamageTierMultiplier(damage, CreatureTier::Boss), 2.25F));
	for (const auto neutral : { CreatureTier::Unknown, CreatureTier::Standard, CreatureTier::Excluded }) {
		assert(Near(CreatureDamageTierMultiplier(damage, neutral), 1.0F));
	}
	const float expectedUnblocked[] = { 9.6F, 19.2F, 22.0F, 36.0F };
	const CreatureTier attackers[] = { CreatureTier::Light, CreatureTier::Medium, CreatureTier::Heavy, CreatureTier::Boss };
	for (int i = 0; i < 4; ++i) {
		State victim;
		victim.SetMaximum(1000.0F);
		Hit bite;
		bite.type = HitType::creature;
		bite.attackerTier = attackers[i];
		victim.Apply(damage, bite);
		assert(Near(1000.0F - victim.current, expectedUnblocked[i]));
	}

	// A blocked hit is a share of the unblocked hit: a boss slam bleeds 9.0 through a shield and 16.2 through a weapon block.
	State shielded;
	shielded.SetMaximum(1000.0F);
	Hit slam;
	slam.type = HitType::creature;
	slam.attackerTier = CreatureTier::Boss;
	slam.blocked = true;
	slam.shieldBlocked = true;
	shielded.Apply(damage, slam);
	assert(Near(1000.0F - shielded.current, 9.0F));
	State weaponBlocked;
	weaponBlocked.SetMaximum(1000.0F);
	slam.shieldBlocked = false;
	weaponBlocked.Apply(damage, slam);
	assert(Near(1000.0F - weaponBlocked.current, 16.2F));
	State openSlam;
	openSlam.SetMaximum(1000.0F);
	slam.blocked = false;
	openSlam.Apply(damage, slam);
	assert(Near(1000.0F - openSlam.current, 36.0F));
	assert(1000.0F - shielded.current < 1000.0F - weaponBlocked.current &&
		1000.0F - weaponBlocked.current < 1000.0F - openSlam.current);
	slam.blocked = true;
	assert(Near(damage.shieldBlockedPhysicalMultiplier, 0.25F) && Near(damage.weaponBlockedPhysicalMultiplier, 0.45F));
	assert(Near(damage.creatureStaggerImmunitySeconds, 2.0F) && Near(damage.summonPoiseMultiplier, 1.5F));
	assert(Near(damage.npcMagicPoiseMultiplier, 0.6F));

	// Light breaks keep the 0.25 floor; boss breaks always stagger at full strength.
	Settings breaking;
	State light;
	light.SetMaximum(100.0F);
	light.ApplyTier(breaking, CreatureTier::Light);
	light.current = 1.0F;
	auto lightBreak = light.ApplyDamage(breaking, 5.0F);
	assert(lightBreak.broke && Near(lightBreak.magnitude, 0.25F));
	State colossus;
	colossus.SetMaximum(240.0F);
	colossus.ApplyTier(breaking, CreatureTier::Boss);
	colossus.current = 1.0F;
	auto bossBreak = colossus.ApplyDamage(breaking, 5.0F);
	assert(bossBreak.broke && Near(bossBreak.magnitude, 1.0F));
	assert(Near(colossus.current, 240.0F));

	// Physical-damage bonus: added to base poise damage before the type, power-attack and block multipliers.
	Settings baseline;
	assert(Near(baseline.physicalDamagePoiseMultiplier, 0.05F) && Near(baseline.physicalDamagePoiseCap, 6.0F));
	assert(Near(baseline.lightArmorRatingPoiseMultiplier, 0.15F) && Near(baseline.lightArmorRatingPoiseCap, 15.0F));
	assert(Near(baseline.lightArmorFleshMultiplier, 0.75F) && Near(baseline.heavyArmorFleshMultiplier, 0.45F));
	auto bonusOf = [&](float dealt) { return PhysicalDamageBonus(baseline, dealt); };
	assert(Near(bonusOf(0.0F).bonus, 0.0F) && !bonusOf(0.0F).capped);
	assert(Near(bonusOf(-50.0F).bonus, 0.0F) && !bonusOf(-50.0F).capped);
	assert(Near(bonusOf(std::numeric_limits<float>::quiet_NaN()).bonus, 0.0F));
	assert(Near(bonusOf(30.0F).bonus, 1.5F) && !bonusOf(30.0F).capped);
	assert(Near(bonusOf(60.0F).bonus, 3.0F));
	assert(Near(bonusOf(120.0F).bonus, 6.0F) && bonusOf(120.0F).capped);
	assert(Near(bonusOf(5000.0F).bonus, 6.0F) && bonusOf(5000.0F).capped);
	assert(Near(bonusOf(std::numeric_limits<float>::infinity()).bonus, 0.0F));
	Settings uncapped = baseline;
	uncapped.physicalDamagePoiseCap = 0.0F;
	assert(Near(PhysicalDamageBonus(uncapped, 100.0F).bonus, 0.0F) && PhysicalDamageBonus(uncapped, 100.0F).capped);

	State sneaky;
	sneaky.SetMaximum(1000.0F);
	Hit stab;
	stab.weaponWeight = 10.0F;
	stab.physicalDamage = 60.0F;
	auto stabResult = sneaky.Apply(baseline, stab);
	assert(stabResult.handled && Near(stabResult.rawBonus, 3.0F) && !stabResult.rawCapped);
	assert(Near(1000.0F - sneaky.current, (8.0F + 5.0F + 3.0F) * 2.5F));
	assert(Near(sneaky.lastRawBonus, 3.0F) && !sneaky.lastRawCapped);
	stab.type = HitType::creature;
	stab.attackerTier = CreatureTier::Boss;
	stab.physicalDamage = 400.0F;
	stab.powerAttack = true;
	stab.blocked = true;
	State slammed;
	slammed.SetMaximum(1000.0F);
	auto slamResult = slammed.Apply(baseline, stab);
	assert(slamResult.rawCapped && Near(slamResult.rawBonus, 6.0F));
	assert(Near(1000.0F - slammed.current, (8.0F + 5.0F + 6.0F) * 0.8F * 2.25F * 1.5F * 2.5F * 0.45F));
	// Telemetry still reports the bonus while stagger immunity absorbs the hit.
	State immune;
	immune.SetMaximum(10.0F);
	immune.current = 1.0F;
	Hit finisher;
	finisher.physicalDamage = 80.0F;
	assert(immune.Apply(baseline, finisher).broke);
	auto absorbed = immune.Apply(baseline, finisher);
	assert(absorbed.handled && !absorbed.broke && Near(absorbed.rawBonus, 4.0F));
	// Zero physical damage keeps the pre-baseline formula.
	Hit legacy;
	legacy.weaponWeight = 10.0F;
	State legacyState;
	legacyState.SetMaximum(100.0F);
	assert(Near(legacyState.Apply(baseline, legacy).rawBonus, 0.0F));
	assert(Near(100.0F - legacyState.current, 13.0F * 2.5F));

	// Light Armor rating bonus is raw-rating based, capped, and clamped at its own bounds.
	assert(Near(LightArmorTierBonus(baseline, 0.0F), 0.0F));
	assert(Near(LightArmorTierBonus(baseline, 40.0F), 6.0F));
	assert(Near(LightArmorTierBonus(baseline, 100.0F), 15.0F));
	assert(Near(LightArmorTierBonus(baseline, 5000.0F), 15.0F));
	assert(Near(LightArmorTierBonus(baseline, -20.0F), 0.0F));
	assert(Near(LightArmorTierBonus(baseline, std::numeric_limits<float>::quiet_NaN()), 0.0F));

	// Flesh efficiency priority: Heavy over Light over Cloth.
	ArmorProfile naked;
	ArmorProfile cloth;
	cloth.weight = 2.0F;
	ArmorProfile lightWear;
	lightWear.hasLight = true;
	ArmorProfile heavyWear;
	heavyWear.hasHeavy = true;
	ArmorProfile mixed;
	mixed.hasLight = true;
	mixed.hasHeavy = true;
	assert(Near(FleshEfficiency(baseline, naked), 1.0F) && Near(FleshEfficiency(baseline, cloth), 1.0F));
	assert(Near(FleshEfficiency(baseline, lightWear), 0.75F));
	assert(Near(FleshEfficiency(baseline, heavyWear), 0.45F));
	assert(Near(FleshEfficiency(baseline, mixed), 0.45F));

	// Maximum poise combines weight, light tier bonus and efficiency-scaled flesh; summon scale applies to the total.
	FleshRating ward;
	ward.armor = 100.0F;
	ward.dragonhide = true;
	const float fleshRaw = 100.0F * 0.45F + 60.0F;
	auto unarmored = ComputeMaximumPoise(baseline, CreatureTier::Standard, naked, ward, 1.0F, true);
	assert(Near(unarmored.maximum, 40.0F + fleshRaw) && Near(unarmored.fleshBonus, fleshRaw));
	assert(Near(unarmored.lightArmorBonus, 0.0F) && Near(unarmored.fleshEfficiency, 1.0F));
	ArmorProfile leather;
	leather.weight = 20.0F;
	leather.lightRating = 60.0F;
	leather.hasLight = true;
	auto leatherMax = ComputeMaximumPoise(baseline, CreatureTier::Standard, leather, ward, 1.0F, true);
	assert(Near(leatherMax.lightArmorBonus, 9.0F) && Near(leatherMax.fleshBonus, fleshRaw * 0.75F));
	assert(Near(leatherMax.maximum, 40.0F + 20.0F + 9.0F + fleshRaw * 0.75F));
	ArmorProfile plate;
	plate.weight = 60.0F;
	plate.lightRating = 300.0F;
	plate.hasHeavy = true;
	auto plateMax = ComputeMaximumPoise(baseline, CreatureTier::Standard, plate, ward, 1.0F, true);
	assert(Near(plateMax.maximum, 40.0F + 60.0F + 15.0F + fleshRaw * 0.45F));
	Settings noHeavyFlesh = baseline;
	noHeavyFlesh.heavyArmorFleshMultiplier = 0.0F;
	assert(Near(ComputeMaximumPoise(noHeavyFlesh, CreatureTier::Standard, heavyWear, ward, 1.0F, true).fleshBonus, 0.0F));
	auto summoned = ComputeMaximumPoise(baseline, CreatureTier::Light, leather, ward, 1.5F, false);
	assert(Near(summoned.maximum, (30.0F + 20.0F + 9.0F + fleshRaw * 0.75F) * 1.5F));
	assert(Near(summoned.lightArmorBonus, 9.0F * 1.5F));
	assert(Near(ComputeMaximumPoise(baseline, CreatureTier::Standard, naked, FleshRating{}, 1.0F, true).maximum, 40.0F));

	Settings shieldSettings;
	shieldSettings.armorWeightMultiplier = 2.0F;
	ArmorProfile shieldArmor;
	shieldArmor.weight = 10.0F;
	shieldArmor.shieldWeight = 20.0F;
	const auto shieldMaximum = [&](bool player, float scale = 1.0F) {
		return ComputeMaximumPoise(shieldSettings, CreatureTier::Standard, shieldArmor, FleshRating{}, scale, player).maximum;
	};
	assert(Near(shieldSettings.shieldWeightPoiseMultiplier, 0.0F) && shieldSettings.applyShieldWeightToNPCs);
	assert(Near(shieldMaximum(true), 60.0F));
	assert(Near(shieldMaximum(false), 100.0F));
	shieldSettings.shieldWeightPoiseMultiplier = -0.25F;
	assert(Near(shieldMaximum(true), 55.0F));
	shieldArmor.shieldWeight = 400.0F;
	assert(Near(shieldMaximum(true), 10.0F));
	assert(Near(shieldMaximum(true, 0.5F), 10.0F));
	shieldArmor.shieldWeight = 20.0F;
	assert(Near(shieldMaximum(false), 100.0F));
	shieldSettings.applyShieldWeightToNPCs = false;
	assert(Near(shieldMaximum(false), 55.0F));
	shieldSettings.shieldWeightPoiseMultiplier = 1.0F;
	assert(Near(shieldMaximum(true), 80.0F));
	assert(Near(shieldMaximum(false), 80.0F));
	shieldSettings.applyShieldWeightToNPCs = true;
	assert(Near(shieldMaximum(false), 100.0F));

	// Breakdown telemetry lands on the state and clears with it.
	State telemetry;
	telemetry.SetMaximum(leatherMax);
	assert(Near(telemetry.maximum, leatherMax.maximum) && Near(telemetry.fleshBonusPoise, leatherMax.fleshBonus));
	assert(Near(telemetry.lightArmorBonusPoise, 9.0F));
	telemetry.Reset();
	assert(Near(telemetry.fleshBonusPoise, 0.0F) && Near(telemetry.lightArmorBonusPoise, 0.0F));
}
