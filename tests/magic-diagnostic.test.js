const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const root = path.join(__dirname, '..');
const diagnostic = fs.readFileSync(path.join(root, 'src', 'magic_diagnostic.cpp'), 'utf8');
const main = fs.readFileSync(path.join(root, 'src', 'main.cpp'), 'utf8');
const build = fs.readFileSync(path.join(root, 'xmake.lua'), 'utf8');

test('magic diagnostics are isolated in an explicit non-default build', () => {
    assert.match(build, /target\("arpghud_magic_diagnostic"\)[\s\S]*set_default\(false\)/);
    assert.match(build, /PLUGIN_VERSION \.\. "-magic-diagnostic"/);
    assert.match(main, /#ifdef ARPGHUD_MAGIC_DIAGNOSTICS\s+MagicDiagnostic::Install\(\);\s+#endif/);
    assert.match(diagnostic, /#ifdef ARPGHUD_MAGIC_DIAGNOSTICS/);
});

test('diagnostic hooks are runtime-gated, bounded and leave gameplay to the originals', () => {
    assert.match(diagnostic, /module\.version\(\) != SKSE::RUNTIME_SSE_1_7_104/);
    assert.match(diagnostic, /constexpr unsigned kLimit = 200/);
    assert.match(diagnostic, /std::memcmp/);
    assert.match(diagnostic, /if \(current != expected\)/);
    assert.match(diagnostic, /const bool result = originalAddTarget\(target, data\);/);
    assert.match(diagnostic, /using ImpactFunction = RE::Projectile::ImpactData\* \(\*\)/);
    assert.match(diagnostic, /RE::hkpCollidable\*, std::int32_t, bool\)/);
    assert.match(diagnostic, /static RE::Projectile::ImpactData\* Thunk/);
    assert.match(diagnostic, /auto\* result = original\(projectile, target, location, velocity, collidable, arg6, arg7\);/);
    assert.match(diagnostic, /currentImpactHandle = previousHandle;/);
    assert.match(diagnostic, /return result;/);
    assert.match(diagnostic, /casterProjectileHandle=.*enclosingImpactHandle=/);
    assert.match(diagnostic, /enclosingHitHandle=/);
    assert.match(diagnostic, /const bool result = original\(projectile, collidable\);/);
    assert.match(diagnostic, /currentHitHandle = previousHandle;/);
    assert.match(diagnostic, /InstallHitTrace<Flame>\(flame, module\.base\(\) \+ 0x8009C0\)/);
    assert.match(diagnostic, /enclosingProcessHandle=/);
    assert.match(diagnostic, /const bool result = original\(projectile\);/);
    assert.match(diagnostic, /currentProcessHandle = previousHandle;/);
    assert.match(diagnostic, /InstallProcessTrace<Flame>\(flame, module\.base\(\) \+ 0x7FA910\)/);
    assert.match(diagnostic, /InstallProcessTrace<Missile>\(missile, module\.base\(\) \+ 0x7F2AB0\)/);
    assert.match(diagnostic, /CaptureStackBackTrace/);
    assert.match(diagnostic, /count\.fetch_add\(1, std::memory_order_relaxed\) >= 8/);
    assert.match(diagnostic, /gameRVAs=/);
    assert.match(diagnostic, /spellCollisionHandle=.*spellCollisionTarget=/);
    assert.match(diagnostic, /REL::ID\(44205\)\.address\(\) \+ 0x1E1/);
    assert.match(diagnostic, /originalSpellCollision\(projectile, target, location, arg4, arg5\);/);
    assert.match(diagnostic, /currentSpellCollisionHandle = previousHandle;/);
    assert.match(diagnostic, /REL::ID\(44166\)\.address\(\) \+ 0x4DE/);
    assert.match(diagnostic, /const bool result = originalCollision\(projectile, target, location, velocity, layer, arg6, applied\);/);
    assert.match(diagnostic, /currentCollisionLayer = previousLayer;/);
    assert.match(diagnostic, /collision outcome:.*wardBefore=.*wardAfter=.*result=.*applied=/);
    assert.match(diagnostic, /resistAV=.*elementalResistance=.*magicResistance=/);
    assert.match(diagnostic, /wardBefore=.*wardAfter=/);
    assert.doesNotMatch(diagnostic, /DamageActorValue|RestoreActorValue|directionalStagger/);
});
