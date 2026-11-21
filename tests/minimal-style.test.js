const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const read = file => fs.readFileSync(path.join(__dirname, '..', file), 'utf8').replace(/\r\n/g, '\n');
const css = read('view/minimal.css'), html = read('view/index.html'), js = read('view/minimal.js');

const barHeight = id => Number(css.match(new RegExp(`#minimal-layout #${id}[^{]*\\{ --bar-h: (\\d+)px; \\}`))?.[1] ??
    css.match(new RegExp(`#minimal-layout #min-magicka, #minimal-layout #min-stamina \\{ --bar-h: (\\d+)px; \\}`))?.[1]);

test('resource bars are fatter than before and Poise stays the slimmest, with 2px rails around the inner bar', () => {
    const health = barHeight('min-health');
    const magickaStamina = barHeight('min-magicka');
    const poise = Number(css.match(/#minimal-layout \.min-thin \{ --bar-h: (\d+)px;/)[1]);
    // Inner bar height is the row height minus the two 2px rails (previously 12px for resources and 6px for Poise).
    const inner = row => row - 4;
    assert.equal(inner(health), 15);
    assert.equal(inner(magickaStamina), 13);
    assert.equal(inner(poise), 9);
    assert.ok(inner(health) > inner(magickaStamina) && inner(magickaStamina) > inner(poise));
    assert.ok(inner(health) > 12 && inner(magickaStamina) > 12 && inner(poise) > 6);
    assert.match(css, /#minimal-layout \.min-track \{[^}]*border-top: 2px solid[^}]*border-bottom: 2px solid/);
    assert.match(css, /#minimal-layout #min-magicka, #minimal-layout #min-stamina \{ --bar-h: 17px; \}/);
});

test('end caps scale with the row height and carry a pewter edge halo', () => {
    assert.match(css, /#minimal-layout \.min-cap \{[^}]*width: calc\(var\(--bar-h\) \* 1\.375\); height: var\(--bar-h\);[^}]*drop-shadow\(0 0 \.6px rgba\(185,192,202,\.85\)\)/);
    assert.match(css, /#minimal-layout \.min-thin \.min-cap \{ width: calc\(var\(--bar-h\) \* 1\.4\); height: var\(--bar-h\); \}/);
    // The cap art keeps its aspect ratio at every row height (22x16 and 14x10).
    for (const [ratio, art] of [[1.375, 22 / 16], [1.4, 14 / 10]]) assert.equal(ratio, art);
});

test('the track has a pewter bevel, a deeper empty interior and a dark outer shadow', () => {
    assert.match(css, /--min-edge-hi: #6b717a; --min-edge-lo: #23262b;/);
    assert.match(css, /#minimal-layout \.min-track \{[^}]*background: rgba\(8,9,12,\.88\);[^}]*border-top: 2px solid var\(--min-edge-hi\); border-bottom: 2px solid var\(--min-edge-lo\);[^}]*box-shadow: var\(--min-track-depth\);/);
    assert.match(css, /--min-track-depth: inset 0 2px 3px rgba\(0,0,0,\.85\)/);
    assert.match(css, /#minimal-layout \.min-row \{[^}]*drop-shadow\(0 1px var\(--min-drop-blur\)[^}]*\) drop-shadow\(0 0 1px rgba\(0,0,0,\.9\)\);/);
    // The Poise debuff treatments were rebuilt on the same variables.
    assert.match(css, /debuff-numbed \.min-track \{\s*box-shadow: var\(--min-track-depth\), inset 0 0 6px 1px rgba\(63, 181, 200, \.8\);/);
    assert.match(css, /@keyframes pd-bleed-pulse \{\s*0%, 100% \{ border-top-color: var\(--min-edge-hi\); border-bottom-color: var\(--min-edge-lo\); \}\s*50% \{ border-color: #c43a3a; \}/);
    assert.match(css, /debuff-scorched \{[^}]*drop-shadow\(0 0 4px rgba\(224, 122, 42, \.9\)\)/);
});

test('the damage trail is a bright phantom chunk on every bar', () => {
    assert.match(css, /#minimal-layout \.min-trail \{ background: rgba\(255, 245, 235, \.78\); opacity: 1; \}/);
    assert.doesNotMatch(css, /\.min-trail \{[^}]*var\(--c1\)/);
});

test('the light diamond is scaled up to balance the taller bar stack', () => {
    assert.match(css, /#minimal-layout #min-emblem \{\s*position: relative; flex: none; width: 66px; height: 66px;/);
    assert.match(css, /#minimal-layout \.emblem-frame \{ position: absolute; top: 0; left: 0; width: 66px; height: 66px; \}/);
    assert.match(css, /#minimal-layout #light-gem \{\s*position: absolute; top: 11px; left: 11px; width: 44px; height: 44px;/);
    assert.match(css, /max-width: calc\(100vw - 106px - var\(--side-offset-left, 20px\)\)/);
});

test('every Poise debuff fill color is still defined at the slimmer bar height and the structure is unchanged', () => {
    for (const [id, color] of [['sapped', '#9d7ae0'], ['enfeebled', '#5fc24a'], ['numbed', '#4cc9e0'],
        ['hemorrhaging', '#c94040'], ['scorched', '#e87722']]) {
        assert.match(css, new RegExp(`#min-poise\\.debuff-${id} \\.min-poise \\{ --c1: ${color};`));
    }
    // Styling only: the same rows, one poise row, no new timers.
    for (const id of ['min-health', 'min-magicka', 'min-stamina', 'min-poise']) {
        assert.equal((html.match(new RegExp(`id="${id}"`, 'g')) || []).length, 1, id);
    }
    assert.equal((html.match(/class="min-row/g) || []).length, 4);
    assert.equal((js.match(/setInterval\(/g) || []).length, 1);
});

test('faded Daedric resource rows and frame filters disappear completely without hiding the emblem', () => {
    assert.match(css, /#minimal-layout\.theme-daedric\.hud-idle #min-health,[^}]*#min-stamina,\s*#minimal-layout\.theme-daedric \.min-row\.hidden \{\s*opacity: 0; filter: none;\s*\}/);
    assert.match(css, /#minimal-layout\.theme-daedric\.hud-idle #min-health,/);
    assert.doesNotMatch(css, /#minimal-layout\.theme-daedric\.hud-idle #min-emblem/);
});

test('Daedric Iron art matches the prototype and the Minimalist rules stay untouched underneath', () => {
    for (const id of ['ironGrad', 'silverBevel', 'spearBlade']) assert.match(html, new RegExp(`<linearGradient id="${id}" x1="0" y1="0" x2="0" y2="1">`));
    assert.match(html, /<stop offset="48%" stop-color="#8b909c" \/>/);
    assert.match(html, /<div id="minimal-layout" class="theme-daedric" hidden>/);
    assert.match(html, /<svg class="dd-crest" viewBox="0 0 68 68"/);
    assert.match(html, /<path fill-rule="evenodd" d="M34,8 L60,34 L34,60 L8,34 Z M34,17 L51,34 L34,51 L17,34 Z" fill="url\(#ironGrad\)"/);
    assert.match(html, /<div id="daedric-light-gem" class="light-gem-core"/);
    assert.match(html, /id="light-gem"/);
    assert.match(css, /\.light-gem-core \{[^}]*width: 23px; height: 23px;[^}]*rotate\(45deg\);[^}]*radial-gradient\(circle, #f7e4a3 0%, #c89d3c 65%, #5c4314 100%\)/);
    // Scaled about 15% from the prototype: 24 -> 28px bars, 26 -> 30px caps, 96x24 -> 112x28 sigil, 68 -> 78px crest.
    assert.match(css, /\.theme-daedric #min-emblem \{ width: 78px; height: 78px;/);
    assert.match(css, /#min-stamina, #minimal-layout\.theme-daedric #min-poise \{ --bar-h: 28px; \}/);
    assert.match(css, /\.dd-cap \{ width: 30px; height: 28px; flex: none;/);
    assert.match(css, /\.dd-mid \{[^}]*height: 28px;/);
    assert.match(css, /\.dd-sigil \{[^}]*width: 112px; height: 28px;/);
    assert.match(css, /\.theme-daedric \.min-track \{[^}]*left: 23px; right: 23px; top: 6px; bottom: 6px;/);
    // The light core still fits inside the crest's inner cutout (half-diagonal 17 of 68, scaled to the 78px crest).
    assert.ok(23 * Math.SQRT2 / 2 < 17 * 78 / 68);
    // Etched runes on the crest, caps and sigils are brightened for daylight backgrounds.
    assert.match(html, /<g stroke="#cfd5e0" stroke-width="0\.9" fill="none" stroke-linecap="round" opacity="0\.95">/);
    assert.match(js, /stroke="#cfd5e0" stroke-width="0\.[0-9]+"/);
    assert.match(js, /<g stroke="#cfd5e0" stroke-width="0\.85" fill="none" stroke-linecap="round" opacity="0\.95">/);
    assert.doesNotMatch(html + js, /#9ca3b0|#a5acb8/);
    // Daedric-only elements are hidden unless the Daedric theme class is on, so Minimalist keeps its own look.
    assert.match(css, /#minimal-layout \.dd-crest, #minimal-layout \.dd-frame, #minimal-layout \.light-gem-core \{ display: none; \}/);
    // Every Daedric selector is scoped to .theme-daedric so Minimalist rules are unaffected.
    const daedric = css.slice(css.indexOf('/* Daedric Iron theme'), css.indexOf('/* Reactive resource bars.'));
    for (const selector of daedric.split('{').slice(0, -1).map(part => part.split('}').pop().trim()).filter(s => s && !s.startsWith('/*') && !s.startsWith('@') && !/^\d|^0%/.test(s))) {
        for (const part of selector.split(',')) assert.match(part.trim(), /theme-daedric|dd-crest|dd-frame|light-gem-core|poise-shield/, part);
    }
    // The shared minimalist structure and reactive hooks remain: the frame overlay is added to, not swapped for, the bars.
    assert.match(js, /<div class="min-track"><div class="min-layer min-trail min-\$\{key\}"><\/div>/);
    assert.match(css, /#minimal-layout #min-health\.health-poison \.min-health \{ --c1: #7ab82e;/);
    assert.match(css, /#minimal-layout\.show-notches \.min-track::after \{ display: block; \}/);
    assert.doesNotMatch(html, /id="poise-shield-(?:fill-grad|trail-grad|specular-grad|cavity-clip)"/);
    assert.match(html, /<div id="poise-shield-cavity"/);
    assert.match(html, /<div id="poise-shield-fill" class="poise-shield-liquid poise-shield-fill"><\/div>/);
    assert.match(js, /setProperty\('--shield-c1', c1\)/);
    assert.match(js, /setProperty\('--shield-c2', c2\)/);
    assert.match(js, /poise-shield-effect-frame'\)\.setAttribute\('stroke', stroke\)/);
});
