const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const root = path.join(__dirname, '..');
const read = (...parts) => fs.readFileSync(path.join(root, ...parts), 'utf8').replace(/\r\n/g, '\n');
const poise = read('src', 'builtin_poise.cpp');
const state = read('src', 'builtin_poise_state.h');
const main = read('src', 'main.cpp');
const menu = read('src', 'menu_framework.cpp');
const ini = read('poisesystemhud.ini');
const diagnostic = read('src', 'stagger_diagnostic.cpp');
const poiseIni = ini.split('[Poise]')[1];

const defaults = {
    bEnableCreaturePoise: ['true', 'enableCreaturePoise', 'get_enableCreaturePoise'],
    fLightCreaturePoiseMultiplier: ['0.75', 'lightCreaturePoiseMultiplier', 'get_lightCreaturePoiseMultiplier'],
    fMediumCreaturePoiseMultiplier: ['1.7', 'mediumCreaturePoiseMultiplier', 'get_mediumCreaturePoiseMultiplier'],
    fHeavyCreaturePoiseMultiplier: ['3.75', 'heavyCreaturePoiseMultiplier', 'get_heavyCreaturePoiseMultiplier'],
    fBossCreaturePoiseMultiplier: ['6.0', 'bossCreaturePoiseMultiplier', 'get_bossCreaturePoiseMultiplier'],
    fBossCreatureRegenMultiplier: ['1.5', 'bossCreatureRegenMultiplier', 'get_bossCreatureRegenMultiplier'],
    fLightCreatureDamageMultiplier: ['0.6', 'lightCreatureDamageMultiplier', 'get_lightCreatureDamageMultiplier'],
    fMediumCreatureDamageMultiplier: ['1.2', 'mediumCreatureDamageMultiplier', 'get_mediumCreatureDamageMultiplier'],
    fHeavyCreatureDamageMultiplier: ['1.375', 'heavyCreatureDamageMultiplier', 'get_heavyCreatureDamageMultiplier'],
    fBossCreatureDamageMultiplier: ['2.25', 'bossCreatureDamageMultiplier', 'get_bossCreatureDamageMultiplier'],
    fCreatureStaggerImmunitySeconds: ['2.0', 'creatureStaggerImmunitySeconds', 'get_creatureStaggerImmunitySeconds'],
    fSummonPoiseMultiplier: ['1.5', 'summonPoiseMultiplier', 'get_summonPoiseMultiplier'],
    fNPCMagicPoiseMultiplier: ['0.6', 'npcMagicPoiseMultiplier', 'get_npcMagicPoiseMultiplier'],
    fShieldBashMultiplier: ['1.0', 'shieldBashMultiplier', 'get_shieldBashMultiplier'],
    fWeaponBashMultiplier: ['0.35', 'weaponBashMultiplier', 'get_weaponBashMultiplier'],
    fShieldBlockedMagicMultiplier: ['0.4', 'shieldBlockedMagicMultiplier', 'get_shieldBlockedMagicMultiplier'],
    fWeaponBlockedMagicMultiplier: ['0.7', 'weaponBlockedMagicMultiplier', 'get_weaponBlockedMagicMultiplier'],
    fShieldBlockedPhysicalMultiplier: ['0.25', 'shieldBlockedPhysicalMultiplier', 'get_shieldBlockedPhysicalMultiplier'],
    fWeaponBlockedPhysicalMultiplier: ['0.45', 'weaponBlockedPhysicalMultiplier', 'get_weaponBlockedPhysicalMultiplier'],
};

