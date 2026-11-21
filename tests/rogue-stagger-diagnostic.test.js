const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const root = path.join(__dirname, '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replace(/\r\n/g, '\n');

test('rogue stagger diagnostics are isolated in a separate uncapped target', () => {
    const build = read('xmake.lua');
    const cpp = read('src/rogue_stagger_diagnostic.cpp');
    assert.match(build, /target\("arpghud_rogue_stagger_diagnostic"\)[\s\S]*?set_default\(false\)[\s\S]*?set_targetdir\(path\.join\(os\.projectdir\(\), "build-rogue-diag"\)\)/);
    assert.match(build, /add_defines\("ARPGHUD_MAGIC_GAMEPLAY", "ARPGHUD_ROGUE_STAGGER_DIAGNOSTICS"\)/);
    assert.match(cpp, /#ifdef ARPGHUD_ROGUE_STAGGER_DIAGNOSTICS/);
    assert.doesNotMatch(cpp, /sample limit|TakeSample|fetch_add|cooldown/);
    assert.match(cpp, /std::ofstream file\(logPath, std::ios::app\)[\s\S]*?file\.flush\(\)/);
    assert.match(cpp, /RogueStagger\.log/);
    assert.match(cpp, /poise\.current <= 0\.0F/);
    assert.match(cpp, /StaggerGate::Allowed\(\)/);
});

test('rogue stagger diagnostics capture entry points, recent context, stack, and telemetry', () => {
    const cpp = read('src/rogue_stagger_diagnostic.cpp');
    const poise = read('src/builtin_poise.cpp');
    const magic = read('src/magic_gameplay.cpp');
    const js = read('view/minimal.js');
    assert.match(cpp, /Actor::TriggerStagger/);
    assert.match(cpp, /NotifyAnimationGraph/);
    assert.match(cpp, /kRecentWindowMs = 150/);
    assert.match(cpp, /staggerBefore/);
    assert.match(cpp, /explosionForce/);
    assert.match(cpp, /CaptureStackBackTrace\(2, kStackFrames/);
    assert.match(cpp, /kStackFrames = 128/);
    assert.match(poise, /RogueStaggerCapture::NotePhysicalHitComplete/);
    assert.match(magic, /RogueStaggerCapture::NoteMagicEffect/);
    assert.match(js, /function updateRogueStaggerAlert\(alert\)/);
});

test('rogue stagger diagnostics capture the complete player graph and physics post-hit window', () => {
    const cpp = read('src/rogue_stagger_diagnostic.cpp');
    assert.match(cpp, /kPostHitWindowMs = 250/);
    assert.match(cpp, /ReportPostHitGraphEvent\(target, name, "", "NotifyAnimationGraph", _ReturnAddress\(\)\)/);
    const notify = cpp.slice(cpp.indexOf('struct NotifyObserver'), cpp.indexOf('struct PlayerTag'));
    assert.ok(notify.indexOf('ReportPostHitGraphEvent') < notify.indexOf('IsStaggerStartEvent'));
    assert.match(cpp, /class PlayerGraphEventObserver final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>/);
    assert.match(cpp, /ReportPostHitGraphEvent\(target, event->tag\.c_str\(\), event->payload\.c_str\(\),\s*"ProcessEvent"/);
    assert.match(cpp, /GetEventSource<RE::BSAnimationGraphEvent>\(\)->AddEventSink\(&playerGraphEventObserver\)/);
    assert.match(cpp, /StaggerSuppression::HolderVTableIndex\(\)/);
    assert.match(cpp, /VTABLE_PlayerCharacter\[\*index\]/);
    assert.match(cpp, /VTABLE_Character\[\*index\]/);
    assert.doesNotMatch(cpp, /VTABLE_(?:PlayerCharacter|Character)\[0\]/);
    const incoming = cpp.slice(cpp.indexOf('void RogueStaggerCapture::NotePhysicalHit('),
        cpp.indexOf('void RogueStaggerCapture::NotePhysicalHitComplete'));
    assert.match(incoming, /RefreshPlayerGraphSink\(target\)/);
    assert.match(cpp, /NormalMovementEvent\(event\)/);
    assert.match(cpp, /target->IsBlocking\(\), target->IsAttacking\(\), target->WhoIsCasting\(\) != 0/);
    assert.match(cpp, /ApplyHavokImpulse/);
    assert.match(cpp, /HitData::incoming/);
    assert.match(cpp, /HitData::post-handler/);
    assert.match(cpp, /\[POST-HIT EVENT: \{\} \| \{\}\]/);
});

test('diagnostic package is separate and physics observers always pass through unchanged', () => {
    const build = read('xmake.lua');
    const cpp = read('src/rogue_stagger_diagnostic.cpp');
    const target = build.slice(build.indexOf('target("arpghud_rogue_stagger_diagnostic")'),
        build.indexOf('target("arpghud_gameplay_candidate")'));
    assert.match(target, /"dist", "TruePoise_Diagnostic", "SKSE", "Plugins"/);
    assert.doesNotMatch(target, /PLUGIN_NAME \.\. "_" \.\. PLUGIN_VERSION/);
    assert.match(cpp, /originalHitPhysics\(target, aggressor, damage, force, direction, position, useHitDirection, data\);/);
    assert.match(cpp, /original\(controller, velocity\);/);
    assert.match(cpp, /player->GetCharController\(\) == controller/);
    assert.match(cpp, /VTABLE_bhkCharProxyController\[1\]/);
    assert.match(cpp, /VTABLE_bhkCharRigidBodyController\[0\]/);
    assert.doesNotMatch(cpp, /6CEEF3|data->pushBack\s*=|ShouldSuppressHitPushBack/);
});

test('native hit velocity observer is narrowly validated, pass-through, and records timed velocity', () => {
    const cpp = read('src/rogue_stagger_diagnostic.cpp');
    const observer = cpp.slice(cpp.indexOf('void ObserveHitVelocity'), cpp.indexOf('void InstallHitVelocityObserver'));
    assert.match(observer, /originalHitVelocity\(controller, input, duration\);/);
    assert.equal((observer.match(/originalHitVelocity\(/g) || []).length, 1);
    assert.doesNotMatch(observer, /return;|^\s*(?:input|duration|controller->\w+)\s*=/m);
    assert.ok(observer.indexOf('hit-velocity/before') < observer.indexOf('originalHitVelocity('));
    assert.ok(observer.indexOf('hit-velocity/after') > observer.indexOf('originalHitVelocity('));
    assert.match(cpp, /site = base \+ 0x6CB36D/);
    assert.match(cpp, /destination != base \+ 0x1065F50/);
    assert.match(cpp, /std::memcmp[\s\S]*?call left unchanged/);
    assert.match(cpp, /originalHitVelocity = reinterpret_cast<HitVelocityFunction>\(destination\)/);
    assert.match(cpp, /initialVelocity=\{\} velocityTime=\{:\.6f\} outVelocity=\{\} velocityMod=\{\} pushDelta=\{\}/);
    assert.match(cpp, /physicalDamage=\{:\.3f\} reflectedDamage=\{:\.3f\}/);
});
