const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

const root = path.join(__dirname, '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replace(/\r\n/g, '\n');

function createView() {
    const elements = new Map();
    const errors = [];
    let now = 0, tick, nextTimer = 0;
    const timers = new Map();
    function element(id) {
        if (!elements.has(id)) {
            const classes = new Set();
            const properties = new Map();
            elements.set(id, {
                id, hidden: false, textContent: '', attributes: {}, dataset: {}, offsetWidth: 0,
                style: { setProperty: (key, value) => properties.set(key, value), properties },
                classList: {
                    toggle: (key, on) => on ? classes.add(key) : classes.delete(key),
                    contains: key => classes.has(key)
                },
                setAttribute(name, value) { this.attributes[name] = value; },
                appendChild(child) { child.parentElement = this; }
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
        document: {
            getElementById: element,
            querySelectorAll: () => rows,
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
    vm.runInContext(read('view/minimal.js'), context);
    return {
        context, element, errors,
        advance(ms) {
            now += ms;
            for (const [id, timer] of timers) {
                if (timer.due <= now) {
                    timers.delete(id);
                    timer.callback();
                }
            }
            tick();
        }
    };
}

test('shield SVG resources are unique and the debuff strip remains outside the poise row', () => {
    const html = read('view/index.html');
    for (const id of ['ironGrad', 'silverBevel', 'spearBlade']) {
        assert.equal((html.match(new RegExp(`id="${id}"`, 'g')) || []).length, 1, id);
    }
    assert.doesNotMatch(html, /poise-shield-fill-(?:golden|sapped|poison|fire|cold|bleed)|poise-shield-trail-grad|poise-shield-specular-grad|poise-shield-cavity-clip/);
    for (const id of ['poise-shield-stage', 'poise-shield-cavity', 'poise-shield-fill',
        'poise-shield-trail', 'poise-shield-meniscus', 'poise-shield-ceiling']) {
        assert.equal((html.match(new RegExp(`id="${id}"`, 'g')) || []).length, 1, id);
    }
    assert.equal((html.match(/class="poise-shield-tick /g) || []).length, 3);
    assert.match(html, /<div id="poise-shield-fill" class="poise-shield-liquid poise-shield-fill"><\/div>/);
    assert.doesNotMatch(html, /<rect id="poise-shield-(?:fill|trail)"/);
    assert.equal((html.match(/id="min-poise-numbers"/g) || []).length, 1);
    const rowStart = html.indexOf('id="min-poise"');
    const rowEnd = html.indexOf('</div>', rowStart);
    const icons = html.indexOf('id="poise-debuff-icons"');
    assert.ok(rowStart > -1 && rowEnd > rowStart && icons > rowEnd);
    assert.ok(icons < html.indexOf('id="poise-shield-widget"'));
});

test('shield placement, geometry, trail and theme visibility follow live poise', () => {
    const view = createView();
    view.context.configureLayouts(false, true);
    view.context.configureVisibility(true, true, 0, 3);
    view.context.configurePoiseShield(true, 14, 22);
    view.context.updateMinimalPoise(true, 100, 100, true);
    assert.equal(view.element('minimal-layout').style.properties.get('--poise-shield-x'), '14px');
    assert.equal(view.element('minimal-layout').style.properties.get('--poise-shield-y'), '22px');
    assert.equal(view.element('poise-shield-widget').hidden, false);
    assert.equal(view.element('poise-shield-cavity').style.properties.get('--shield-fill-level'), '100.000%');

    view.context.updateMinimalPoise(true, 50, 100, true);
    assert.equal(view.element('poise-shield-cavity').style.properties.get('--shield-fill-level'), '50.000%');
    assert.equal(view.element('poise-shield-cavity').style.properties.get('--shield-trail-level'), '100.000%');
    view.advance(601);
    view.advance(33);
    assert.equal(view.element('poise-shield-cavity').style.properties.get('--shield-trail-level'), '90.500%');
    assert.equal(view.element('poise-shield-meniscus').style.opacity, '0.9');

    view.context.configureHudTheme('minimalist');
    assert.equal(view.element('poise-shield-widget').hidden, true);
    assert.equal(view.element('min-poise').classList.contains('hidden'), false);
    assert.equal(view.element('min-poise-numbers').parentElement, view.element('min-poise'));
    view.context.configureHudTheme('daedric');
    assert.equal(view.element('poise-shield-widget').hidden, false);
    assert.equal(view.element('min-poise-numbers').parentElement, view.element('poise-shield-widget'));

    view.context.configurePoiseShield(false, 14, 22);
    assert.equal(view.element('poise-shield-widget').hidden, true);
    assert.equal(view.element('minimal-layout').classList.contains('poise-shield-disabled'), true);
    assert.equal(view.element('min-poise-numbers').parentElement, view.element('min-poise'));
});

test('debuff and reactive shield effects do not depend on the icon strip', () => {
    const view = createView();
    view.context.configureLayouts(false, false);
    view.context.configurePoiseDebuffIcons(false);
    view.context.updatePoiseDebuffs(['numbed', 'scorched'], 0.75);
    const shield = view.element('poise-shield-widget');
    assert.ok(shield.classList.contains('debuff-numbed'));
    assert.ok(shield.classList.contains('debuff-scorched'));
    assert.equal(view.element('poise-shield-ceiling').style.opacity, '0');

    view.context.configureReactiveBars(true);
    view.context.updateReactiveBars(['health-poison', 'stamina-frost', 'health-fire']);
    assert.ok(shield.classList.contains('effect-poison'));
    assert.ok(shield.classList.contains('effect-cold'));
    assert.ok(shield.classList.contains('effect-fire'));
    assert.equal(view.element('poise-shield-cavity').style.properties.get('--shield-c1'), '#d7ff5c');
    assert.equal(view.element('poise-shield-cavity').style.properties.get('--shield-c2'), '#3f8c16');
    assert.equal(view.element('poise-shield-effect-frame').attributes.stroke, '#c4ff4d');
    view.context.updateReactiveBars(['bar-shock']);
    assert.equal(shield.classList.contains('effect-shock'), false);

    view.context.updatePoiseDebuffs(['enfeebled'], 0.75);
    assert.equal(view.element('poise-shield-widget').style.properties.get('--poise-shield-ceiling'), '75.000%');
    assert.equal(view.element('poise-shield-ceiling').style.opacity, '0.95');
    assert.equal(view.element('poise-debuff-icons').hidden, true);
    view.context.updatePoiseDebuffs([], 1);
    view.context.configureReactiveBars(false);
    assert.equal(view.element('poise-shield-cavity').style.properties.get('--shield-c1'), '#ffe066');
    assert.equal(view.element('poise-shield-cavity').style.properties.get('--shield-c2'), '#8a6410');
    assert.equal(view.element('poise-shield-effect-frame').attributes.stroke, '#d8b54b');
});

test('stagger animation is entered only on a positive-to-zero transition', () => {
    const view = createView();
    view.context.configureLayouts(false, false);
    view.context.updateMinimalPoise(true, 100, 100);
    const shield = view.element('poise-shield-widget');
    assert.equal(shield.classList.contains('broken'), false);
    view.context.updateMinimalPoise(true, 0, 100);
    assert.equal(shield.classList.contains('broken'), true);
    view.context.updateMinimalPoise(true, 0, 100);
    assert.equal(shield.classList.contains('broken'), true);
    view.context.updateMinimalPoise(true, 20, 100);
    assert.equal(shield.classList.contains('broken'), true);
    view.advance(360);
    assert.equal(shield.classList.contains('broken'), false);
});

test('shield settings are shipped, loaded, reset, exported and sent to the view', () => {
    const ini = read('TruePoise.ini');
    const main = read('src/main.cpp');
    const menu = read('src/menu_framework.cpp');
    const css = read('view/minimal.css');
    assert.match(ini.split('[Poise]')[0], /^bEnablePoiseShield=true$/m);
    assert.match(ini.split('[Poise]')[0], /^iPoiseShieldOffsetX=0$/m);
    assert.match(ini.split('[Poise]')[0], /^iPoiseShieldOffsetY=0$/m);
    assert.match(main, /configurePoiseShield\(\{\}, \{\}, \{\}\)/);
    assert.match(main, /readBool\(L"bEnablePoiseShield", "bEnablePoiseShield", settings\.enablePoiseShield\)/);
    assert.match(main, /readOffset\(L"iPoiseShieldOffsetX", "iPoiseShieldOffsetX", settings\.poiseShieldOffsetX, -500\)/);
    assert.match(main, /readOffset\(L"iPoiseShieldOffsetY", "iPoiseShieldOffsetY", settings\.poiseShieldOffsetY, -200\)/);
    assert.match(main, /get_enablePoiseShield\(\)/);
    assert.match(menu, /Checkbox\("Enable Forged Shield Crest##widgets"/);
    assert.match(menu, /SliderInt\("Shield X Offset##layout"/);
    assert.match(menu, /SliderInt\("Shield Y Offset##layout"/);
    for (const key of ['bEnablePoiseShield', 'iPoiseShieldOffsetX', 'iPoiseShieldOffsetY']) {
        assert.ok(menu.includes(`QueueIniWrite("${key}"`));
    }
    assert.match(css, /left: calc\(100% \+ 12px\); bottom: 0;/);
    assert.match(css, /#minimal-layout\.theme-daedric\.poise-shield-disabled #min-poise \{ display: flex; \}/);
    assert.match(css, /hud-scale, 1\) - 222px/);
    assert.match(css, /#minimal-layout\.theme-daedric #poise-shield-widget\[hidden\] \{ display: none; \}/);
    assert.match(css, /background-image:\s*linear-gradient\(to bottom, rgba\(255,255,255,var\(--min-shine-alpha\)\)/);
    assert.match(css, /box-shadow: var\(--min-track-depth\)/);
    assert.match(css, /clip-path: polygon/);
    assert.match(css, /z-index: 0;[^}]*poise-shield-tick|poise-shield-tick \{[^}]*z-index: 0/s);
});
