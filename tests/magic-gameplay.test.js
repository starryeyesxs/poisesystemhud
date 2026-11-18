const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const root = path.join(__dirname, '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
const magic = read('src/magic_gameplay.cpp');
const poise = read('src/builtin_poise.cpp');
const main = read('src/main.cpp');
const menu = read('src/menu_framework.cpp');
const ini = read('poisesystemhud.ini');
const build = read('xmake.lua');

test('candidate has the requested settings and isolated package target', () => {
    const defaults = {
        fFleshSpellPoiseMultiplier: '0.45',
        fDragonhidePoiseBonus: '60.0',
        fWardMagicDamageMultiplier: '0.35',
        fShieldBlockedMagicMultiplier: '0.4',
        fWeaponBlockedMagicMultiplier: '0.7',
        fMagicBurstPoiseMultiplier: '1.20',
        fClothingMagicPoiseMultiplier: '0.70',
        fLightArmorMagicPoiseMultiplier: '1.00',
        fHeavyArmorMagicPoiseMultiplier: '1.15',
        fMagicStaggerPoiseMultiplier: '10.0',
        fConcentrationPoiseMultiplier: '1.0',
        fConcentrationIntervalSeconds: '1.0',
        fConcentrationMaxPoisePerInterval: '15.0',
    };
    for (const [name, value] of Object.entries(defaults)) {
        assert.match(ini, new RegExp(`^${name}=${value.replace('.', '\\.')}$`, 'm'));
        assert.match(main, new RegExp(`L"${name}"[^\\n]*L"Poise"`));
    }
    assert.match(build, /target\("arpghud_gameplay_candidate"\)[\s\S]*set_default\(false\)[\s\S]*ARPGHUD_MAGIC_GAMEPLAY/);
    assert.match(build, /target\(PLUGIN_NAME\)[\s\S]*?add_defines\("ARPGHUD_MAGIC_GAMEPLAY"\)[\s\S]*?target\("arpghud_magic_diagnostic"\)/);
    assert.match(build, /PLUGIN_VERSION \.\. "-candidate"/);
});

test('blocking scales magic poise in ApplyMagicDamage unless a ward already mitigated the hit', () => {
    assert.doesNotMatch(magic, /IsShieldBlocking|shieldBlocking|shieldMagicDamageMultiplier/);
    assert.match(magic, /BuiltinPoise::ApplyMagicDamage\(target\.get\(\), attacker\.get\(\), damage, impact\.ward, breakdownPtr\)/);
    assert.match(poise, /BlockKind CurrentBlock\(RE::Actor\* actor\)/);
    assert.match(poise, /!actor \|\| !actor->IsBlocking\(\)/);
    assert.match(poise, /shield && shield->IsShield\(\)\) \{\s*return BlockKind::shield;/);
    assert.match(poise, /settings\.shieldBlockedMagicMultiplier/);
    assert.match(poise, /settings\.weaponBlockedMagicMultiplier/);
    assert.match(poise, /const auto blockKind = wardMitigated \? BlockKind::none : CurrentBlock\(target\);/);
    assert.match(poise, /const float blockScale = BlockedMagicScale\(configuration, blockKind\);\s*damage \*= blockScale;/);
});
test('body and ward hooks forward original calls and queue only confirmed results', () => {
    assert.match(magic, /REL::ID\(44205\)\.address\(\) \+ 0x1E1/);
    assert.match(magic, /REL::ID\(44166\)\.address\(\) \+ 0x4DE/);
    assert.match(magic, /const bool accepted = originalAddTarget\(target, data\)/);
    assert.match(magic, /if \(accepted && impact\.target\)/);
    assert.match(magic, /originalSpellCollision\(projectile, target, location, arg4, arg5\)/);
    assert.match(magic, /const bool blocked = originalCollision\(projectile, target, location, velocity, layer, arg6, applied\)/);
    assert.match(magic, /if \(blocked && impact\.target && applied && !\*applied\)/);
    assert.match(magic, /kWardPower\) > 0\.0F/);
    assert.match(magic, /impact\.magnitude = effect->effectItem\.magnitude/);
    assert.match(magic, /impact\.magnitude = 8\.0F/);
    assert.match(magic, /tasks->AddTask\(\[impact\]/);
});

test('magic deduplication is projectile and target scoped; concentration is pooled', () => {
    assert.match(magic, /impact\.projectile\.native_handle\(\)/);
    assert.match(magic, /targets\.insert\(targetHandle\)/);
    assert.match(magic, /const auto targetHandle = impact\.target\.native_handle\(\)/);
    assert.match(magic, /!pair\.second\.handle\.get\(\)/);
    assert.match(magic, /damage\.Add\(damage, settings\.concentrationIntervalSeconds, settings\.concentrationMaxPoisePerInterval\)/);
    assert.match(magic, /damage\.Advance\(delta, settings\.concentrationMaxPoisePerInterval\)/);
    assert.match(magic, /BuiltinPoise::IsGenerationActive\(impact\.generation\)/);
    assert.match(magic, /input\.wardIntercepted = impact\.ward/);
    assert.match(magic, /input\.magicBurst = !impact\.concentration/);
    assert.match(magic, /impact\.projectileless && !impact\.concentration &&\s*impact\.magnitude < kMinimumProjectilelessBurstMagnitude/);
    assert.match(poise, /MagicArmorPoiseScale\(configuration, armor\)/);
    assert.match(poise, /damage \*= armorScale\.multiplier;\s*if \(!std::isfinite\(damage\) \|\| damage <= 0\.0F\)/);
    for (const key of ['fClothingMagicPoiseMultiplier', 'fLightArmorMagicPoiseMultiplier', 'fHeavyArmorMagicPoiseMultiplier']) {
        assert.match(main, new RegExp(`L"${key}"`));
        assert.match(menu, new RegExp(`QueueIniWrite\\("${key}"`));
    }
    assert.match(poise, /record\.Add\(armorScale\.label, armorScale\.multiplier\)/);
});

test('flesh bonus ignores inactive effects and physical damage has no ward multiplier', () => {
    assert.match(poise, /kInactive, RE::ActiveEffect::Flag::kDispelled/);
    assert.match(poise, /active->spell->GetFormID\(\) == 0xCDB70/);
    assert.match(poise, /effect->GetFormID\(\) == 0xCDB75/);
    const stateHeader = read('src/builtin_poise_state.h');
    assert.match(stateHeader, /flesh\.armor \* settings\.fleshSpellPoiseMultiplier/);
    assert.match(stateHeader, /flesh\.dragonhide \? settings\.dragonhidePoiseBonus/);
    const physical = read('src/builtin_poise_state.h').split('HitResult Apply(const Settings& settings, const Hit& hit, HitBreakdown* breakdown = nullptr)')[1].split('HitResult ApplyDamage')[0];
    assert.doesNotMatch(physical, /wardMagicDamageMultiplier/);
});
