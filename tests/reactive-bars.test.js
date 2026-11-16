const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

const root = path.join(__dirname, '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replace(/\r\n/g, '\n');
const section = (text, start, end) => text.slice(text.indexOf(start), text.indexOf(end, text.indexOf(start) + 1));

// Restricted DOM: getElementById, classList.toggle/contains, hidden and style.setProperty only.
function createView() {
    const elements = new Map();
    const errors = [];
    let intervals = 0;
    function element(id) {
        if (!elements.has(id)) {
            const classes = new Set();
            elements.set(id, {
                id, hidden: false, textContent: '', attributes: {}, dataset: {},
                style: { setProperty() {} },
                classList: { toggle: (key, on) => on ? classes.add(key) : classes.delete(key), contains: key => classes.has(key) },
                setAttribute() {}
            });
        }
        return elements.get(id);
    }
    const rows = ['health', 'magicka', 'stamina', 'poise'].map(key => {
        const row = element(`min-${key}`);
        row.dataset.stat = key;
        row.firstElementChild = true;
        const fill = { style: {} }, trail = { style: {} };
        row.querySelector = selector => selector === '.min-fill' ? fill : trail;
        return row;
    });
    const context = {
        document: { getElementById: element, querySelectorAll: () => rows, documentElement: { style: { setProperty() {} } } },
        console: { error: (...args) => errors.push(args) },
        performance: { now: () => 0 },
        setInterval() { intervals++; },
        setTimeout: () => 1,
        clearTimeout() {}
    };
    vm.createContext(context);
    vm.runInContext(read('view/minimal.js'), context);
    return { context, element, errors, intervals: () => intervals };
}

const classesOf = (view, id) => ['health-poison', 'health-fire', 'health-bleed', 'health-sapped', 'bar-shock', 'stamina-frost']
    .filter(name => view.element(id).classList.contains(name));

test('reactive classes stay off until enabled and land on the right bars', () => {
    const view = createView();
    view.context.updateReactiveBars(['health-poison', 'stamina-frost', 'bar-shock']);
    assert.deepEqual(['min-health', 'min-magicka', 'min-stamina'].map(id => classesOf(view, id)), [[], [], []]);
    view.context.configureReactiveBars(true);
    assert.deepEqual(classesOf(view, 'min-health'), ['health-poison', 'bar-shock']);
    assert.deepEqual(classesOf(view, 'min-magicka'), ['bar-shock']);
    assert.deepEqual(classesOf(view, 'min-stamina'), ['stamina-frost']);
    view.context.updateReactiveBars(['health-fire', 'health-bleed', 'health-sapped']);
    assert.deepEqual(classesOf(view, 'min-health'), ['health-fire', 'health-bleed', 'health-sapped']);
    assert.deepEqual(classesOf(view, 'min-magicka'), []);
    assert.deepEqual(classesOf(view, 'min-stamina'), []);
    view.context.updateReactiveBars([]);
    assert.deepEqual(['min-health', 'min-magicka', 'min-stamina'].map(id => classesOf(view, id)), [[], [], []]);
});

test('switching the effects off clears every reactive class and ignores later updates', () => {
    const view = createView();
    view.context.configureReactiveBars(true);
    view.context.updateReactiveBars(['health-poison', 'bar-shock', 'stamina-frost']);
    view.context.configureReactiveBars(false);
    assert.deepEqual(['min-health', 'min-magicka', 'min-stamina'].map(id => classesOf(view, id)), [[], [], []]);
    view.context.updateReactiveBars(['health-fire']);
    assert.deepEqual(classesOf(view, 'min-health'), []);
    view.context.configureReactiveBars(true);
    assert.deepEqual(classesOf(view, 'min-health'), ['health-fire']);
});

test('invalid reactive payloads are rejected without changing the bars', () => {
    const view = createView();
    view.context.configureReactiveBars(true);
    view.context.updateReactiveBars(['health-poison']);
    view.context.updateReactiveBars(['health-frozen']);
    view.context.updateReactiveBars('health-fire');
    view.context.updateReactiveBars(['__proto__']);
    view.context.configureReactiveBars('on');
    assert.equal(view.errors.length, 4);
    assert.deepEqual(classesOf(view, 'min-health'), ['health-poison']);
});

test('reactive bars add no timers or DOM calls and the poise row is untouched', () => {
    const view = createView();
    const base = view.intervals();
    assert.equal(base, 1);
    view.context.configureReactiveBars(true);
    view.context.updateReactiveBars(['health-poison', 'bar-shock', 'stamina-frost']);
    assert.equal(view.intervals(), base);
    assert.deepEqual(classesOf(view, 'min-poise'), []);
    const js = read('view/minimal.js');
    const block = section(js, 'const REACTIVE_BAR_EFFECTS', 'function updateReactiveBars');
    assert.doesNotMatch(block, /createElement|appendChild|querySelector|innerHTML|setInterval/);
    assert.equal((read('view/index.html').match(/id="min-(health|magicka|stamina)"/g) || []).length, 3);
});

test('every reactive state is styled with the specified colors and never hides bar text', () => {
    const css = read('view/minimal.css');
    assert.match(css, /#min-health\.health-poison \.min-health \{ --c1: #7ab82e; --c2: #5a941e; \}/);
    assert.match(css, /#min-health\.health-poison \{[^}]*rgba\(122, 184, 46,/);
    assert.match(css, /#min-health\.health-sapped \.min-health \{ --c1: #9d7ae0;/);
    assert.match(css, /#min-health\.health-fire \.min-track \{[^}]*#e07a2a/);
    assert.match(css, /#min-health\.health-fire \{[^}]*rgba\(224, 122, 42,/);
    assert.match(css, /#min-health\.health-bleed \.min-track \{ animation: rb-bleed-border/);
    assert.match(css, /@keyframes rb-bleed-border \{[^@]*#c43a3a/);
    assert.match(css, /#min-health\.bar-shock \.min-track, #minimal-layout #min-magicka\.bar-shock \.min-track \{\s*animation: rb-shock/);
    assert.match(css, /@keyframes rb-shock \{[^@]*#4da6ff[^@]*#9ad8ff/);
    assert.match(css, /#min-stamina\.stamina-frost \.min-track \{[^}]*#3fb5c8/);
    const block = css.slice(css.indexOf('/* Reactive resource bars.'));
    assert.doesNotMatch(block, /calc\(|var\(--poise-ceiling|display: none|visibility: hidden|opacity: 0[;\s]/);
    assert.doesNotMatch(block, /\.min-text|\.min-value|\.min-label/);
});

test('the reactive bar setting is shipped, loaded, in the menu, reset, polled and reapplied', () => {
    const ini = read('poisesystemhud.ini');
    const hud = section(ini, '[HUD]', '\n[Poise]');
    assert.match(hud, /^bShowReactiveBarEffects\s*=\s*true$/m);
    const main = read('src/main.cpp');
    assert.match(main, /bool showReactiveBarEffects = true;/);
    assert.match(main, /readBool\(L"bShowReactiveBarEffects", "bShowReactiveBarEffects", settings\.showReactiveBarEffects\);/);
    assert.match(main, /bool\* get_showReactiveBarEffects\(\) \{ return &settings\.showReactiveBarEffects; \}/);
    assert.match(main, /BuiltinPoise::SetReactiveBarsEnabled\(settings\.showReactiveBarEffects\);/);
    assert.match(main, /changedKey == "bShowReactiveBarEffects" \|\| changedKey == "resetHud"\) \{\s*ConfigureReactiveBarsView\(\);/);
    const dom = section(main, 'ConfigurePoiseTelemetryView();\n\t\t\tConfigurePoiseDebuffIconsView();', 'UpdateHudVisibility();');
    assert.match(dom, /ConfigureReactiveBarsView\(\);/);
    const send = section(main, 'void SendReactiveBars()', 'void ConfigureReactiveBarsView()');
    assert.ok(send.indexOf('!settings.showReactiveBarEffects') < send.indexOf('ReadReactiveBars'));
    assert.match(send, /script == lastReactiveBarScript/);
    assert.match(main, /SendPoiseDebuffIcons\(\);\s*SendReactiveBars\(\);\s*if \(auto\* player = RE::PlayerCharacter::GetSingleton\(\)\) \{/);
    const menu = read('src/menu_framework.cpp');
    assert.match(menu, /Checkbox\("Show Reactive Resource Bar Effects##widgets", showReactiveBarEffects\)/);
    assert.match(menu, /QueueIniWrite\("bShowReactiveBarEffects", val, "HUD"\);\s*SendSettingsToHudImpl\("bShowReactiveBarEffects"\);/);
    assert.match(menu, /QueueIniWrite\("bShowReactiveBarEffects", "true", "HUD"\);/);
});

test('the player effect scan runs for the bars even when the poise debuffs are off, and only for the player', () => {
    const poise = read('src/builtin_poise.cpp');
    assert.match(poise, /std::atomic<bool> reactiveBarsEnabled\{ false \};/);
    const scan = section(poise, 'ScanDebuffs(', 'GetActiveEffectList()');
    assert.match(scan, /!gateDebuffs\.load\(std::memory_order_relaxed\) &&\s*!\(reactiveBarsEnabled\.load\(std::memory_order_relaxed\) && actor->IsPlayerRef\(\)\)/);
    const reactive = section(poise, 'bool BuiltinPoise::ReadReactiveBars', 'bool BuiltinPoise::ReadPlayerDebuffs');
    assert.doesNotMatch(reactive, /enablePoiseDebuffs|ResolveDebuffEffects/);
    assert.match(reactive, /gameplaySeconds - lastShockHit <= kShockHitSeconds/);
    assert.match(poise, /kShockHitSeconds = 1\.5/);
});

test('the two-handed default is 0.85 everywhere and concentration spells are excluded by default', () => {
    assert.match(read('src/builtin_poise_state.h'), /float twoHandedMultiplier = 0\.85F;/);
    assert.match(read('poisesystemhud.ini'), /^fTwoHandedMultiplier=0\.85$/m);
    assert.match(read('src/menu_framework.cpp'), /QueueIniWrite\("fTwoHandedMultiplier", "0\.85", "Poise"\);/);
    assert.match(read('README.md'), /0\.85/);
    const ini = read('poisesystemhud.ini');
    const magic = ini.split('[Magic Poise]')[1].split('\n[')[0];
    assert.match(magic, /^bConcentrationSpellsDealPoise=false$/m);
    assert.match(read('src/builtin_poise_state.h'), /bool concentrationSpellsDealPoise = false;/);
    const main = read('src/main.cpp');
    assert.match(main, /readBool\(L"bConcentrationSpellsDealPoise", "bConcentrationSpellsDealPoise",\s*poiseSettings\.concentrationSpellsDealPoise, L"Magic Poise"\);/);
    const menu = read('src/menu_framework.cpp');
    assert.match(menu, /QueueIniWrite\("bConcentrationSpellsDealPoise", \*concentrationDealsPoise \? "true" : "false", "Magic Poise"\);/);
    assert.match(menu, /QueueIniWrite\("bConcentrationSpellsDealPoise", "false", "Magic Poise"\);/);
});

test('hostile shock hits on the player are noted before concentration spells are skipped', () => {
    const magic = read('src/magic_gameplay.cpp');
    const note = magic.indexOf('BuiltinPoise::NoteShockHit();');
    assert.ok(note > 0);
    assert.match(magic.slice(note - 300, note), /kResistShock/);
    assert.ok(note < magic.indexOf('concentrationAllowed'));
    assert.doesNotMatch(magic, /kPoisonResist|MagicAlchHarmful|ScanDebuffs/);
});
