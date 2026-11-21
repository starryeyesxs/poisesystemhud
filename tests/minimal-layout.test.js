const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

function createView() {
    const elements = new Map();
    const errors = [];
    let now = 0;
    let tick;
    let nextTimer = 0;
    const timers = new Map();
    function element(id) {
        if (!elements.has(id)) {
            const classes = new Set();
            const properties = new Map();
            elements.set(id, {
                id, hidden: false, textContent: '', attributes: {}, dataset: {},
                style: { setProperty: (key, value) => properties.set(key, value), properties },
                classList: {
                    add: key => classes.add(key),
                    remove: key => classes.delete(key),
                    toggle: (key, on) => on ? classes.add(key) : classes.delete(key),
                    contains: key => classes.has(key)
                },
                setAttribute(name, value) { this.attributes[name] = value; }
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
        row.fill = fill;
        row.trail = trail;
        return row;
    });
    const context = {
        document: {
            getElementById: element, querySelectorAll: () => rows,
            documentElement: { style: { setProperty: (key, value) => element('root').style.setProperty(key, value) } }
        },
        console: { error: (...args) => errors.push(args) },
        performance: { now: () => now },
        setInterval: callback => { tick = callback; },
        setTimeout: (callback, delay) => {
            const id = ++nextTimer;
            timers.set(id, { callback, due: now + delay });
            return id;
        },
        clearTimeout: id => timers.delete(id)
    };
    vm.createContext(context);
    vm.runInContext(fs.readFileSync(path.join(__dirname, '..', 'view', 'minimal.js'), 'utf8'), context);
    return { context, element, errors, advance(ms) {
        now += ms;
        for (const [id, timer] of timers) {
            if (timer.due <= now) {
                timers.delete(id);
                timer.callback();
            }
        }
        tick();
    } };
}

test('minimalist damage trail holds, drains, and never falls below recovery', () => {
    const v = createView(), c = v.context;
    c.configureLayouts(true, true);
    c.updateMinimalStats(100, 100, 100, 100, 100, 100, true, false);
    c.updateMinimalStats(60, 100, 80, 100, 70, 100, true, false);
    c.updateMinimalPoise(true, 100, 100);
    c.updateMinimalPoise(true, 50, 100);
    for (const [key, expected] of [['health', 'scaleX(0.6)'], ['magicka', 'scaleX(0.8)'], ['stamina', 'scaleX(0.7)'], ['poise', 'scaleX(0.5)']]) {
        assert.equal(v.element(`min-${key}`).fill.style.transform, expected);
        assert.equal(v.element(`min-${key}`).trail.style.transform, 'scaleX(1)');
    }
    v.advance(600);
    assert.equal(v.element('min-health').trail.style.transform, 'scaleX(1)');
    v.advance(33);
    assert.equal(v.element('min-health').trail.style.transform, 'scaleX(0.96)');
    c.updateMinimalStats(98, 100, 80, 100, 70, 100, true, false);
    assert.equal(v.element('min-health').trail.style.transform, 'scaleX(0.98)');
    v.advance(2000);
    assert.equal(v.element('min-health').trail.style.transform, 'scaleX(0.98)');
});

test('health, magicka, and stamina widths scale with max; poise stays fixed', () => {
    const v = createView(), c = v.context;
    c.configureLayouts(false, false);
    c.updateMinimalStats(100, 100, 100, 100, 100, 100, true, false);
    for (const key of ['health', 'magicka', 'stamina']) assert.equal(v.element(`min-${key}`).style.width, '200px');
    c.updateMinimalStats(200, 200, 300, 300, 100, 100, true, false);
    assert.equal(v.element('min-health').style.width, '250px');
    assert.equal(v.element('min-magicka').style.width, '300px');
    assert.equal(v.element('min-stamina').style.width, '200px');
    c.updateMinimalStats(50, 50, 660, 660, 5000, 5000, true, false);
    assert.equal(v.element('min-health').style.width, '200px');
    assert.equal(v.element('min-magicka').style.width, '480px');
    assert.equal(v.element('min-stamina').style.width, '480px');
    c.updateMinimalPoise(true, 10, 40);
    c.updateMinimalPoise(true, 10, 900);
    assert.equal(v.element('min-poise').style.width, undefined);
    assert.equal(v.errors.length, 0);
});

test('minimalist graphics toggle classes and scale shine and shadow', () => {
    const v = createView(), c = v.context;
    const layout = v.element('minimal-layout');
    c.updateMinimalGraphics(false, 2, 0, true);
    assert.ok(layout.classList.contains('no-filigree'));
    assert.ok(layout.classList.contains('show-notches'));
    assert.equal(layout.style.properties.get('--min-shine-alpha'), '0.7');
    assert.equal(layout.style.properties.get('--min-edge-alpha'), '1');
    assert.equal(layout.style.properties.get('--min-drop-alpha'), '0');
    assert.equal(layout.style.properties.get('--min-drop-blur'), '0px');
    c.updateMinimalGraphics(true, 1, 1, false);
    assert.ok(!layout.classList.contains('no-filigree'));
    assert.ok(!layout.classList.contains('show-notches'));
    assert.equal(layout.style.properties.get('--min-drop-alpha'), '0.8');
    assert.equal(v.errors.length, 0);
    c.updateMinimalGraphics(true, 2.5, 1, false);
    c.updateMinimalGraphics('true', 1, 1, false);
    assert.equal(v.errors.length, 2);
});

test('layout visibility, weather, and gem respond independently', () => {
    const v = createView(), c = v.context;
    c.configureLayouts(true, true);
    c.updateMinimalStats(10, 100, 10, 100, 10, 100, true, false);
    assert.equal(v.element('minimal-layout').hidden, false);
    c.updateWorldWidget('Wilderness', 14, 30, 17, 8, 201, 'cloudy', false);
    assert.equal(v.element('world-weather').textContent, 'cloudy');
    c.updateWorldWidget('Barrow', 14, 30, 17, 8, 201, 'cloudy', true);
    assert.equal(v.element('world-weather').textContent, 'Indoors');
    assert.equal(v.element('world-time').textContent, '14:30');
    c.configureLayouts(false, true);
    c.updateMinimalStats(100, 100, 100, 100, 100, 100, false, false);
    assert.equal(v.element('world-widget').hidden, true);
    assert.ok(!v.element('min-health').classList.contains('hidden'));
    c.updateMinimalStats(100, 100, 100, 100, 100, 100, false, true);
    assert.ok(!v.element('min-stamina').classList.contains('hidden'));
    assert.ok(v.element('min-health').classList.contains('hidden'));
    c.configureHudTheme('minimalist');
    c.updateLightGem(true, 100);
    assert.equal(v.element('light-gem').hidden, false);
    assert.equal(v.element('light-gem-fill').attributes.height, '34');
    assert.equal(v.element('daedric-light-gem').hidden, true);
    c.updateLightGem(true, null);
    assert.equal(v.element('light-gem').hidden, true);
    c.updateLightGem(false, 50);
    assert.equal(v.element('light-gem').hidden, true);
    c.configureLayouts(true, false);
    c.updateLightGem(true, 100);
    assert.equal(v.element('light-gem').hidden, true);
    assert.equal(v.errors.length, 0);
});

test('custom HUD visibility leaves standalone Poise, Weather, target Poise and telemetry active', () => {
    const v = createView(), c = v.context;
    const layout = v.element('minimal-layout');
    c.configureLayouts(true, true);
    c.configureCustomHud(false);
    assert.ok(layout.classList.contains('custom-hud-disabled'));
    assert.equal(layout.hidden, false);
    assert.equal(v.element('world-widget').hidden, false);

    c.updateWorldWidget('Wilderness', 14, 30, 17, 8, 201, 'rain', false);
    c.updateMinimalPoise(true, 42.5, 65, true);
    c.updateTargetPoise(true, 'Bandit', 'Heavy', 20, 100, false);
    c.configureTelemetry(true);
    assert.equal(v.element('world-weather').textContent, 'rain');
    assert.equal(v.element('min-poise-numbers').hidden, false);
    assert.equal(v.element('min-poise-numbers').textContent, '42.5 / 65.0');
    assert.ok(v.element('target-poise').classList.contains('show'));
    assert.equal(v.element('poise-telemetry').hidden, false);

    c.configureCustomHud(true);
    assert.ok(!layout.classList.contains('custom-hud-disabled'));
    c.configureCustomHud('false');
    assert.equal(v.errors.length, 1);
});

test('visibility settings control idle fading and always-on bars', () => {
    const v = createView(), c = v.context;
    const layout = v.element('minimal-layout');
    c.configureLayouts(true, true);
    c.configureVisibility(false, true, 0.4, 3);
    c.updateMinimalStats(100, 100, 100, 100, 100, 100, false, false);
    assert.equal(layout.style.properties.get('--idle-opacity'), 0.4);
    assert.ok(!layout.classList.contains('hud-idle'));
    v.advance(2999);
    assert.ok(!layout.classList.contains('hud-idle'));
    v.advance(1);
    assert.ok(layout.classList.contains('hud-idle'));
    c.updateMinimalPoise(true, 50, 100);
    assert.ok(!layout.classList.contains('hud-idle'));
    c.updateMinimalPoise(true, 100, 100);
    v.advance(3000);
    assert.ok(layout.classList.contains('hud-idle'));
    c.configureVisibility(true, true, 0.4, 3);
    assert.ok(!layout.classList.contains('hud-idle'));
    c.updateMinimalStats(100, 100, 100, 100, 100, 100, true, false);
    assert.ok(!v.element('min-health').classList.contains('hidden'));
    c.configureVisibility(false, false, 0.4, 3);
    assert.ok(!v.element('min-health').classList.contains('hidden'));
    v.advance(4000);
    assert.ok(!layout.classList.contains('hud-idle'));
    c.configureVisibility(false, true, -1, 3);
    assert.equal(v.errors.length, 1);
});

test('resource idle detection tolerates float drift while preserving casting, sprinting, and drain wakeups', () => {
    const v = createView(), c = v.context;
    const layout = v.element('minimal-layout');
    c.configureLayouts(true, true);
    c.configureVisibility(false, true, 0, 0);

    c.updateMinimalStats(199, 200, 297.5, 300, 99.1, 100, false, false);
    v.advance(0);
    assert.ok(layout.classList.contains('hud-idle'));

    c.updateMinimalStats(199, 200, 298, 300, 99.1, 100, false, false, true);
    assert.ok(!layout.classList.contains('hud-idle'));
    assert.ok(!v.element('min-magicka').classList.contains('hidden'));

    c.updateMinimalStats(199, 200, 298, 300, 99.1, 100, false, true, false);
    assert.ok(!layout.classList.contains('hud-idle'));
    assert.ok(!v.element('min-stamina').classList.contains('hidden'));

    c.updateMinimalStats(199, 200, 300, 300, 99.1, 100, false, true, false);
    c.updateMinimalStats(199, 200, 298.2, 300, 99.1, 100, false, false, false);
    assert.ok(!layout.classList.contains('hud-idle'));
    assert.ok(!v.element('min-magicka').classList.contains('hidden'));
    c.updateMinimalStats(199, 200, 298.2, 300, 99.1, 100, false, false, false);
    v.advance(0);
    assert.ok(layout.classList.contains('hud-idle'));
});

test('minimalist position accepts the documented ranges', () => {
    const v = createView(), c = v.context;
    c.updateHudPosition(-500, 2000);
    assert.equal(v.element('root').style.properties.get('--side-offset-left'), '-500px');
    assert.equal(v.element('root').style.properties.get('--bottom-offset'), '2000px');
    c.updateHudPosition(-501, 0);
    assert.equal(v.errors.length, 1);
    assert.equal(v.element('root').style.properties.get('--side-offset-left'), '-500px');
});

test('poise numbers show raw values to one decimal and follow the poise bar', () => {
    const v = createView(), c = v.context;
    c.configureLayouts(true, true);
    c.configureVisibility(false, true, 0.4, 3);
    c.updateMinimalStats(100, 100, 100, 100, 100, 100, false, false);
    const label = v.element('min-poise-numbers');
    const row = v.element('min-poise');

    c.updateMinimalPoise(true, 42.54, 65, true);
    assert.equal(label.hidden, false);
    assert.equal(label.textContent, '42.5 / 65.0');
    assert.equal(row.fill.style.transform, `scaleX(${42.54 / 65})`);
    c.updateMinimalPoise(true, 72.25, 65, true);
    assert.equal(label.textContent, '72.3 / 65.0');
    assert.equal(row.fill.style.transform, 'scaleX(1)');
    v.advance(3000);
    assert.ok(v.element('minimal-layout').classList.contains('hud-idle'));

    c.updateMinimalPoise(true, 72.25, 65, false);
    assert.equal(label.hidden, true);
    c.updateMinimalPoise(true, 72.25, 65, true);
    assert.equal(label.hidden, false);
    c.updateMinimalPoise(false, 0, 0, true);
    assert.equal(label.hidden, true);
    assert.ok(row.classList.contains('hidden'));
    c.updateMinimalPoise(true, 1, 0, true);
    assert.equal(label.hidden, true);
    c.updateMinimalPoise(true, 42.5, 65, true);
    c.resetMinimal();
    assert.equal(label.hidden, true);
    assert.ok(row.classList.contains('hidden'));
    assert.equal(v.errors.length, 0);
});

test('Daedric Iron is the default theme and the theme switches live', () => {
    const v = createView(), c = v.context;
    const layout = v.element('minimal-layout');
    c.configureLayouts(true, true);
    c.configureHudTheme('minimalist');
    assert.ok(layout.classList.contains('theme-minimalist'));
    assert.ok(!layout.classList.contains('theme-daedric'));
    c.configureHudTheme('daedric');
    assert.ok(layout.classList.contains('theme-daedric'));
    assert.ok(!layout.classList.contains('theme-minimalist'));
    c.configureHudTheme('Daedric Iron');
    c.configureHudTheme(null);
    assert.equal(v.errors.length, 2);
    assert.ok(layout.classList.contains('theme-daedric'));
});

test('the Daedric light gem core follows light level, sneaking, the toggle and live theme changes', () => {
    const v = createView(), c = v.context;
    const core = v.element('daedric-light-gem'), dark = v.element('light-gem');
    c.configureLayouts(true, true);
    c.updateLightGem(true, 75);
    assert.equal(core.hidden, false);
    assert.equal(dark.hidden, true);
    assert.equal(core.style.opacity, String(0.25 + 0.75 * 0.75));
    assert.equal(core.style.boxShadow, `0 0 12px rgba(240, 198, 92, ${0.2 + 0.75 * 0.75})`);
    assert.equal(core.attributes['aria-label'], 'Light level: 75 out of 100');
    c.updateLightGem(true, 0);
    assert.equal(core.style.opacity, '0.25');
    assert.equal(core.style.boxShadow, '0 0 0px rgba(240, 198, 92, 0.2)');
    c.updateLightGem(true, 100);
    assert.equal(core.style.opacity, '1');
    assert.equal(core.style.boxShadow, '0 0 16px rgba(240, 198, 92, 0.95)');
    c.updateLightGem(true, null);
    assert.equal(core.hidden, true);
    c.updateLightGem(false, 50);
    assert.equal(core.hidden, true);

    // Switching themes while sneaking moves the gem without a new telemetry update; Minimalist stays untouched.
    c.updateLightGem(true, 60);
    c.configureHudTheme('minimalist');
    assert.equal(core.hidden, true);
    assert.equal(dark.hidden, false);
    assert.equal(v.element('light-gem-fill').attributes.height, String(34 * 60 / 100));
    c.configureHudTheme('daedric');
    assert.equal(core.hidden, false);
    assert.equal(dark.hidden, true);

    c.configureLayouts(true, false);
    assert.equal(core.hidden, true);
    c.configureLayouts(true, true);
    assert.equal(core.hidden, true);
    c.updateLightGem(true, 40);
    c.resetMinimal();
    assert.equal(core.hidden, true);
    assert.equal(v.errors.length, 0);
});

test('the HUD scale is validated and applied to the whole cluster for either theme', () => {
    const v = createView(), c = v.context;
    const layout = v.element('minimal-layout');
    for (const theme of ['daedric', 'minimalist']) {
        c.configureHudTheme(theme);
        c.configureHudScale(1.25);
        assert.equal(layout.style.properties.get('--hud-scale'), '1.25');
    }
    c.configureHudScale(0.7);
    assert.equal(layout.style.properties.get('--hud-scale'), '0.7');
    c.configureHudScale(1.5);
    assert.equal(layout.style.properties.get('--hud-scale'), '1.5');
    for (const bad of [0.69, 1.51, NaN, Infinity, '1.2', null]) c.configureHudScale(bad);
    assert.equal(v.errors.length, 6);
    assert.equal(layout.style.properties.get('--hud-scale'), '1.5');
});

test('the Daedric crest follows the same stealth visibility as the Minimalist diamond', () => {
    const v = createView(), c = v.context;
    const emblem = v.element('min-emblem'), core = v.element('daedric-light-gem'), dark = v.element('light-gem');
    const run = theme => {
        c.configureLayouts(true, true);
        c.configureHudTheme(theme);
        const gem = theme === 'daedric' ? core : dark;
        // The diamond frame is shown whenever the light gem option is on; only the light gem follows sneaking.
        assert.equal(emblem.hidden, false);
        assert.equal(gem.hidden, true);
        c.updateLightGem(true, 55);
        assert.equal(emblem.hidden, false);
        assert.equal(gem.hidden, false);
        c.updateLightGem(true, null);
        assert.equal(gem.hidden, true);
        c.updateLightGem(true, 90);
        assert.equal(gem.hidden, false);
        c.updateLightGem(false, 90);
        assert.equal(emblem.hidden, false);
        assert.equal(gem.hidden, true);
        c.configureLayouts(true, false);
        assert.equal(emblem.hidden, true);
        assert.equal(gem.hidden, true);
    };
    run('minimalist');
    run('daedric');
    assert.equal(v.errors.length, 0);
});

test('Daedric bar frames keep fixed caps and non-scaling rails while bar widths scale', () => {
    const v = createView(), c = v.context;
    const frame = c.daedricFrameMarkup();
    assert.equal((frame.match(/class="dd-cap/g) || []).length, 2);
    assert.equal((frame.match(/class="dd-cap right"/g) || []).length, 1);
    assert.equal((frame.match(/viewBox="0 0 26 24"/g) || []).length, 2);
    assert.equal((frame.match(/vector-effect="non-scaling-stroke"/g) || []).length, 4);
    assert.match(frame, /<svg class="dd-mid-svg" viewBox="0 0 100 24" preserveAspectRatio="none">/);
    assert.match(frame, /<svg class="dd-sigil" viewBox="0 0 96 24">/);
    // Only the middle rails stretch; the caps and the sigil carry their own fixed viewBox and size.
    assert.ok(frame.indexOf('dd-mid-svg') > frame.indexOf('dd-cap'));
    assert.doesNotMatch(frame.slice(frame.indexOf('dd-cap'), frame.indexOf('dd-mid')), /preserveAspectRatio/);

    c.configureLayouts(true, true);
    c.configureHudTheme('daedric');
    c.updateMinimalStats(200, 200, 300, 300, 100, 100, true, false);
    assert.equal(v.element('min-health').style.width, '250px');
    assert.equal(v.element('min-magicka').style.width, '300px');
    c.updateMinimalStats(60, 200, 150, 300, 70, 100, true, false);
    assert.equal(v.element('min-health').fill.style.transform, 'scaleX(0.3)');
    assert.equal(v.element('min-health').trail.style.transform, 'scaleX(1)');
    assert.equal(v.errors.length, 0);
});