test('creature settings are shipped, loaded, exported, shown in the menu and reset', () => {
    for (const [key, [value, member, getter]] of Object.entries(defaults)) {
        assert.match(poiseIni, new RegExp(`^${key}=${value.replace('.', '\\.')}$`, 'm'));
        assert.match(main, new RegExp(`L"${key}"[^\\n]*poiseSettings\\.${member}[^\\n]*L"Poise"`));
        assert.match(main, new RegExp(`${getter}\\(\\) \\{ return &poiseSettings\\.${member}; \\}`));
        assert.match(menu, new RegExp(`${getter}\\(\\);`));
        assert.match(menu, new RegExp(`QueueIniWrite\\("${key}", [^)]*"Poise"\\)`));
        assert.match(menu, new RegExp(`QueueIniWrite\\("${key}", "${value.replace('.', '\\.')}", "Poise"\\)`));
    }
    assert.match(menu, /Checkbox\("Enable Creature Poise##poise"/);
    assert.match(state, /float lightCreaturePoiseMultiplier = 0\.75F;/);
    assert.match(state, /float mediumCreaturePoiseMultiplier = 1\.7F;/);
    assert.match(state, /float heavyCreaturePoiseMultiplier = 3\.75F;/);
    assert.match(state, /float bossCreaturePoiseMultiplier = 6\.0F;/);
    assert.match(state, /float bossCreatureRegenMultiplier = 1\.5F;/);
    assert.match(state, /bool enableCreaturePoise = true;/);
});

test('creature multiplier defaults to 0.8 everywhere', () => {
    assert.match(state, /float creatureMultiplier = 0\.8F;/);
    assert.match(poiseIni, /^fCreatureMultiplier=0\.8$/m);
    assert.match(menu, /QueueIniWrite\("fCreatureMultiplier", "0\.8", "Poise"\)/);
});

test('creature hits are classified by attacker keyword before any weapon path', () => {
    const classify = poise.slice(poise.indexOf('bool ClassifyHit('), poise.indexOf('void ProcessHit('));
    const creatureBranch = classify.indexOf('!aggressor->HasKeywordString("ActorTypeNPC")');
    assert.ok(creatureBranch > 0);
    assert.ok(creatureBranch < classify.indexOf('RE::HitData::Flag::kBash'));
    assert.ok(creatureBranch < classify.indexOf('} else if (weapon) {'));
    assert.ok(creatureBranch > classify.indexOf('HitType::projectile'));
    assert.match(classify.slice(creatureBranch, creatureBranch + 300), /HitType::creature;\s*weapon = nullptr;/);
});

test('creature tiers gate tracking and suppression through one resolver', () => {
    assert.match(poise, /BuildRaceTable\(\);[\s\S]*SKSE::GetTrampoline\(\)\.write_call<5>\(hitSite, ProcessHit\)/);
    assert.match(poise, /TrackedTier\(RE::Actor\* actor\)/);
    assert.match(poise, /ResolveActorTier\(raceTier, npc, actor->GetName\(\)/);
    assert.doesNotMatch(poise, /EligibleNPC/);
    assert.match(poise, /gateCreature\.store\(npcInstalled && configuration\.enableCreaturePoise/);
    assert.match(poise, /\*tier == CreatureTier::Standard \? gateNpc\.load\([^)]*\) :\s*gateCreature\.load/);
    assert.match(poise, /configuration\.enableCreaturePoise != settings\.enableCreaturePoise/);
    assert.match(poise, /IsFlying\(\)/);
    assert.equal((state.match(/TierBasePoise\(settings, tier\)/g) || []).length >= 1, true);
    assert.equal((poise.match(/ApplyTier\(configuration, /g) || []).length >= 3, true);
});

test('boss breaks use the hardcoded full-strength stagger magnitude', () => {
    assert.match(state, /constexpr float kBossMinimumBreakMagnitude = 1\.0F;/);
    assert.match(state, /std::clamp\(damage \/ maximum, minimumBreakMagnitude, 1\.0F\)/);
    assert.doesNotMatch(ini, /BossMinimum|MinimumBreak|MinimumStagger/i);
});

test('stagger diagnostics report the race and poise tier of both actors', () => {
    assert.match(diagnostic, /BuiltinPoise::DescribeActorClass\(actor\)/);
    assert.match(diagnostic, /target=\[\{\}\] aggressor=\[\{\}\]/);
    assert.match(poise, /std::string BuiltinPoise::DescribeActorClass\(RE::Actor\* actor\)/);
});

test('tracked entries catch up on the gameplay clock instead of relying on the per-actor update hook', () => {
    assert.match(poise, /double lastAdvance = -1\.0;/);
    assert.match(poise, /void CatchUp\(NPCState& entry\)/);
    assert.match(poise, /gameplaySeconds - entry\.lastAdvance/);
    assert.match(poise, /CatchUp\(entry\);\s*(?:#ifdef ARPGHUD_STAGGER_DIAGNOSTICS[\s\S]*?#endif\s*)?result = entry\.poise\.Apply\(/);
    assert.match(poise, /CatchUp\(it->second\);\s*(?:#ifdef ARPGHUD_STAGGER_DIAGNOSTICS[\s\S]*?#endif\s*)?result = it->second\.poise\.Apply\(/);
    assert.match(poise, /CatchUp\(entry\);\s*result = entry\.poise\.ApplyDamage\(/);
    assert.doesNotMatch(poise, /it->second\.poise\.Advance\(/);
});

const magic = read('src', 'magic_gameplay.cpp');
const html = read('view', 'index.html');
const css = read('view', 'minimal.css');
const js = read('view', 'minimal.js');
const readme = read('README.md');

test('shield and weapon blocks use separate multipliers and the legacy key is gone', () => {
    assert.doesNotMatch(state, /blockedDamageMultiplier/);
    assert.doesNotMatch(main, /fBlockedDamageMultiplier/);
    assert.doesNotMatch(menu, /fBlockedDamageMultiplier|get_blockedDamageMultiplier/);
    assert.doesNotMatch(ini, /fBlockedDamageMultiplier/);
    assert.match(state, /hit\.shieldBlocked \? settings\.shieldBlockedPhysicalMultiplier :\s*settings\.weaponBlockedPhysicalMultiplier/);
    const classify = poise.slice(poise.indexOf('bool ClassifyHit('), poise.indexOf('void ProcessHit('));
    assert.match(classify, /hit\.shieldBlocked = true;/);
    assert.match(menu, /Slider[^\n]*Shield Blocked Physical Mult##poise/);
    assert.match(menu, /Slider[^\n]*Weapon Blocked Physical Mult##poise/);
});

test('creature classification, damage tiers, immunity and summons are wired into the hit path', () => {
    assert.match(poise, /BuiltinPoise::ResolveActorTier\(raceTier, npc, actor->GetName\(\)/);
    assert.match(poise, /hit\.attackerTier = hit\.type == BuiltinPoise::HitType::creature \? ClassifyActor\(aggressor\)/);
    assert.match(state, /const float tierMultiplier = CreatureDamageTierMultiplier\(settings, hit\.attackerTier\);\s*damage \*= settings\.creatureMultiplier \* tierMultiplier;/);
    assert.match(state, /creatureImmunity \? settings\.creatureStaggerImmunitySeconds : settings\.staggerImmunitySeconds/);
    assert.match(poise, /actor->IsCommandedActor\(\) \? settings\.summonPoiseMultiplier : 1\.0F/);
    assert.match(poise, /ComputeMaximumPoise\(settings, tier, armor, flesh, SummonScale\(settings, actor\), !actor \|\| actor->IsPlayerRef\(\)\)/);
    assert.equal((poise.match(/[^:]MaximumPoise\(configuration/g) || []).length >= 4, true);
});

test('creature refresh and cleanup run from the player update instead of the per-actor hook', () => {
    const playerUpdate = poise.slice(poise.indexOf('void PlayerUpdate('), poise.indexOf('bool ClassifyHit('));
    assert.match(playerUpdate, /creatureRefreshTimer -= delta;/);
    assert.match(playerUpdate, /RefreshCreatureEntries\(\);/);
    const refresh = poise.slice(poise.indexOf('void RefreshCreatureEntries('), poise.indexOf('void PlayerUpdate('));
    assert.match(refresh, /IsCreatureTier\(entry\.tier\)/);
    assert.match(refresh, /ActiveFlesh\(actor\.get\(\)\)/);
    assert.match(refresh, /npcStates\.erase\(it\)/);
});

test('magic from any caster, cloaks and explosions reach poise with the NPC multiplier', () => {
    assert.doesNotMatch(magic, /ActorTypeNPC/);
    assert.match(magic, /MakeProjectilelessImpact\(target, data\)/);
    assert.match(magic, /kInstant && !data->areaTarget/);
    assert.match(magic, /EffectArchetype::kDualValueModifier/);
    assert.match(magic, /const bool npcCaster = !attacker->IsPlayerRef\(\);\s*if \(npcCaster\) \{\s*damage \*= settings\.npcMagicPoiseMultiplier;/);
    assert.match(magic, /RecentlyHit\(impact\.spell, targetHandle\)/);
    assert.match(state, /float wardMagicDamageMultiplier = 0\.35F;/);
    assert.match(state, /float npcMagicPoiseMultiplier = 0\.6F;/);
});

test('target poise bar is configurable, tracked by the backend and rendered upper-right', () => {
    assert.match(read('poisesystemhud.ini').split('[Poise]')[0], /^bShowTargetPoiseBar=false$/m);
    assert.match(main, /L"bShowTargetPoiseBar"[^\n]*settings\.showTargetPoiseBar/);
    assert.match(main, /get_showTargetPoiseBar\(\) \{ return &settings\.showTargetPoiseBar; \}/);
    assert.match(menu, /Checkbox\("Show Target Poise Bar##widgets"/);
    assert.match(menu, /QueueIniWrite\("bShowTargetPoiseBar", val, "HUD"\)/);
    assert.match(menu, /QueueIniWrite\("bShowTargetPoiseBar", "false", "HUD"\)/);
    assert.match(poise, /constexpr double kTargetBarSeconds = 4\.0;/);
    assert.match(poise, /bool BuiltinPoise::ReadTarget\(TargetValues& values\)/);
    assert.match(main, /BuiltinPoise::ReadTarget\(target\)/);
    assert.match(main, /updateTargetPoise\(true, \{\}, \{\}, \{:\.1f\}, \{:\.1f\}, \{\}\)/);
    assert.match(html, /id="target-poise"/);
    assert.match(css, /#target-poise \{[^}]*top: 96px; right: 28px/);
    assert.match(css, /#target-poise\.show \{ opacity: 1; \}/);
    assert.match(js, /function updateTargetPoise\(visible, name, tier, current, max, immune\)/);
    assert.match(js, /`\$\{name\} \[\$\{tier\}\]`/);
    assert.match(js, /current\.toFixed\(1\)\} \/ \$\{max\.toFixed\(1\)\}/);
});

test('readme documents the creature polish settings', () => {
    for (const key of ['fCreatureStaggerImmunitySeconds', 'fSummonPoiseMultiplier', 'fNPCMagicPoiseMultiplier',
        'fShieldBlockedPhysicalMultiplier', 'fWeaponBlockedPhysicalMultiplier', 'bShowTargetPoiseBar']) {
        assert.match(readme, new RegExp(key));
    }
});
