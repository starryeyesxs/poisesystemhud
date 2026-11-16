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
                classList: { toggle: (key, on) => on ? classes.add(key) : classes.delete(key), contains: key => classes.has(key) },
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
    c.updateLightGem(true, 100);
    assert.equal(v.element('light-gem').hidden, false);
    assert.equal(v.element('light-gem-fill').attributes.height, '34');
    c.updateLightGem(true, null);
    assert.equal(v.element('light-gem').hidden, true);
    c.updateLightGem(false, 50);
    assert.equal(v.element('light-gem').hidden, true);
    c.configureLayouts(true, false);
    c.updateLightGem(true, 100);
    assert.equal(v.element('light-gem').hidden, true);
    assert.equal(v.errors.length, 0);
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
