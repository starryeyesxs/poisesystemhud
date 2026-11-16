const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const source = fs.readFileSync(path.join(__dirname, '..', 'src', 'main.cpp'), 'utf8').replace(/\r\n/g, '\n');
const ini = fs.readFileSync(path.join(__dirname, '..', 'poisesystemhud.ini'), 'utf8');
const names = [
    'bEnableBuiltInPoise', 'bEnableNPCPoise', 'bSuppressVanillaStaggers', 'bEnableCreaturePoise',
    'fLightCreaturePoiseMultiplier', 'fMediumCreaturePoiseMultiplier', 'fHeavyCreaturePoiseMultiplier',
    'fBossCreaturePoiseMultiplier', 'fBossCreatureRegenMultiplier', 'fBasePoise', 'fArmorWeightMultiplier',
    'fShieldWeightPoiseMultiplier', 'bApplyShieldWeightToNPCs',
    'fBaseHitPoiseDamage', 'fWeaponWeightDamageMultiplier', 'fTwoHandedMultiplier',
    'fShieldBashMultiplier', 'fWeaponBashMultiplier', 'bAllowTimedBashStaggers', 'fCreatureMultiplier', 'fProjectileMultiplier',
    'fPowerAttackMultiplier', 'fShieldBlockedPhysicalMultiplier', 'fWeaponBlockedPhysicalMultiplier',
    'fUnblockedPhysicalMultiplier', 'fRegenDelaySeconds',
    'fLightCreatureDamageMultiplier', 'fMediumCreatureDamageMultiplier', 'fHeavyCreatureDamageMultiplier',
    'fBossCreatureDamageMultiplier', 'fCreatureStaggerImmunitySeconds', 'fSummonPoiseMultiplier',
    'fNPCMagicPoiseMultiplier',
    'fRegenRatePerSecond', 'fStaggerImmunitySeconds',
    'fPhysicalDamagePoiseMultiplier', 'fPhysicalDamagePoiseCap', 'fLightArmorRatingPoiseMultiplier',
    'fLightArmorRatingPoiseCap', 'fLightArmorFleshMultiplier', 'fHeavyArmorFleshMultiplier',
];

test('all poise tuning keys are shipped and reloaded through the Poise section', () => {
    const section = ini.split('[Poise]')[1];
    assert.ok(section);
    for (const name of names) {
        assert.match(section, new RegExp(`^${name}=`, 'm'));
        assert.match(source, new RegExp(`L"${name}"[^\\n]*L"Poise"`));
    }
    const reload = source.slice(source.indexOf('void ReloadHud()\n'), source.indexOf('void RegisterReloadKey()\n', source.indexOf('void ReloadHud()\n')));
    assert.match(reload, /LoadSettings\(\)/);
    assert.doesNotMatch(reload, /BuiltinPoise::Install/);
    assert.match(source, /BuiltinPoise::Configure\(poiseSettings\)/);
});

test('baseline progression keys are ranged, exported, slidered and reset', () => {
    const menu = fs.readFileSync(path.join(__dirname, '..', 'src', 'menu_framework.cpp'), 'utf8');
    const keys = [
        ['fPhysicalDamagePoiseMultiplier', 'physicalDamagePoiseMultiplier', '0.0F, 0.25F', '0.05', 'Physical Damage Poise Mult'],
        ['fPhysicalDamagePoiseCap', 'physicalDamagePoiseCap', '0.0F, 25.0F', '6.0', 'Physical Damage Poise Cap'],
        ['fLightArmorRatingPoiseMultiplier', 'lightArmorRatingPoiseMultiplier', '0.0F, 0.5F', '0.15', 'Light Armor Rating Poise Mult'],
        ['fLightArmorRatingPoiseCap', 'lightArmorRatingPoiseCap', '0.0F, 50.0F', '15.0', 'Light Armor Rating Poise Cap'],
        ['fLightArmorFleshMultiplier', 'lightArmorFleshMultiplier', '0.0F, 1.0F', '0.75', 'Light Armor Flesh Efficiency'],
        ['fHeavyArmorFleshMultiplier', 'heavyArmorFleshMultiplier', '0.0F, 1.0F', '0.45', 'Heavy Armor Flesh Efficiency'],
    ];
    const loadSettings = source.slice(source.indexOf('void LoadSettings()'), source.indexOf('bool HasValidView()'));
    for (const [key, field, range, value, label] of keys) {
        const getter = `get_${field}`;
        assert.match(loadSettings, new RegExp(`L"${key}", "${key}", poiseSettings\\.${field}, ${range.replace('.', '\\.')}, L"Poise"`));
        assert.doesNotMatch(source.slice(source.indexOf('#ifdef ARPGHUD_MAGIC_GAMEPLAY', source.indexOf('void LoadSettings()')),
            source.indexOf('BuiltinPoise::Configure(poiseSettings)')), new RegExp(key));
        assert.match(source, new RegExp(`float\\* ${getter}\\(\\) \\{ return &poiseSettings\\.${field}; \\}`));
        assert.match(menu, new RegExp(`float\\* ${getter}\\(\\);`));
        assert.match(menu, new RegExp(`SliderFloat\\("${label}##poise"`));
        assert.match(menu, new RegExp(`QueueIniWrite\\("${key}", "${value.replace('.', '\\.')}", "Poise"\\)`));
        assert.match(ini.split('[Poise]')[1], new RegExp(`^${key}=${value.replace('.', '\\.')}$`, 'm'));
    }
});

