const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

const root = path.join(__dirname, '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replace(/\r\n/g, '\n');

function createDom() {
    const elements = new Map();
    const errors = [];
    function makeElement(id = '') {
        const classes = new Set();
        const element = {
            id, hidden: false, children: [], attributes: {}, className: '', title: '', _text: '',
            classList: {
                toggle: (key, on) => (on ? classes.add(key) : classes.delete(key)),
                contains: key => classes.has(key)
            },
            setAttribute(name, value) { this.attributes[name] = value; },
            appendChild(child) { this.children.push(child); return child; }
        };
        Object.defineProperty(element, 'textContent', {
            get() { return element._text + element.children.map(child => child.textContent).join(''); },
            set(value) { element._text = String(value); element.children = []; }
        });
        return element;
    }
    const getElementById = id => {
        if (!elements.has(id)) elements.set(id, makeElement(id));
        return elements.get(id);
    };
    const context = {
        document: {
            getElementById, querySelectorAll: () => [], createElement: () => makeElement(),
            createTextNode: text => ({ textContent: text }),
            documentElement: { style: { setProperty() {} } }
        },
        console: { error: (...args) => errors.push(args) },
        performance: { now: () => 0 }, setInterval() {}, setTimeout() {}, clearTimeout() {}
    };
    vm.createContext(context);
    vm.runInContext(read('view/minimal.js'), context);
    return { context, element: getElementById, errors };
}

const actor = (extra = {}) => ({
    name: 'Player', tier: 'Player', current: 55, max: 73, ceilingRatio: 1, ceilingPoise: 73, regen: 8, delay: 0,
    delayMax: 3, immune: 0, flesh: 18, lightArmor: 0, stale: false, debuffs: [], ...extra
});
const payload = (extra = {}) => ({
    player: actor(), target: null,
    weapon: { armed: true, twoHanded: false, ranged: false, light: 26.25, power: 39.375, rawEstimate: 2, rawCapped: false },
    hits: [], ...extra
});
const hit = (extra = {}) => ({
    dir: 'OUT', attacker: 'Player', target: 'Frost Troll', type: '2H Melee', base: 13.6, raw: 2.3, rawCapped: false,
    mults: [['2H', 1.15]], final: 38, broke: false, immune: false, ...extra
});

test('the overlay is hidden until configured and ignores payloads while disabled', () => {
    const { context, element, errors } = createDom();
    context.updatePoiseTelemetry(payload());
    assert.equal(element('pt-player-poise').textContent, '');
    context.configureTelemetry(true);
    assert.equal(element('poise-telemetry').hidden, false);
    assert.equal(element('poise-telemetry').attributes['aria-hidden'], 'false');
    context.configureTelemetry(false);
    assert.equal(element('poise-telemetry').hidden, true);
    assert.equal(element('poise-telemetry').attributes['aria-hidden'], 'true');
    context.updatePoiseTelemetry(payload());
    assert.equal(element('pt-player-poise').textContent, '');
    context.configureTelemetry('yes');
    assert.equal(errors.length, 1);
});

test('player card shows poise, flesh, regen, ceiling, timers, weapon benchmark and a clean debuff list', () => {
    const { context, element } = createDom();
    context.configureTelemetry(true);
    context.updatePoiseTelemetry(payload({ player: actor({ delay: 1.5, lightArmor: 9 }) }));
    assert.equal(element('pt-player-name').textContent, 'Player');
    assert.equal(element('pt-player-poise').textContent, 'Poise: 55.0 / 73.0 (Flesh +18.0, Light Armor +9.0)');
    assert.equal(element('pt-player-regen').textContent, 'Regen: +8.0/s | Cap: 100% (73.0)');
    assert.equal(element('pt-player-delay').textContent, 'Delay: 1.5s / 3.0s');
    assert.equal(element('pt-player-delay').classList.contains('active'), true);
    assert.equal(element('pt-player-immune').textContent, 'Immune: 0.0s');
    assert.equal(element('pt-player-immune').classList.contains('active'), false);
    assert.equal(element('pt-player-debuffs').textContent, 'None');
    assert.equal(element('pt-weapon').textContent, '1H Light: 26.3 | Power: 39.4 (est. dmg bonus +2.0)');
    assert.equal(element('pt-target').hidden, true);
    assert.equal(element('pt-status').hidden, true);
});

test('weapon benchmark labels two-handed, ranged, capped and unarmed states', () => {
    const { context, element } = createDom();
    context.configureTelemetry(true);
    const weapon = extra => ({ armed: true, twoHanded: false, ranged: false, light: 40, power: 60, rawEstimate: 6, rawCapped: true, ...extra });
    context.updatePoiseTelemetry(payload({ weapon: weapon({ twoHanded: true }) }));
    assert.equal(element('pt-weapon').textContent, '2H Light: 40.0 | Power: 60.0 (est. dmg bonus +6.0 [CAP])');
    context.updatePoiseTelemetry(payload({ weapon: weapon({ ranged: true, rawCapped: false }) }));
    assert.match(element('pt-weapon').textContent, /^Ranged Light: 40\.0/);
    context.updatePoiseTelemetry(payload({ weapon: weapon({ armed: false }) }));
    assert.equal(element('pt-weapon').textContent, 'Unarmed');
});

test('target card shows tier, hits-to-break estimate, timers and debuff pills', () => {
    const { context, element } = createDom();
    context.configureTelemetry(true);
    const sapped = { id: 'sapped', label: 'SAPPED', detail: '0.0/s regen', source: 'Frostbite' };
    context.updatePoiseTelemetry(payload({
        target: actor({ name: 'Frost Troll', tier: 'Heavy', current: 112, max: 150, delay: 2.4, immune: 1, flesh: 0, debuffs: [sapped] })
    }));
    assert.equal(element('pt-target').hidden, false);
    assert.equal(element('pt-target-name').textContent, 'Frost Troll [Heavy]');
    assert.equal(element('pt-target-poise').textContent, 'Poise: 112.0 / 150.0 (~4.3 light hits)');
    assert.equal(element('pt-target-delay').classList.contains('active'), true);
    assert.equal(element('pt-target-immune').classList.contains('active'), true);
    assert.equal(element('pt-target-debuffs').children[0].className, 'pt-pill pt-sapped');
    assert.equal(element('pt-target-debuffs').textContent, 'SAPPED 0.0/s regen');
    context.updatePoiseTelemetry(payload());
    assert.equal(element('pt-target').hidden, true);
});

test('a killed target keeps its card, marked last known, and a live target is not marked', () => {
    const { context, element } = createDom();
    context.configureTelemetry(true);
    context.updatePoiseTelemetry(payload({
        target: actor({ name: 'Bandit Outlaw', tier: 'NPC', current: 0, max: 80, stale: true }), hits: [hit({ target: 'Bandit Outlaw' })]
    }));
    assert.equal(element('pt-target').hidden, false);
    assert.equal(element('pt-target-name').textContent, 'Bandit Outlaw [NPC] (last known)');
    context.updatePoiseTelemetry(payload({ target: actor({ name: 'Bandit Outlaw', tier: 'NPC', current: 60, max: 80 }) }));
    assert.equal(element('pt-target-name').textContent, 'Bandit Outlaw [NPC]');
    assert.equal(element('pt-target-poise').textContent, 'Poise: 60.0 / 80.0 (Flesh +18.0) (~2.3 light hits)');
});

test('the target card sits between the player card and the hit log and does not depend on the target bar setting', () => {
    const html = read('view/index.html');
    assert.ok(html.indexOf('id="pt-player"') < html.indexOf('id="pt-target"'));
    assert.ok(html.indexOf('id="pt-target"') < html.indexOf('id="pt-hits"'));
    const main = read('src/main.cpp');
    const send = main.slice(main.indexOf('void SendPoiseTelemetry()'), main.indexOf('void ConfigurePoiseTelemetryView()'));
    assert.doesNotMatch(send, /showTargetPoiseBar|ReadTarget\(/);
    const poise = read('src/builtin_poise.cpp');
    const readTelemetry = poise.slice(poise.indexOf('bool BuiltinPoise::ReadTelemetry('), poise.indexOf('bool BuiltinPoise::ReadTarget('));
    assert.doesNotMatch(readTelemetry, /showTargetPoiseBar|kTargetBarSeconds/);
    assert.match(readTelemetry, /snapshot\.target\.stale = true;/);
    assert.doesNotMatch(readTelemetry, /snapshot\.hasTarget = false/);
    assert.match(poise, /NoteTarget\(target, id, targetHandle, tier, hitPoise\)/);
    assert.match(poise, /NoteTarget\(target, id, handle, tier, &entry\.poise\)/);
    assert.match(poise, /lastTarget\.view = DescribePoise\(\*poise, lastTarget\.name, TierLabel\(tier\)\)/);
});

test('hit log renders newest first with raw bonus, cap tag, multipliers, immunity and stagger tags', () => {
    const { context, element } = createDom();
    context.configureTelemetry(true);
    const hits = [
        hit({ rawCapped: true, broke: true }),
        hit({ dir: 'IN', attacker: 'Giant', target: 'Player', type: 'Creature', base: 8, raw: 0, final: 9, mults: [['Boss', 2.4], ['Blocked (Shield)', 0.25]] }),
        hit({ type: 'Magic Burst', mults: [], immune: true, final: 12 })
    ];
    context.updatePoiseTelemetry(payload({ hits }));
    assert.equal(element('pt-hit-0').textContent,
        '[OUT] Player -> Frost Troll (2H Melee): 38.0 [Base 13.6 + RawDmg 2.3 [CAP] * 2H 1.15] [STAGGER!]');
    assert.equal(element('pt-hit-0').classList.contains('pt-out'), true);
    assert.equal(element('pt-hit-0').children[1].className, 'pt-break');
    assert.equal(element('pt-hit-1').textContent,
        '[IN] Giant -> Player (Creature): 9.0 [Base 8.0 * Boss 2.40 * Blocked (Shield) 0.25]');
    assert.equal(element('pt-hit-1').classList.contains('pt-in'), true);
    assert.equal(element('pt-hit-2').textContent, '[OUT] Player -> Frost Troll (Magic Burst): 0.0 (IMMUNE) [Base 13.6 + RawDmg 2.3]');
    assert.equal(element('pt-hit-3').hidden, true);
    assert.equal(element('pt-hit-4').hidden, true);
    context.updatePoiseTelemetry(payload());
    assert.equal(element('pt-hit-0').hidden, true);
    assert.equal(element('pt-hit-0').textContent, '');
});

test('actor and attacker names are rendered as text, never as markup', () => {
    const { context, element } = createDom();
    context.configureTelemetry(true);
    const name = '<img src=x onerror=alert(1)>';
    context.updatePoiseTelemetry(payload({ target: actor({ name, tier: 'NPC' }), hits: [hit({ target: name })] }));
    assert.equal(element('pt-target-name').textContent, `${name} [NPC]`);
    assert.match(element('pt-hit-0').textContent, /<img src=x onerror=alert\(1\)>/);
    const js = read('view/minimal.js');
    assert.doesNotMatch(js.slice(js.indexOf('const telemetryState')), /innerHTML|outerHTML|insertAdjacentHTML|document\.write/);
});

test('an unavailable payload shows a status line and an invalid payload changes nothing', () => {
    const { context, element, errors } = createDom();
    context.configureTelemetry(true);
    context.updatePoiseTelemetry(payload({ target: actor({ name: 'Draugr', tier: 'Medium' }), hits: [hit()] }));
    context.updatePoiseTelemetry(null);
    assert.equal(element('pt-status').hidden, false);
    assert.equal(element('pt-player').hidden, true);
    assert.equal(element('pt-target').hidden, true);
    assert.equal(element('pt-hits').hidden, true);
    context.updatePoiseTelemetry(payload());
    assert.equal(element('pt-status').hidden, true);
    assert.equal(element('pt-player').hidden, false);
    const before = element('pt-player-poise').textContent;
    context.updatePoiseTelemetry({ ...payload(), player: { ...actor(), current: 'x' } });
    context.updatePoiseTelemetry({ ...payload(), hits: [{ dir: 'SIDEWAYS' }] });
    context.updatePoiseTelemetry(undefined);
    assert.equal(errors.length, 3);
    assert.equal(element('pt-player-poise').textContent, before);
});

test('the overlay is a separate fixed, click-through block that does not touch the minimalist layout', () => {
    const html = read('view/index.html'), css = read('view/minimal.css');
    assert.match(html, /<aside id="poise-telemetry" hidden/);
    for (const id of ['pt-weapon', 'pt-status', 'pt-player', 'pt-target', 'pt-hits', 'pt-hit-0', 'pt-hit-4',
        'pt-player-debuffs', 'pt-target-debuffs']) {
        assert.match(html, new RegExp(`id="${id}"`));
    }
    assert.ok(html.indexOf('id="poise-telemetry"') > html.indexOf('id="minimal-layout"'));
    assert.ok(html.indexOf('id="poise-telemetry"') < html.indexOf('<script src="minimal.js">'));
    assert.match(css, /html, body \{[^}]*pointer-events: none/);
    assert.match(css, /#poise-telemetry \{[^}]*position: fixed; top: 96px; left: 14px/);
    assert.match(css, /#poise-telemetry\[hidden\], #poise-telemetry \[hidden\] \{ display: none; \}/);
    const js = read('view/minimal.js');
    assert.equal((js.match(/setInterval\(/g) || []).length, 1);
});

test('bShowPoiseTelemetry is shipped off, loaded, reset and applied on reload, with no menu option', () => {
    const ini = read('TruePoise.ini'), main = read('src/main.cpp'), menu = read('src/menu_framework.cpp');
    assert.match(ini.split('[Poise]')[0], /^bShowPoiseTelemetry=false$/m);
    assert.match(main, /bool showPoiseTelemetry = false;/);
    assert.match(main, /readBool\(L"bShowPoiseTelemetry", "bShowPoiseTelemetry", settings\.showPoiseTelemetry\)/);
    assert.match(main, /BuiltinPoise::Configure\(poiseSettings\);\s*BuiltinPoise::SetTelemetryEnabled\(settings\.showPoiseTelemetry\);/);
    // Regular players do not see the dev overlay in the menu; the showpoisedev console command and the INI remain.
    assert.doesNotMatch(menu, /Poise Telemetry|get_showPoiseTelemetry|SendSettingsToHudImpl\("bShowPoiseTelemetry"\)/);
    assert.doesNotMatch(main, /get_showPoiseTelemetry/);
    assert.match(menu, /QueueIniWrite\("bShowPoiseTelemetry", "false", "HUD"\)/);
    const apply = main.slice(main.indexOf('void SendSettingsToHudImpl('), main.indexOf('// Export settings for menu access'));
    assert.match(apply, /changedKey == "bShowPoiseTelemetry"\) \{\s*BuiltinPoise::SetTelemetryEnabled\(settings\.showPoiseTelemetry\);/);
    assert.match(apply, /changedKey == "bShowPoiseTelemetry" \|\| changedKey == "resetHud"\) \{\s*ConfigurePoiseTelemetryView\(\);/);
    const domReady = main.slice(main.indexOf('void OnDomReady('), main.indexOf('void CreateHudView()'));
    assert.match(domReady, /viewReady = true;\s*hasSentPoise = false;\s*ConfigurePoiseTelemetryView\(\);/);
    const reload = main.slice(main.indexOf('void ReloadHud()'), main.indexOf('void RegisterReloadKey()\n\t{'));
    assert.match(reload, /LoadSettings\(\)/);
    assert.match(reload, /window\.location\.reload\(\)/);
});

test('telemetry is polled at 10 Hz, skipped when off, and never serialized without the flag', () => {
    const main = read('src/main.cpp');
    assert.match(main, /constexpr DWORD kStatsUpdateIntervalMs = 100;/);
    const poll = main.slice(main.indexOf('void PollHud()'), main.indexOf('VOID CALLBACK TimerCallback'));
    assert.match(poll, /SendPoise\(\);\s*SendPoiseTelemetry\(\);/);
    const send = main.slice(main.indexOf('void SendPoiseTelemetry()'), main.indexOf('void ConfigurePoiseTelemetryView()'));
    assert.ok(send.indexOf('if (!settings.showPoiseTelemetry)') < send.indexOf('TelemetryJson('));
    assert.ok(send.indexOf('if (!settings.showPoiseTelemetry)') < send.indexOf('ReadTelemetry('));
    assert.match(send, /updatePoiseTelemetry\(\{\}\)/);
    assert.match(send, /script == lastTelemetryScript/);
    const configure = main.slice(main.indexOf('void ConfigurePoiseTelemetryView()'), main.indexOf('// showpoisedev'));
    assert.match(configure, /lastTelemetryScript\.clear\(\);/);
    assert.match(configure, /configureTelemetry\(\{\}\)/);
});

test('showpoisedev toggles the same state, prints the result and updates the view on the UI task', () => {
    const main = read('src/main.cpp');
    const execute = main.slice(main.indexOf('bool ExecuteShowPoiseDev('), main.indexOf('void RegisterShowPoiseDevCommand()'));
    assert.match(execute, /settings\.showPoiseTelemetry = !settings\.showPoiseTelemetry;/);
    assert.match(execute, /BuiltinPoise::SetTelemetryEnabled\(settings\.showPoiseTelemetry\);/);
    assert.match(execute, /"Poise Telemetry Overlay: %s"/);
    assert.match(execute, /settings\.showPoiseTelemetry \? "ON" : "OFF"/);
    assert.match(execute, /taskInterface->AddUITask\(\[\]\(\) \{ ConfigurePoiseTelemetryView\(\); \}\)/);
    assert.doesNotMatch(execute, /QueueIniWrite|WritePrivateProfile/);
    const register = main.slice(main.indexOf('void RegisterShowPoiseDevCommand()'), main.indexOf('void SendWorldTelemetry('));
    assert.match(register, /LocateConsoleCommand\("ClearScreenBlood"\)/);
    assert.match(register, /command->functionName = "showpoisedev";/);
    assert.match(register, /command->executeFunction = ExecuteShowPoiseDev;/);
    assert.match(register, /command->SetParameters\(\);/);
    assert.match(register, /if \(!command\) \{[\s\S]*?return;/);
    const dataLoaded = main.slice(main.indexOf('case SKSE::MessagingInterface::kDataLoaded:'), main.indexOf('case SKSE::MessagingInterface::kPreLoadGame:'));
    assert.match(dataLoaded, /RegisterReloadKey\(\);\s*RegisterShowPoiseDevCommand\(\);/);
});

test('backend recording is gated by one atomic and the target card holds for 15 seconds', () => {
    const poise = read('src/builtin_poise.cpp');
    assert.match(poise, /std::atomic<bool> telemetryEnabled\{ false \};/);
    assert.match(poise, /constexpr double kTelemetryTargetSeconds = 15\.0;/);
    assert.match(poise, /gameplaySeconds - lastTarget\.lastHit <= kTelemetryTargetSeconds/);
    assert.match(poise, /actor->IsDead\(\) \|\| !actor->Is3DLoaded\(\)/);
    assert.match(poise, /const bool recordHit = telemetryEnabled\.load\(std::memory_order_relaxed\) && \(player \|\| aggressor->IsPlayerRef\(\)\)/);
    assert.match(poise, /HitBreakdown\* breakdownOut = recordHit \? &breakdown : nullptr;/);
    assert.match(poise, /RecordHit\(!player, std::move\(attackerLabel\), std::move\(targetLabel\), breakdown, result\)/);
    assert.match(poise, /const bool recordHit = telemetryEnabled\.load\(std::memory_order_relaxed\) && \(player \|\| attacker->IsPlayerRef\(\)\)/);
    assert.match(poise, /const bool telemetry = refresh && telemetryEnabled\.load\(std::memory_order_relaxed\);/);
    assert.match(poise, /weaponBenchmark = BuiltinPoise::ComputeWeaponBenchmark\(/);
    assert.match(poise, /void BuiltinPoise::SetTelemetryEnabled\(bool enabled\)[\s\S]*?hitLog\.Clear\(\);\s*weaponBenchmark = \{\};/);
    assert.equal((poise.match(/hitLog\.Clear\(\)/g) || []).length >= 4, true);
    const magic = read('src/magic_gameplay.cpp');
    assert.match(magic, /if \(BuiltinPoise::TelemetryEnabled\(\)\) \{\s*const auto explained = MagicPoise::Explain\(input\);/);
    assert.match(magic, /breakdown\.Add\("Ward", explained\.ward\)/);
    assert.match(magic, /breakdown\.Add\("NPC Magic", settings\.npcMagicPoiseMultiplier\)/);
});

test('debuff and ceiling fields are wired into the payload ready for the debuff pass', () => {
    const header = read('src/builtin_poise_telemetry.h');
    assert.match(header, /float ceilingRatio = 1\.0F;/);
    assert.match(header, /std::vector<Debuff> debuffs;/);
    assert.match(header, /"\],\\"debuffs\\":\[|\\"debuffs\\":\[/);
    const poise = read('src/builtin_poise.cpp');
    assert.match(poise, /result\.ceilingRatio = fx\.ceilingRatio;\s*result\.ceilingPoise = poise\.maximum \* result\.ceilingRatio;/);
    assert.match(poise, /result\.debuffs = BuiltinPoise::BuildDebuffList\(configuration, poise\.debuffs\);/);
});
