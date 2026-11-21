#include "../src/builtin_poise_telemetry.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>

namespace
{
	bool Near(float a, float b) { return std::abs(a - b) < 0.001F; }

	using namespace BuiltinPoise;

	EffectFacts Hostile()
	{
		EffectFacts facts;
		facts.hostile = true;
		return facts;
	}

	PoiseDebuffs With(const EffectFacts& facts, const char* name = "Effect")
	{
		PoiseDebuffs debuffs;
		AccumulateDebuff(debuffs, facts, name);
		return debuffs;
	}

	State Ready(const Settings& settings, float maximum = 100.0F)
	{
		State state;
		state.SetMaximum(maximum);
		(void)settings;
		return state;
	}
}

int main()
{
	Settings settings;

	// Classification: every debuff has exactly one trigger and nothing fires for friendly or self-cast effects.
	EffectFacts absorb = Hostile();
	absorb.kind = EffectKind::absorb;
	assert(With(absorb).Has(DebuffId::sapped) && !With(absorb).Has(DebuffId::enfeebled));
	absorb.hostile = false;
	assert(!With(absorb).Any());
	absorb.hostile = true;
	absorb.selfCast = true;
	assert(!With(absorb).Any());

	EffectFacts poison = Hostile();
	poison.resist = EffectResist::poison;
	assert(With(poison).Has(DebuffId::enfeebled) && !With(poison).Has(DebuffId::sapped));
	EffectFacts harmful = Hostile();
	harmful.harmfulPoisonKeyword = true;
	assert(With(harmful).Has(DebuffId::enfeebled));
	EffectFacts shock = Hostile();
	shock.resist = EffectResist::other;
	shock.healthDamage = true;
	shock.duration = 5.0F;
	assert(!With(shock).Any());

	EffectFacts frost = Hostile();
	frost.resist = EffectResist::frost;
	frost.healthDamage = true;
	frost.duration = 4.0F;
	assert(With(frost).Has(DebuffId::numbed));
	frost.duration = 0.0F;
	assert(!With(frost).Any());
	frost.duration = 4.0F;
	frost.healthDamage = false;
	assert(!With(frost).Any());

	EffectFacts fire = Hostile();
	fire.resist = EffectResist::fire;
	fire.healthDamage = true;
	fire.duration = 4.0F;
	assert(With(fire).Has(DebuffId::scorched) && !With(fire).Has(DebuffId::numbed));
	fire.duration = 0.0F;
	assert(!With(fire).Any());

	EffectFacts bleed = Hostile();
	bleed.bleedMatch = true;
	assert(With(bleed).Has(DebuffId::hemorrhaging));
	bleed.hostile = false;
	assert(!With(bleed).Any());

	// Vanilla Skyrim.esm bleed (PerkBleedingDamage, 0x000C367A, used by the War Axe and Battleaxe perks) is flagged
	// detrimental only, has no editor ID at runtime and is cast with no caster: it classifies by form alone.
	{
		EffectFacts vanilla;
		vanilla.plugin = "Skyrim.esm";
		vanilla.localFormID = 0x000C367A;
		const bool bleedCandidate = IsVanillaBleedEffect(vanilla.plugin, vanilla.localFormID);
		assert(bleedCandidate);
		// detrimental without hostile, on the effect and on its parent spell
		vanilla.hostile = EffectIsHostile(false, false, true, bleedCandidate);
		assert(vanilla.hostile);
		vanilla.selfCast = false;
		assert(With(vanilla, "").Has(DebuffId::hemorrhaging) &&
			With(vanilla, "").source[static_cast<std::size_t>(DebuffId::hemorrhaging)] == "");
		vanilla.plugin = "SKYRIM.ESM";
		assert(With(vanilla).Has(DebuffId::hemorrhaging));
		vanilla.selfCast = false;
		// detrimental alone does not make an ordinary effect hostile
		assert(!EffectIsHostile(false, false, true, false));
		assert(EffectIsHostile(true, false, false, false) && EffectIsHostile(false, true, false, false));
		vanilla.hostile = EffectIsHostile(false, false, false, bleedCandidate);
		assert(!With(vanilla).Any());
	}
	// The log from the game showed PerkBleedingDamage arriving with the victim as its own caster (selfCast=true) and
	// only the detrimental flag set. A detrimental bleed counts even then; every other self-cast effect stays ignored.
	{
		EffectFacts perkBleed;
		perkBleed.plugin = "Skyrim.esm";
		perkBleed.localFormID = 0x000C367A;
		perkBleed.detrimental = true;
		perkBleed.selfCast = true;
		perkBleed.hostile = EffectIsHostile(false, false, true, IsVanillaBleedEffect(perkBleed.plugin, perkBleed.localFormID));
		perkBleed.duration = 3.0F;
		perkBleed.magnitude = -1.0F;
		perkBleed.healthDamage = true;
		const auto bled = With(perkBleed, "Bleeding Damage");
		assert(bled.Has(DebuffId::hemorrhaging) && bled.source[static_cast<std::size_t>(DebuffId::hemorrhaging)] == "Bleeding Damage");
		// only Hemorrhaging comes from a self-cast bleed, whatever else the record looks like
		assert(!bled.Has(DebuffId::sapped) && !bled.Has(DebuffId::enfeebled) && !bled.Has(DebuffId::numbed) &&
			!bled.Has(DebuffId::scorched));
		// the exception needs the detrimental flag
		perkBleed.detrimental = false;
		assert(!With(perkBleed).Any());
		perkBleed.detrimental = true;
		// a keyword match gets the same treatment
		EffectFacts modBleed = Hostile();
		modBleed.bleedMatch = true;
		modBleed.detrimental = true;
		modBleed.selfCast = true;
		assert(With(modBleed).Has(DebuffId::hemorrhaging));
		modBleed.detrimental = false;
		assert(!With(modBleed).Any());
		// self-cast fire, frost, poison and absorb effects, and non-bleed forms, are still ignored
		for (const auto kind : { 0, 1, 2, 3, 4 }) {
			EffectFacts self = Hostile();
			self.selfCast = true;
			self.detrimental = true;
			self.healthDamage = true;
			self.duration = 4.0F;
			if (kind == 0) self.resist = EffectResist::poison;
			if (kind == 1) self.resist = EffectResist::fire;
			if (kind == 2) self.resist = EffectResist::frost;
			if (kind == 3) self.kind = EffectKind::absorb;
			if (kind == 4) { self.plugin = "Skyrim.esm"; self.localFormID = 0x00073F33; }
			assert(!With(self).Any());
		}
		// not self-cast: unchanged
		perkBleed.selfCast = false;
		assert(With(perkBleed).Has(DebuffId::hemorrhaging));
	}
	// The potions that were mistaken for bleed effects (0x00073F33-35) and other forms do not match.
	for (const std::uint32_t id : { 0x00073F33u, 0x00073F34u, 0x00073F35u, 0x000C3679u, 0x000C367Bu }) {
		EffectFacts other = Hostile();
		other.plugin = "Skyrim.esm";
		other.localFormID = id;
		assert(!With(other).Any());
	}
	EffectFacts notBleed = Hostile();
	notBleed.plugin = "Update.esm";
	notBleed.localFormID = 0x000C367A;
	assert(!With(notBleed).Any());
	notBleed.plugin = {};
	assert(!With(notBleed).Any());
	assert(IsVanillaBleedEffect("Skyrim.esm", 0x000C367A) && !IsVanillaBleedEffect("Skyrim.esm.bak", 0x000C367A));

	// Absorb: a lasting absorb effect marks Sapped on scan, and an absorb hit keeps it for a short pulse that survives
	// rescans, expires on its own, and is gated by the toggles like any other debuff.
	{
		Settings pulseSettings;
		State victim;
		victim.SetMaximum(100.0F);
		victim.current = 40.0F;
		victim.Advance(pulseSettings, 1.0F);
		assert(victim.current > 40.0F);
		victim.current = 40.0F;
		victim.NoteSapped("Absorb");
		assert(victim.debuffs.Has(DebuffId::sapped) && victim.debuffs.Any());
		assert(ResolveDebuffEffects(pulseSettings, victim.debuffs).active[0]);
		victim.Advance(pulseSettings, 1.0F);
		assert(Near(victim.current, 40.0F));
		// a rescan finds nothing (zero-duration enchantment) yet the pulse still holds
		victim.SetDebuffs(PoiseDebuffs{});
		assert(victim.debuffs.Has(DebuffId::sapped) && victim.debuffs.source[0] == "Absorb");
		victim.Advance(pulseSettings, 0.5F);
		assert(Near(victim.current, 40.0F));
		// the pulse ends after about two seconds
		victim.Advance(pulseSettings, 1.0F);
		assert(!victim.debuffs.Has(DebuffId::sapped) && !victim.debuffs.Any());
		victim.Advance(pulseSettings, 1.0F);
		assert(victim.current > 40.0F);
		// toggles gate it
		victim.NoteSapped("Absorb");
		Settings off = pulseSettings;
		off.enableSappedDebuff = false;
		assert(!ResolveDebuffEffects(off, victim.debuffs).active[0]);
		off = pulseSettings;
		off.enablePoiseDebuffs = false;
		assert(!ResolveDebuffEffects(off, victim.debuffs).active[0]);
		// re-hitting extends but never shortens, and Reset clears
		victim.Advance(pulseSettings, 1.5F);
		victim.NoteSapped("Absorb");
		assert(victim.debuffs.sappedPulse > 1.9F);
		victim.Reset();
		assert(!victim.debuffs.Has(DebuffId::sapped));
		// a lasting effect keeps Sapped after the pulse ends
		EffectFacts drain = Hostile();
		drain.kind = EffectKind::absorb;
		victim.SetMaximum(100.0F);
		victim.SetDebuffs(With(drain, "Vampiric Drain"));
		victim.NoteSapped("Absorb");
		victim.Advance(pulseSettings, 3.0F);
		assert(victim.debuffs.Has(DebuffId::sapped) && victim.debuffs.source[0] == "Vampiric Drain");
	}

	// Same-type effects never stack: one flag, the first source, and the strongest poison wins.
	PoiseDebuffs twice;
	AccumulateDebuff(twice, absorb, "Vampiric Drain");
	absorb.selfCast = false;
	AccumulateDebuff(twice, absorb, "Vampiric Drain");
	AccumulateDebuff(twice, absorb, "Absorb Health");
	assert(twice.Has(DebuffId::sapped) && twice.source[0] == "Vampiric Drain");
	PoiseDebuffs poisons;
	poison.magnitude = 3.0F;
	AccumulateDebuff(poisons, poison, "Weak Poison");
	poison.magnitude = 9.0F;
	AccumulateDebuff(poisons, poison, "Deadly Poison");
	poison.magnitude = 5.0F;
	AccumulateDebuff(poisons, poison, "Mid Poison");
	assert(Near(poisons.poisonMagnitude, 9.0F) && poisons.source[1] == "Deadly Poison");
	// Different types coexist on one actor.
	PoiseDebuffs all;
	for (const auto* facts : { &absorb, &poison, &frost, &fire, &bleed }) {
		EffectFacts copy = *facts;
		copy.hostile = true;
		copy.selfCast = false;
		copy.duration = 4.0F;
		copy.healthDamage = true;
		AccumulateDebuff(all, copy, "x");
	}
	assert(all.Has(DebuffId::sapped) && all.Has(DebuffId::enfeebled) && all.Has(DebuffId::numbed) &&
		all.Has(DebuffId::scorched) && all.Has(DebuffId::hemorrhaging));

	// Bleed keywords: comma separated, case-insensitive, substring, blanks ignored.
	bleed.hostile = true;
	assert(MatchesKeywordList("bleed,hemorrhag", "Bleeding Out"));
	assert(MatchesKeywordList("bleed,hemorrhag", "MOD_HEMORRHAGE_DoT"));
	assert(MatchesKeywordList(" Bleed , ,Wound", "deep wound"));
	assert(!MatchesKeywordList("bleed,hemorrhag", "Damage Health"));
	assert(!MatchesKeywordList("", "bleed") && !MatchesKeywordList(" , ", "bleed"));
	assert(MatchesKeywordList("bleed", "BLEED"));

	// Resolution is neutral with nothing active, with the master switch off, and per debuff toggle.
	DebuffEffects none = ResolveDebuffEffects(settings, PoiseDebuffs{});
	assert(Near(none.ceilingRatio, 1.0F) && Near(none.regenRateMult, 1.0F) && !none.sapped &&
		Near(none.delayBonus, 0.0F) && Near(none.scorchedMeleeMult, 1.0F));
	assert(Near(ResolveDebuffEffects(settings, all).scorchedMeleeMult, 1.2F));
	Settings off = settings;
	off.enablePoiseDebuffs = false;
	const auto masterOff = ResolveDebuffEffects(off, all);
	assert(Near(masterOff.ceilingRatio, 1.0F) && Near(masterOff.regenRateMult, 1.0F) && !masterOff.sapped &&
		Near(masterOff.delayBonus, 0.0F) && Near(masterOff.scorchedMeleeMult, 1.0F));
	assert(BuildDebuffList(off, all).empty());
	Settings partial = settings;
	partial.enableSappedDebuff = false;
	partial.enableNumbedDebuff = false;
	const auto partialFx = ResolveDebuffEffects(partial, all);
	assert(!partialFx.sapped && Near(partialFx.delayBonus, 0.0F) && Near(partialFx.regenRateMult, 0.65F));
	assert(BuildDebuffList(partial, all).size() == 3);

	// Enfeebled ceiling scales linearly with poison magnitude per second and never goes below the global floor.
	auto ceilingFor = [&](const Settings& config, float magnitude) {
		PoiseDebuffs debuffs;
		EffectFacts facts = Hostile();
		facts.resist = EffectResist::poison;
		facts.magnitude = magnitude;
		AccumulateDebuff(debuffs, facts, "Poison");
		return ResolveDebuffEffects(config, debuffs).ceilingRatio;
	};
	assert(Near(ceilingFor(settings, 0.0F), 0.85F) && Near(ceilingFor(settings, 5.0F), 0.775F));
	assert(Near(ceilingFor(settings, 10.0F), 0.70F) && Near(ceilingFor(settings, 500.0F), 0.70F));
	assert(Near(ceilingFor(settings, -4.0F), 0.85F));
	Settings strict = settings;
	strict.enfeebledRegenCeilingMin = 0.40F;
	strict.minGlobalRegenCeiling = 0.60F;
	assert(Near(ceilingFor(strict, 10.0F), 0.60F));
	Settings swapped = settings;
	swapped.enfeebledRegenCeilingMin = 0.85F;
	swapped.enfeebledRegenCeilingMax = 0.70F;
	assert(Near(ceilingFor(swapped, 10.0F), 0.70F) && Near(ceilingFor(swapped, 0.0F), 0.85F));

	// Sapped stops recovery completely and deals no poise damage.
	Settings quick = settings;
	quick.regenDelaySeconds = 0.0F;
	State sapped = Ready(quick);
	sapped.current = 20.0F;
	sapped.SetDebuffs(With(absorb));
	sapped.Advance(quick, 10.0F);
	assert(Near(sapped.current, 20.0F));
	sapped.SetDebuffs({});
	sapped.Advance(quick, 1.0F);
	assert(Near(sapped.current, 28.0F));

	// Hemorrhaging scales the regen rate and keeps the boss tier multiplier.
	State bleeding = Ready(quick);
	bleeding.current = 20.0F;
	bleeding.SetDebuffs(With(bleed));
	bleeding.Advance(quick, 1.0F);
	assert(Near(bleeding.current, 20.0F + 8.0F * 0.65F));
	State boss = Ready(quick);
	boss.ApplyTier(quick, CreatureTier::Boss);
	boss.current = 20.0F;
	boss.SetDebuffs(With(bleed));
	boss.Advance(quick, 1.0F);
	assert(Near(boss.current, 20.0F + 8.0F * 1.5F * 0.65F));

	// Enfeebled only caps upward recovery; poise above the ceiling is never reduced.
	State poisoned = Ready(quick);
	poisoned.current = 30.0F;
	poison.magnitude = 10.0F;
	poisoned.SetDebuffs(With(poison));
	poisoned.Advance(quick, 100.0F);
	assert(Near(poisoned.current, 70.0F) && Near(poisoned.maximum, 100.0F));
	poisoned.current = 95.0F;
	poisoned.Advance(quick, 100.0F);
	assert(Near(poisoned.current, 95.0F));
	poisoned.current = 99.0F;
	poisoned.SetDebuffs({});
	poisoned.Advance(quick, 100.0F);
	assert(Near(poisoned.current, 100.0F));
	// A poise break remains empty through immunity and retains post-break surge state.
	State broken = Ready(quick, 10.0F);
	broken.SetPlayerTarget(true);
	poison.magnitude = 10.0F;
	broken.SetDebuffs(With(poison));
	broken.current = 1.0F;
	assert(broken.ApplyDamage(quick, 50.0F).broke && Near(broken.current, 0.0F));
	assert(broken.recoveringFromBreak);
	broken.Advance(quick, quick.staggerImmunitySeconds);
	assert(Near(broken.current, 0.0F));
	broken.Advance(quick, 0.1F);
	assert(Near(broken.current, 0.0F + 8.0F * kPostBreakRegenMultiplier * 0.1F));

	State npcBroken = Ready(quick, 10.0F);
	npcBroken.SetPlayerTarget(false);
	npcBroken.SetDebuffs(With(poison));
	npcBroken.current = 1.0F;
	assert(npcBroken.ApplyDamage(quick, 50.0F).broke);
	assert(Near(npcBroken.current, npcBroken.maximum));
	assert(!npcBroken.recoveringFromBreak);

	// Debuffs and the cap combine: poisoned and bleeding recovers slower toward the ceiling.
	State both = Ready(quick);
	both.current = 30.0F;
	PoiseDebuffs poisonedBleeding = With(poison);
	AccumulateDebuff(poisonedBleeding, bleed, "Bleed");
	both.SetDebuffs(poisonedBleeding);
	both.Advance(quick, 1.0F);
	assert(Near(both.current, 30.0F + 8.0F * 0.65F));
	both.Advance(quick, 100.0F);
	assert(Near(both.current, 70.0F));

	// Numbed lengthens the regen delay assigned on a hit.
	State chilled = Ready(settings);
	frost.healthDamage = true;
	frost.duration = 4.0F;
	chilled.SetDebuffs(With(frost));
	chilled.ApplyDamage(settings, 5.0F);
	assert(Near(chilled.regenDelay, 4.0F));
	State plain = Ready(settings);
	plain.ApplyDamage(settings, 5.0F);
	assert(Near(plain.regenDelay, 3.0F));
	chilled.regenDelay = 0.0F;
	chilled.immunity = 0.0F;
	Settings noNumb = settings;
	noNumb.enableNumbedDebuff = false;
	chilled.ApplyDamage(noNumb, 5.0F);
	assert(Near(chilled.regenDelay, 3.0F));

	// Scorched multiplies non-projectile physical hits only; magic never routes through Apply.
	EffectFacts burn = Hostile();
	burn.resist = EffectResist::fire;
	burn.healthDamage = true;
	burn.duration = 5.0F;
	auto damageOf = [&](Hit hit, bool scorched, const Settings& config, HitBreakdown* out = nullptr) {
		State victim;
		victim.SetMaximum(1000.0F);
		if (scorched) {
			victim.SetDebuffs(With(burn));
		}
		victim.Apply(config, hit, out);
		return 1000.0F - victim.current;
	};
	Hit sword;
	sword.weaponWeight = 10.0F;
	const float base = damageOf(sword, false, settings);
	HitBreakdown tags;
	assert(Near(damageOf(sword, true, settings, &tags), base * 1.2F));
	bool tagged = false;
	for (std::size_t i = 0; i < tags.tagCount; ++i) {
		tagged = tagged || (std::string(tags.tags[i].label) == "Scorched" && Near(tags.tags[i].factor, 1.2F));
	}
	assert(tagged);
	Hit bash = sword;
	bash.type = HitType::bash;
	assert(Near(damageOf(bash, true, settings), damageOf(bash, false, settings) * 1.2F));
	Hit bite;
	bite.type = HitType::creature;
	assert(Near(damageOf(bite, true, settings), damageOf(bite, false, settings) * 1.2F));
	Hit arrow = sword;
	arrow.type = HitType::projectile;
	assert(Near(damageOf(arrow, true, settings), damageOf(arrow, false, settings)));
	HitBreakdown arrowTags;
	damageOf(arrow, true, settings, &arrowTags);
	for (std::size_t i = 0; i < arrowTags.tagCount; ++i) {
		assert(std::string(arrowTags.tags[i].label) != "Scorched");
	}
	assert(Near(damageOf(sword, true, off), base));
	Settings noBurn = settings;
	noBurn.enableScorchedDebuff = false;
	assert(Near(damageOf(sword, true, noBurn), base));

	// Debuffs never change the maximum poise formula.
	ArmorProfile armor;
	armor.weight = 20.0F;
	const auto maxBefore = ComputeMaximumPoise(settings, CreatureTier::Standard, armor, FleshRating{}, 1.0F, true).maximum;
	State staysTheSame = Ready(settings, maxBefore);
	staysTheSame.SetDebuffs(all);
	assert(Near(staysTheSame.maximum, maxBefore));
	staysTheSame.Reset();
	assert(!staysTheSame.debuffs.Any());

	// Pills carry fixed ids, labels, live details and the source effect name; the actor JSON includes them.
	PoiseDebuffs sources;
	AccumulateDebuff(sources, absorb, "Vampiric Drain");
	poison.magnitude = 4.0F;
	AccumulateDebuff(sources, poison, "Deadly Poison");
	AccumulateDebuff(sources, frost, "Frostbite");
	AccumulateDebuff(sources, bleed, "Wounds");
	AccumulateDebuff(sources, burn, "Burning");
	const auto list = BuildDebuffList(settings, sources);
	assert(list.size() == 5);
	assert(list[0].id == "sapped" && list[0].label == "SAPPED" && list[0].detail == "0.0/s regen" && list[0].source == "Vampiric Drain");
	assert(list[1].id == "enfeebled" && list[1].detail == "Cap 79%" && list[1].source == "Deadly Poison");
	assert(list[2].id == "numbed" && list[2].detail == "+1.0s delay");
	assert(list[3].id == "hemorrhaging" && list[3].detail == "0.65x regen" && list[3].source == "Wounds");
	assert(list[4].id == "scorched" && list[4].detail == "1.20x melee taken");
	TelemetrySnapshot snapshot;
	snapshot.player.debuffs = list;
	snapshot.player.ceilingRatio = ResolveDebuffEffects(settings, sources).ceilingRatio;
	const std::string json = TelemetryJson(snapshot);
	assert(json.find("\"ceilingRatio\":0.790") != std::string::npos);
	assert(json.find("{\"id\":\"enfeebled\",\"label\":\"ENFEEBLED\",\"detail\":\"Cap 79%\",\"source\":\"Deadly Poison\"}") != std::string::npos);

	// Defaults match the specification.
	assert(settings.enablePoiseDebuffs && settings.enableSappedDebuff && settings.enableEnfeebledDebuff &&
		settings.enableNumbedDebuff && settings.enableHemorrhagingDebuff && settings.enableScorchedDebuff);
	assert(Near(settings.enfeebledRegenCeilingMin, 0.70F) && Near(settings.enfeebledRegenCeilingMax, 0.85F) &&
		Near(settings.enfeebledMagnitudeForMin, 10.0F) && Near(settings.numbedRegenDelayBonus, 1.0F) &&
		Near(settings.hemorrhagingRegenRateMultiplier, 0.65F) && Near(settings.scorchedMeleeDamageMultiplier, 1.20F) &&
		Near(settings.minGlobalRegenCeiling, 0.60F));
	assert(settings.hemorrhagingKeywords == "bleed,hemorrhag");
	assert(Near(settings.basePoise, 40.0F));
}