test('built-in poise is independent of view initialization and bridge remains a fallback', () => {
    const dataLoaded = source.slice(source.indexOf('case SKSE::MessagingInterface::kDataLoaded:'), source.indexOf('case SKSE::MessagingInterface::kPreLoadGame:'));
    assert.ok(dataLoaded.indexOf('BuiltinPoise::Install()') < dataLoaded.indexOf('RequestPluginAPI'));
    assert.match(source, /BuiltinPoise::Read\(values\)\s*\|\|\s*PoiseBridge::Read/);
    assert.match(source, /BuiltinPoise::BeginGame\(\)/);
    assert.match(source, /BuiltinPoise::EndGame\(\)/);
});

test('poise excludes downed actors without unsupported AE actor relocations', () => {
    const poise = fs.readFileSync(path.join(__dirname, '..', 'src', 'builtin_poise.cpp'), 'utf8');
    assert.doesNotMatch(poise, /->IsEssentialDown\s*\(|->IsInBleedout\s*\(/);
    assert.ok((poise.match(/->AsActorState\(\)->IsBleedingOut\(\)/g) || []).length >= 3);
    assert.match(poise, /downed-actor check: using runtime-adjusted ActorState life state/);
});

test('physical hit chaining preserves a validated prior destination', () => {
    const poise = fs.readFileSync(path.join(__dirname, '..', 'src', 'builtin_poise.cpp'), 'utf8');
    assert.match(poise, /previousHit = HookCall::ExecutableTarget\(hitSite\)/);
    assert.match(poise, /previousHit != 0 && previousHit != reinterpret_cast<std::uintptr_t>\(&ProcessHit\)/);
    assert.match(poise, /originalHit = reinterpret_cast<HitFunction>\(previousHit\)/);
    assert.ok(poise.indexOf('originalHit = reinterpret_cast<HitFunction>(previousHit)') <
        poise.indexOf('write_call<5>(hitSite, ProcessHit)'));
    assert.match(poise, /ABI and mod gameplay compatibility are not verified/);
});

test('poise update validation reports actual addresses and owning modules', () => {
    const poise = fs.readFileSync(path.join(__dirname, '..', 'src', 'builtin_poise.cpp'), 'utf8');
    assert.match(poise, /DescribeAddress\(std::uintptr_t address, std::uintptr_t executableBase\)/);
    assert.match(poise, /VirtualQuery\(reinterpret_cast<const void\*>\(address\)/);
    assert.match(poise, /GetModuleFileNameW\(moduleHandle/);
    assert.match(poise, /bool IsLoadedExecutableAddress\(std::uintptr_t address\)/);
    assert.match(poise, /memory\.Type != MEM_IMAGE/);
    assert.match(poise, /case PAGE_EXECUTE_READWRITE:/);
    assert.match(poise, /GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS \| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT/);
    assert.match(poise, /player update slot: vtable=0x\{:X\}, slot=0xAD, actual=0x\{:X\} \[\{\}\], executable-module validation \{\}/);
    assert.match(poise, /NPC update slot: vtable=0x\{:X\}, slot=0xAD, actual=0x\{:X\} \[\{\}\], executable-module validation \{\}/);
    assert.match(poise, /originalUpdate = reinterpret_cast<UpdateFunction>\(updateAddress\)/);
    assert.match(poise, /originalNPCUpdate = reinterpret_cast<NPCUpdateFunction>\(npcUpdateAddress\)/);
    assert.ok(poise.indexOf('originalUpdate = reinterpret_cast<UpdateFunction>(updateAddress)') <
        poise.indexOf('vtable.write_vfunc(0xAD, PlayerUpdate)'));
    assert.ok(poise.indexOf('originalNPCUpdate = reinterpret_cast<NPCUpdateFunction>(npcUpdateAddress)') <
        poise.indexOf('npcVtable.write_vfunc(0xAD, CharacterUpdate)'));
});

test('armor scan excludes shields from class and rating, uses base rating, and feeds every maximum-poise site', () => {
    const poise = fs.readFileSync(path.join(__dirname, '..', 'src', 'builtin_poise.cpp'), 'utf8');
    const scan = poise.slice(poise.indexOf('BuiltinPoise::ArmorProfile ScanArmor('), poise.indexOf('// Commanded actors'));
    assert.match(scan, /const bool shield = armor && armor->IsShield\(\)/);
    assert.match(scan, /!armor->IsLightArmor\(\) && !armor->IsHeavyArmor\(\) && !shield/);
    assert.match(scan, /shieldWeightTotal \+=/);
    assert.ok(scan.indexOf('if (shield) {\n\t\t\t\tcontinue;') < scan.indexOf('profile.hasHeavy = true'));
    assert.ok(scan.indexOf('if (shield) {\n\t\t\t\tcontinue;') < scan.indexOf('GetArmorRating()'));
    assert.match(scan, /armor->templateArmor->GetArmorRating\(\)/);
    assert.match(scan, /\(std::min\)\(rating, 500\.0F\)/);
    assert.match(scan, /\(std::min\)\(ratingTotal, 2000\.0F\)/);
    assert.doesNotMatch(poise, /GetActorValue\(RE::ActorValue::kDamageResist\)/);
    assert.doesNotMatch(poise, /ArmorWeight\(/);
    assert.equal((poise.match(/ScanArmor\(/g) || []).length, 7);
    assert.equal((poise.match(/[^:]MaximumPoise\(configuration/g) || []).length, 7);
    assert.match(poise, /BuiltinPoise::ComputeMaximumPoise\(settings, tier, armor, flesh, SummonScale\(settings, actor\), !actor \|\| actor->IsPlayerRef\(\)\)/);
    assert.match(poise, /hit\.physicalDamage = data->physicalDamage;/);
});

test('shield settings support negative values, menu persistence, live configuration and reset', () => {
    const menu = fs.readFileSync(path.join(__dirname, '..', 'src', 'menu_framework.cpp'), 'utf8');
    assert.match(source, /poiseSettings\.shieldWeightPoiseMultiplier, -1\.0F, 2\.0F, L"Poise"/);
    assert.match(source, /readBool\(L"bApplyShieldWeightToNPCs", "bApplyShieldWeightToNPCs", poiseSettings\.applyShieldWeightToNPCs, L"Poise"\)/);
    assert.match(source, /changedKey == "fShieldWeightPoiseMultiplier" \|\| changedKey == "bApplyShieldWeightToNPCs"\)\s*\{\s*BuiltinPoise::Configure\(poiseSettings\)/);
    assert.match(menu, /SliderFloat\("Shield Weight Poise Multiplier##poise", shieldWeightMult, -1\.0f, 2\.0f/);
    assert.match(menu, /std::round\(\*shieldWeightMult \* 20\.0f\) \/ 20\.0f/);
    assert.match(menu, /Checkbox\("Apply Shield Weight to NPCs##poise"/);
    assert.match(menu, /QueueIniWrite\("fShieldWeightPoiseMultiplier", "0\.0", "Poise"\)/);
    assert.match(menu, /QueueIniWrite\("bApplyShieldWeightToNPCs", "true", "Poise"\)/);
    assert.match(menu, /SendSettingsToHudImpl\("resetPoise"\)/);
    assert.match(ini, /^fShieldWeightPoiseMultiplier=0\.0$/m);
    assert.match(ini, /^bApplyShieldWeightToNPCs=true$/m);
});

test('maximum-poise math lives in the state header with efficiency priority and telemetry', () => {
    const state = fs.readFileSync(path.join(__dirname, '..', 'src', 'builtin_poise_state.h'), 'utf8');
    assert.match(state, /flesh\.armor \* settings\.fleshSpellPoiseMultiplier/);
    assert.match(state, /flesh\.dragonhide \? settings\.dragonhidePoiseBonus : 0\.0F/);
    const efficiency = state.slice(state.indexOf('inline float FleshEfficiency('), state.indexOf('inline MaximumBreakdown ComputeMaximumPoise('));
    assert.ok(efficiency.indexOf('armor.hasHeavy') < efficiency.indexOf('armor.hasLight'));
    assert.match(state, /float damage = settings\.baseHitDamage \+ weight \* settings\.weaponWeightDamageMultiplier \+ raw\.bonus;/);
    assert.match(state, /float rawBonus = 0\.0F;\s*bool rawCapped = false;/);
    const poise = fs.readFileSync(path.join(__dirname, '..', 'src', 'builtin_poise.cpp'), 'utf8');
    assert.match(poise, /bool BuiltinPoise::ReadTelemetry\(TelemetrySnapshot& snapshot\)/);
    assert.match(poise, /snapshot\.player = DescribePoise\(state, "Player", "Player"\)/);
    assert.match(poise, /fleshBonus = poise\.fleshBonusPoise/);
});
