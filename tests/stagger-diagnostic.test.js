const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const root = path.join(__dirname, '..');
const read = (...parts) => fs.readFileSync(path.join(root, ...parts), 'utf8').replace(/\r\n/g, '\n');
const diagnostic = read('src', 'stagger_diagnostic.cpp');
const poise = read('src', 'builtin_poise.cpp');
const main = read('src', 'main.cpp');
const build = read('xmake.lua');
const menu = read('src', 'menu_framework.cpp');
const ini = read('poisesystemhud.ini');

test('stagger diagnostics are isolated in an explicit non-default build', () => {
    assert.match(build, /target\("arpghud_stagger_diagnostic"\)[\s\S]*?set_default\(false\)/);
    assert.match(build, /add_defines\("ARPGHUD_MAGIC_GAMEPLAY", "ARPGHUD_STAGGER_DIAGNOSTICS"\)/);
    assert.match(build, /PLUGIN_VERSION \.\. "-stagger-diagnostic"/);
    assert.match(main, /#ifdef ARPGHUD_STAGGER_DIAGNOSTICS\s+StaggerDiagnostic::Install\(\);\s+#endif/);
    assert.match(diagnostic, /#ifdef ARPGHUD_STAGGER_DIAGNOSTICS/);
});

test('stagger diagnostic hooks are validated, bounded, chained and logging-only', () => {
    assert.match(diagnostic, /module\.version\(\) != SKSE::RUNTIME_SSE_1_7_104/);
    assert.match(diagnostic, /constexpr unsigned kLimit = 1500/);
    assert.match(diagnostic, /StaggerSuppression::SetRoutineObserver\(ObserveRoutine\)/);
    assert.match(diagnostic, /StaggerSuppression::HolderVTableIndex\(\)/);
    assert.doesNotMatch(diagnostic, /write_branch|safe_fill|originalStagger/);
    assert.match(diagnostic, /const bool result = original\(holder, event\);[\s\S]*return result;/);
    assert.match(diagnostic, /VTABLE_StaggerEffect/);
    assert.match(diagnostic, /VTABLE_DisarmEffect/);
    assert.match(diagnostic, /_ReturnAddress\(\)/);
    assert.match(diagnostic, /recentEvents/);
    assert.match(diagnostic, /playerWasStaggering\.exchange\(true\)/);
    assert.match(diagnostic, /msSinceRoutineCall=/);
    assert.match(diagnostic, /CaptureStackBackTrace\(2, static_cast<DWORD>\(std::size\(frames\)\)/);
    assert.match(diagnostic, /ScanStackForCode\(\)/);
    assert.match(diagnostic, /staggerMagnitude/);
    assert.match(diagnostic, /StartsWithI\(name, "turn"\)/);
    assert.match(diagnostic, /void StaggerDiagnostic::OnHit/);
    assert.match(diagnostic, /Stagger diagnostic hit: target=\[\{\}\] aggressor=\[\{\}\]/);
    assert.match(diagnostic, /void StaggerDiagnostic::OnDamageSample/);
    assert.match(diagnostic, /physicalDamage=\{\} totalDamage=\{\} sneakAttackBonus=\{\}/);
    assert.match(diagnostic, /sample\.aggressor->IsPlayerRef\(\)\) \|\|\s*sample\.data->flags\.any\(RE::HitData::Flag::kPredictDamage\)/);
    assert.match(diagnostic, /!StaggerGate::Allowed\(\) && effectDepth == 0[\s\S]*?StartsWithI\(event\.c_str\(\), "staggerStart"\)/);
    assert.match(diagnostic, /non-poise \{\} sender/);
    assert.match(diagnostic, /wardbreak/);
    assert.doesNotMatch(diagnostic, /DamageActorValue|RestoreActorValue|directionalStagger/);
});

test('poise-triggered staggers are marked with the thread-local allow guard', () => {
    assert.match(poise, /#include "stagger_gate\.h"/);
    const calls = poise.match(/StaggerGate::ScopedAllowStagger allowStagger;\s+directionalStagger\(/g) || [];
    assert.equal(calls.length, 2);
    assert.equal((poise.match(/\bdirectionalStagger\(/g) || []).length, 2);
});

test('hit sampling in the poise hook is compiled only into the stagger diagnostic build', () => {
    const body = poise.slice(poise.indexOf('void ProcessHit('), poise.indexOf('void BuiltinPoise::Install()'));
    const stripped = body.replace(/#ifdef ARPGHUD_STAGGER_DIAGNOSTICS[\s\S]*?#endif/g, '');
    assert.doesNotMatch(stripped, /StaggerDiagnostic|sample\./);
    assert.equal((body.match(/#ifdef ARPGHUD_STAGGER_DIAGNOSTICS/g) || []).length, 9);
    assert.match(body, /StaggerDiagnostic::OnHit\(sample\);/);
});

test('poise reset restores exactly the shipped ini defaults', () => {
    const reset = menu.slice(menu.indexOf('void ResetPoiseDefaults()'));
    const writes = new Map([...reset.matchAll(/QueueIniWrite\("([A-Za-z]+)", "([^"]+)", "(Poise|Poise Debuffs|Magic Poise)"\)/g)]
        .map(m => [`${m[3]}/${m[1]}`, m[2]]));
    assert.ok(writes.size > 20);
    for (const section of ['Poise', 'Poise Debuffs', 'Magic Poise']) {
        const body = ini.split(`[${section}]`)[1].split('\n[')[0];
        const defaults = new Map(body.split('\n').filter(line => /^[bfs][A-Za-z]+=/.test(line))
            .map(line => line.trim().split('=')));
        assert.ok(defaults.size > (section === 'Poise' ? 20 : section === 'Magic Poise' ? 0 : 10), section);
        for (const [key, value] of defaults) {
            const written = writes.get(`${section}/${key}`);
            assert.ok(written !== undefined, `${section}/${key} is not reset`);
            const same = /^[a-z,]+$/.test(value) || value === 'true' || value === 'false' ? written === value :
                Number(written) === Number(value);
            assert.ok(same, `${key}: reset writes ${written}, ini ships ${value}`);
        }
    }
});

test('hud reset retains the minimalist offset', () => {
    assert.doesNotMatch(menu, /QueueIniWrite\("iSideOffset"/);
    assert.match(menu, /QueueIniWrite\("iSideOffsetLeft", "20", "HUD"\)/);
    assert.doesNotMatch(menu, /QueueIniWrite\("iSideOffsetRight"/);
});
