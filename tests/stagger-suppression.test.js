const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const root = path.join(__dirname, '..');
const read = (...parts) => fs.readFileSync(path.join(root, ...parts), 'utf8').replace(/\r\n/g, '\n');
const suppression = read('src', 'stagger_suppression.cpp');
const poise = read('src', 'builtin_poise.cpp');
const state = read('src', 'builtin_poise_state.h');
const main = read('src', 'main.cpp');
const menu = read('src', 'menu_framework.cpp');
const ini = read('TruePoise.ini');

test('suppression setting is shipped, loaded, shown in the menu and reset', () => {
    assert.match(ini.split('[Poise]')[1], /^bSuppressVanillaStaggers=true$/m);
    assert.match(state, /bool suppressVanillaStaggers = true;/);
    assert.match(main, /L"bSuppressVanillaStaggers"[^\n]*poiseSettings\.suppressVanillaStaggers[^\n]*L"Poise"/);
    assert.match(main, /get_poiseSuppressVanillaStaggers\(\) \{ return &poiseSettings\.suppressVanillaStaggers; \}/);
    assert.match(menu, /Checkbox\("Suppress Vanilla Staggers##poise"/);
    assert.match(menu, /QueueIniWrite\("bSuppressVanillaStaggers", val, "Poise"\)/);
    assert.match(menu, /QueueIniWrite\("bSuppressVanillaStaggers", "true", "Poise"\)/);
});

test('hit pushback is disabled by default and can be re-enabled through settings', () => {
    assert.match(ini.split('[Poise]')[1], /^bEnableHitPushBack=false$/m);
    assert.match(state, /bool enableHitPushBack = false;/);
    assert.match(main, /L"bEnableHitPushBack"[^\n]*poiseSettings\.enableHitPushBack[^\n]*L"Poise"/);
    assert.match(main, /get_poiseEnableHitPushBack\(\) \{ return &poiseSettings\.enableHitPushBack; \}/);
    assert.match(menu, /Checkbox\("Enable Hit Pushback##poise"/);
    assert.match(menu, /QueueIniWrite\("bEnableHitPushBack", val, "Poise"\)/);
    assert.match(menu, /QueueIniWrite\("bEnableHitPushBack", "false", "Poise"\)/);
    assert.match(poise, /if \(data && suppressHitPushBack\) \{\s*data->pushBack = 0\.0F;/);
});

test('baseline does not install the reverted downstream pushback hook', () => {
    assert.doesNotMatch(poise, /6CEEF3|ApplyHitPushBack|HitPushBackFunction|hitPushBackSite|ShouldSuppressHitPushBack/);
});

test('suppression installs after built-in poise and is gated on it', () => {
    const dataLoaded = main.slice(main.indexOf('case SKSE::MessagingInterface::kDataLoaded:'));
    assert.ok(dataLoaded.indexOf('BuiltinPoise::Install()') < dataLoaded.indexOf('StaggerSuppression::Install()'));
    assert.match(suppression, /module\.version\(\) != SKSE::RUNTIME_SSE_1_7_104/);
    assert.match(suppression, /!BuiltinPoise::IsInstalled\(\)/);
});

test('routine gate validates the prologue, whitelists shouts first and never calls through when suppressed', () => {
    assert.match(suppression, /REL::RelocationID\(36700, 37710\)/);
    assert.match(suppression, /0x48, 0x8B, 0xC4, 0x57, 0x48, 0x81, 0xEC, 0xC0, 0x00, 0x00, 0x00/);
    assert.match(suppression, /std::memcmp\(reinterpret_cast<const void\*>\(site\), prologue\.data\(\), prologue\.size\(\)\) != 0/);
    assert.match(suppression, /if \(suppressed\) \{[\s\S]*?return;\s+\}\s+originalStagger\(target, magnitude, aggressor\);/);
    assert.match(suppression, /staggerGate && disarmWhitelist &&\s+InstallRoutineGate/);
    assert.ok(suppression.indexOf('InstallStaggerEffectGate(staggerEffect)') <
        suppression.indexOf('InstallRoutineGate(REL::RelocationID'));
    assert.match(suppression, /StaggerGate::ScopedAllowStagger allowStagger;\s+original\(effect\);/);
    assert.match(suppression, /VTABLE_StaggerEffect/);
    assert.match(suppression, /VTABLE_DisarmEffect/);
});

test('StaggerEffect::Start suppresses melee-triggered stagger spells but keeps shouts whitelisted', () => {
    const gate = suppression.slice(suppression.indexOf('struct StaggerEffectGate'),
        suppression.indexOf('bool InstallStaggerEffectGate'));
    assert.match(gate, /static_cast<RE::StaggerEffect\*>\(effect\)/);
    assert.match(gate, /GetSpellType\(\) == RE::MagicSystem::SpellType::kVoicePower/);
    assert.match(gate, /ScopedAllowStagger allowStagger/);
    // The target must be resolved via GetTargetStatsObject()->As<Actor>() (never a raw effect->target cast),
    // because MagicTarget is a secondary base of Actor and GetTargetActor()'s reinterpret_cast is invalid.
    assert.match(gate, /stagger->target \? stagger->target->GetTargetStatsObject\(\) : nullptr/);
    assert.match(gate, /refr->As<RE::Actor>\(\)/);
    assert.doesNotMatch(gate, /GetTargetActor\(\)/);
    assert.match(gate, /actor && BuiltinPoise::ShouldSuppressStagger\(actor\)/);
    assert.match(gate, /LogSuppression\([^;]*"magic stagger"/);
    assert.match(gate, /stagger->Dispel\(false\)/);
    assert.match(gate, /return;/);
    assert.match(suppression, /InstallStaggerEffectGate\(staggerEffect\)/);
    assert.match(suppression, /vtable\.write_vfunc\(0x14, StaggerEffectGate::Thunk\)/);
    // DisarmEffect keeps the plain whitelist
    assert.match(suppression, /InstallEffectWhitelist<DisarmEffectTag>\(disarmEffect\)/);
});

test('ward break event is dropped only for suppressed actors and everything else is chained', () => {
    assert.match(suppression, /StaggerGate::IsWardBreakEvent\(event\.c_str\(\)\)/);
    assert.match(suppression, /BuiltinPoise::ShouldSuppressStagger\(actor\)\) \{[\s\S]*?return false;\s+\}\s+\}\s+return original\(holder, event\);/);
    assert.match(suppression, /table\.address\(\) == actual/);
});

test('suppression decision is lock-free and respects poise state, NPC setting and the allow guard', () => {
    assert.match(poise, /std::atomic<bool> gateActive/);
    assert.match(poise, /void PublishGate\(\)/);
    const decision = poise.slice(poise.indexOf('bool BuiltinPoise::ShouldSuppressStagger'), poise.indexOf('bool BuiltinPoise::Read'));
    assert.doesNotMatch(decision, /stateMutex|lock_guard/);
    assert.match(decision, /StaggerGate::ShouldSuppress\(true, managed, allowed\)/);
    assert.match(decision, /StaggerGate::Allowed\(\) \|\|\s*\(StaggerGate::InTimedBash\(\) && gateTimedBash\.load/);
    assert.match(decision, /gateNpc\.load/);
    assert.match(decision, /gateCreature\.load/);
    for (const fn of ['void BuiltinPoise::Configure', 'void BuiltinPoise::BeginGame', 'void BuiltinPoise::EndGame']) {
        const body = poise.slice(poise.indexOf(fn));
        assert.ok(body.slice(0, body.indexOf('\n}\n')).includes('PublishGate();'), `${fn} must publish the gate`);
    }
});

    test('timed-bash staggerStart events sent straight to the graph are dropped for suppressed actors only', () => {
        assert.match(suppression, /StaggerGate::IsWardBreakEvent\(event\.c_str\(\)\) \|\| StaggerGate::IsStaggerStartEvent\(event\.c_str\(\)\)/);
    const thunk = suppression.slice(suppression.indexOf('static bool Thunk(RE::IAnimationGraphManagerHolder* holder'));
    const body = thunk.slice(0, thunk.indexOf('return original(holder, event);'));
    assert.match(body, /BuiltinPoise::ShouldSuppressStagger\(actor\)/);
    assert.match(body, /LogSuppression\([^;]*"stagger event"/);
    assert.match(body, /return false;/);
    // Poise breaks and whitelisted shouts reach the event with the allow guard held, which ShouldSuppress honors.
    assert.match(poise, /StaggerGate::ScopedAllowStagger allowStagger;\s+directionalStagger\(/);
});

test('timed-bash stagger toggle is shipped, loaded, shown in the menu, reset and scoped to the vanilla hit call', () => {
    assert.match(ini.split('[Poise]')[1], /^bAllowTimedBashStaggers=false$/m);
    assert.match(state, /bool allowTimedBashStaggers = false;/);
    assert.match(main, /L"bAllowTimedBashStaggers"[^\n]*poiseSettings\.allowTimedBashStaggers[^\n]*L"Poise"/);
    assert.match(main, /get_allowTimedBashStaggers\(\) \{ return &poiseSettings\.allowTimedBashStaggers; \}/);
    assert.match(menu, /Checkbox\("Allow Timed Bash Staggers##poise"/);
    assert.match(menu, /QueueIniWrite\("bAllowTimedBashStaggers", val, "Poise"\)/);
    assert.match(menu, /QueueIniWrite\("bAllowTimedBashStaggers", "false", "Poise"\)/);
    assert.match(poise, /gateTimedBash\.store\(configuration\.allowTimedBashStaggers/);
        assert.match(poise, /kTimedBash\)\) \{\s*timedBash\.emplace\(\);\s*\}\s*originalHit\(target, data\);/);
});