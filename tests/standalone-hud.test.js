const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const root = path.join(__dirname, '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8');

test('the only shipped layout is the minimalist view', () => {
    const html = read('view/index.html');
    assert.match(html, /minimal\.css/);
    assert.match(html, /minimal\.js/);
    assert.doesNotMatch(html, /arpg-layout|health_frame|magicka_frame|hotbar_frame|script\.js|styles\.css/);
    for (const file of ['view/script.js', 'view/styles.css', 'src/hotbar.cpp', 'src/hotbar.h']) {
        assert.equal(fs.existsSync(path.join(root, file)), false, file);
    }
    const build = read('xmake.lua');
    assert.doesNotMatch(build, /"view", "\*"/);
    for (const file of ['index.html', 'minimal.css', 'minimal.js']) assert.ok(build.includes(`"view", "${file}"`));
});

test('the HUD scale is configured, persisted, reset, selectable live and sent to the view', () => {
    const ini = read('TruePoise.ini'), main = read('src/main.cpp'), menu = read('src/menu_framework.cpp');
    assert.match(ini.split('[Poise]')[0], /^fHudScale=1\.00$/m);
    assert.match(main, /float hudScale = 1\.0F;/);
    assert.match(main, /readRange\(L"fHudScale", "fHudScale", settings\.hudScale, 0\.70F, 1\.50F\)/);
    assert.match(main, /configureHudScale\(\{:\.2f\}\)/);
    assert.equal((main.match(/HudScaleScript\(\)/g) || []).length, 3);
    assert.match(main, /float\* get_hudScale\(\) \{ return &settings\.hudScale; \}/);
    assert.match(menu, /SliderFloat\("HUD Scale##layout", hudScale, 0\.70f, 1\.50f, "%\.2f"\)/);
    assert.match(menu, /std::round\(\*hudScale \* 20\.0f\) \/ 20\.0f/);
    assert.match(menu, /QueueIniWrite\("fHudScale", FloatToString\(\*hudScale\), "HUD"\)/);
    assert.match(menu, /QueueIniWrite\("fHudScale", "1\.00", "HUD"\)/);
    const css = read('view/minimal.css');
    assert.match(css, /#minimal-layout \{[^}]*transform: scale\(var\(--hud-scale, 1\)\); transform-origin: left bottom;/);
});

test('the custom player HUD is enabled by default and can be disabled without disabling standalone widgets', () => {
    const ini = read('TruePoise.ini'), main = read('src/main.cpp');
    const menu = read('src/menu_framework.cpp'), js = read('view/minimal.js'), css = read('view/minimal.css');
    assert.match(ini.split('[Poise]')[0], /^bEnableCustomHud=true$/m);
    assert.match(main, /bool enableCustomHud = true;/);
    assert.match(main, /readBool\(L"bEnableCustomHud", "bEnableCustomHud", settings\.enableCustomHud\)/);
    assert.match(main, /bool\* get_enableCustomHud\(\) \{ return &settings\.enableCustomHud; \}/);
    assert.match(menu, /Checkbox\("Enable Custom Player HUD##widgets", enableCustomHud\)/);
    assert.match(menu, /QueueIniWrite\("bEnableCustomHud", val, "HUD"\)/);
    assert.match(menu, /QueueIniWrite\("bEnableCustomHud", "true", "HUD"\)/);
    assert.match(menu, /QueueIniWrite\("bEnablePoiseShield", "true", "HUD"\)/);
    assert.match(menu, /QueueIniWrite\("bShowPoiseTelemetry", "false", "HUD"\)/);
    assert.match(menu, /QueueIniWrite\("bEnableHitPushBack", "false", "Poise"\)/);
    assert.match(js, /function configureCustomHud\(enabled\)/);
    assert.match(js, /classList\.toggle\('custom-hud-disabled', !enabled\)/);
    assert.match(main, /configureCustomHud\(\{\}\)/);
    assert.match(css, /#minimal-layout\.custom-hud-disabled #min-health,[\s\S]*?#min-emblem,[\s\S]*?display: none !important;/);
    assert.doesNotMatch(css, /custom-hud-disabled #(?:world-widget|min-poise|poise-shield-widget|target-poise|poise-telemetry)/);
    assert.match(main, /if \(!settings\.enableCustomHud\) \{[\s\S]*?RestoreVanillaMeters\(\)/);
    assert.match(main, /GetDisplayInfo\(&vanillaMeterState\.original\[i\]\)/);
    assert.match(main, /SetDisplayInfo\(vanillaMeterState\.original\[i\]\)/);
    assert.doesNotMatch(main, /TrueHUD.*GetModuleHandle|GetModuleHandle.*TrueHUD/);
});

test('removed layout settings do not appear in the runtime, menu or shipped ini', () => {
    const sources = ['TruePoise.ini', 'src/main.cpp', 'src/menu_framework.cpp'].map(read);
    const obsolete = /iHudStyle|fGlobeScale|fHotbarScale|iSideOffsetRight|iHotbarBottomOffset|bGlassEffects|fGlassShineOpacity|fMeniscusGlow|fOrbInnerShadow|bShowVitalNumbers/;
    for (const source of sources) assert.doesNotMatch(source, obsolete);
    for (const key of ['iSideOffsetLeft', 'iBottomOffset', 'bDynamicHUD', 'bAlwaysOn',
        'fIdleOpacity', 'fFadeDelaySeconds', 'bMinBarFiligree', 'fMinBarShine',
        'fMinBarShadow', 'bMinBarNotches']) {
        for (const source of sources) assert.ok(source.includes(key), key);
    }
});

test('every packaged variant, diagnostics included, is self-contained with the view, ini and fomod info', () => {
    const build = read('xmake.lua');
    const blocks = build.split(/^target\(/m).slice(1);
    assert.ok(blocks.length >= 4);
    for (const block of blocks) {
        const name = block.slice(0, block.indexOf(')'));
        if (!block.includes('after_build')) continue;
            // The rogue-stagger diagnostic is a deliberate DLL/PDB-only overlay; it is not a shipped player package.
            if (name.replace(/"/g, '') === 'arpghud_rogue_stagger_diagnostic') continue;
            for (const file of ['index.html', 'minimal.css', 'minimal.js']) {
                assert.ok(block.includes(`"view", "${file}"`), `${name} does not package view/${file}`);
            }
            assert.ok(block.includes('"TruePoise.ini"'), `${name} does not package TruePoise.ini`);
            assert.ok(block.includes('"PrismaUI", "views", VIEW_NAME'), `${name} does not package into the PrismaUI view folder`);
        }
});

test('TruePoise identity and view path are consistent across build, plugin and menu registration', () => {
    const build = read('xmake.lua'), main = read('src/main.cpp'), menu = read('src/menu_framework.cpp');
    assert.match(build, /local PLUGIN_NAME = "TruePoise"/);
    assert.match(build, /local PLUGIN_VERSION = "0\.9\.0"/);
    assert.match(build, /local VIEW_NAME = "TruePoise"/);
    assert.match(main, /constexpr auto kViewPath = "TruePoise\/index\.html"/);
    assert.match(menu, /Model::Internal::key = "TruePoise"/);
});

test('no poise broadcast was introduced', () => {
    assert.equal(fs.existsSync(path.join(root, 'src', 'PoiseMessagingAPI.h')), false);
    assert.doesNotMatch(read('src/main.cpp'), /BroadcastPoise|MessagingInterface.*Dispatch|->Dispatch\(/);
});

test('poise numbers are configured, persisted, reset and sent when toggled', () => {
    const ini = read('TruePoise.ini'), main = read('src/main.cpp'), menu = read('src/menu_framework.cpp');
    assert.match(ini.split('[Poise]')[0], /bShowPoiseNumbers=false/);
    assert.match(main, /bool showPoiseNumbers = false/);
    assert.match(main, /readBool\(L"bShowPoiseNumbers", "bShowPoiseNumbers", settings\.showPoiseNumbers\)/);
    assert.match(main, /settings\.showPoiseNumbers == lastPoiseNumbers/);
    assert.match(main, /updateMinimalPoise\(\{\}, \{:\.9g\}, \{:\.9g\}, \{\}\)/);
    assert.match(main, /changedKey == "bShowPoiseNumbers"[\s\S]*?SendPoise\(\)/);
    assert.match(menu, /Checkbox\("Show Poise Numbers##widgets", showPoiseNumbers\)/);
    assert.match(menu, /QueueIniWrite\("bShowPoiseNumbers", val, "HUD"\)/);
    assert.match(menu, /QueueIniWrite\("bShowPoiseNumbers", "false", "HUD"\)/);
    const reload = main.slice(main.indexOf('void ReloadHud()'), main.indexOf('void RegisterReloadKey()\n\t{'));
    assert.match(reload, /LoadSettings\(\)/);
    assert.match(reload, /hasSentPoise = false/);
});

test('the HUD theme is configured, persisted, reset, selectable live and sent to the view', () => {
    const ini = read('TruePoise.ini'), main = read('src/main.cpp'), menu = read('src/menu_framework.cpp');
    assert.match(ini.split('[Poise]')[0], /sHudTheme=Daedric Iron/);
    assert.match(ini, /"Minimalist"/);
    assert.match(main, /int hudTheme = 0;/);
    assert.match(main, /kHudThemeNames\{ "Daedric Iron", "Minimalist" \}/);
    assert.match(main, /readValue\(L"sHudTheme"\)/);
    assert.match(main, /configureHudTheme\(\\"\{\}\\"\)/);
    assert.equal((main.match(/HudThemeScript\(\)/g) || []).length, 3);
    assert.match(main, /int\* get_hudTheme\(\) \{ return &settings\.hudTheme; \}/);
    assert.match(menu, /Combo\("HUD Theme##widgets", hudTheme, "Daedric Iron\\0Minimalist\\0"\)/);
    assert.match(menu, /QueueIniWrite\("sHudTheme", \*hudTheme == 1 \? "Minimalist" : "Daedric Iron", "HUD"\)/);
    assert.match(menu, /QueueIniWrite\("sHudTheme", "Daedric Iron", "HUD"\)/);
});
