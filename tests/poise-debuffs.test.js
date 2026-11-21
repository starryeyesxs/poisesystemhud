const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

const root = path.join(__dirname, '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replace(/\r\n/g, '\n');
const IDS = ['sapped', 'enfeebled', 'numbed', 'hemorrhaging', 'scorched'];

// The same restricted DOM the minimalist layout tests use: getElementById, hidden, classList.toggle/contains,
// style.setProperty and timers only. Anything else the icon code touched would throw.
function createView() {
    const elements = new Map();
    const errors = [];
    let now = 0, nextTimer = 0;
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
        return row;
    });
    const context = {
        document: {
            getElementById: element, querySelectorAll: () => rows,
            documentElement: { style: { setProperty() {} } }
        },
        console: { error: (...args) => errors.push(args) },
        performance: { now: () => now },
        setInterval() {},
        setTimeout: (callback, delay) => { const id = ++nextTimer; timers.set(id, { callback, due: now + delay }); return id; },
        clearTimeout: id => timers.delete(id)
    };
    vm.createContext(context);
    vm.runInContext(read('view/minimal.js'), context);
    return { context, element, errors, advance(ms) {
        now += ms;
        for (const [id, timer] of timers) {
            if (timer.due <= now) { timers.delete(id); timer.callback(); }
        }
    } };
}

const iconOff = (view, id) => view.element(`pd-icon-${id}`).classList.contains('pd-off');

test('icons stay hidden until enabled and show only the active debuffs', () => {
    const view = createView();
    const { context } = view;
    context.updatePoiseDebuffs(['sapped', 'scorched'], 1);
    assert.equal(view.element('poise-debuff-icons').hidden, true);
    context.configurePoiseDebuffIcons(true);
    assert.equal(view.element('poise-debuff-icons').hidden, false);
    assert.deepEqual(IDS.map(id => !iconOff(view, id)), [true, false, false, false, true]);
    context.updatePoiseDebuffs(['numbed', 'hemorrhaging', 'enfeebled'], 0.7);
    assert.deepEqual(IDS.map(id => !iconOff(view, id)), [false, true, true, true, false]);
    context.updatePoiseDebuffs([], 1);
    assert.equal(view.element('poise-debuff-icons').hidden, true);
    assert.ok(IDS.every(id => iconOff(view, id)));
    context.updatePoiseDebuffs(['sapped'], 1);
    context.configurePoiseDebuffIcons(false);
    assert.equal(view.element('poise-debuff-icons').hidden, true);
    assert.ok(IDS.every(id => iconOff(view, id)));
});

test('the Enfeebled ceiling tick follows the ratio and only shows while Enfeebled', () => {
    const view = createView();
    const row = view.element('min-poise');
    view.context.configurePoiseDebuffIcons(true);
    view.context.updatePoiseDebuffs(['enfeebled'], 0.78);
    assert.equal(row.classList.contains('has-ceiling'), true);
    assert.equal(row.style.properties.get('--poise-ceiling'), '0.78');
    view.context.updatePoiseDebuffs(['enfeebled'], 1);
    assert.equal(row.classList.contains('has-ceiling'), false);
    view.context.updatePoiseDebuffs(['sapped'], 0.7);
    assert.equal(row.classList.contains('has-ceiling'), false);
    view.context.updatePoiseDebuffs(['enfeebled'], 0.6);
    view.context.configurePoiseDebuffIcons(false);
    assert.equal(row.classList.contains('has-ceiling'), false);
});

test('invalid debuff payloads are rejected without changing the icons', () => {
    const view = createView();
    view.context.configurePoiseDebuffIcons(true);
    view.context.updatePoiseDebuffs(['sapped'], 1);
    view.context.updatePoiseDebuffs(['frozen'], 1);
    view.context.updatePoiseDebuffs('sapped', 1);
    view.context.updatePoiseDebuffs(['sapped'], 1.5);
    view.context.updatePoiseDebuffs(['sapped'], NaN);
    view.context.configurePoiseDebuffIcons('on');
    assert.equal(view.errors.length, 5);
    assert.equal(iconOff(view, 'sapped'), false);
    assert.equal(view.element('poise-debuff-icons').hidden, false);
});

test('an active debuff keeps the poise row visible and the bars awake, then releases them', () => {
    const view = createView();
    const { context } = view;
    context.configureHudTheme('minimalist');
    context.configureLayouts(false, false);
    context.configureVisibility(false, true, 0, 3);
    // Before the vitals have reported, a full poise bar is hidden; an active debuff shows it.
    context.updateMinimalPoise(true, 100, 100);
    const row = view.element('min-poise');
    const layout = view.element('minimal-layout');
    assert.equal(row.classList.contains('hidden'), true);
    context.configurePoiseDebuffIcons(true);
    context.updatePoiseDebuffs(['sapped'], 1);
    assert.equal(row.classList.contains('hidden'), false);
    context.updatePoiseDebuffs([], 1);
    assert.equal(row.classList.contains('hidden'), true);
    // Once idle, the bars fade out after the delay unless a debuff is active.
    context.updateMinimalStats(100, 100, 100, 100, 100, 100, false, false);
    view.advance(5000);
    assert.equal(layout.classList.contains('hud-idle'), true);
    context.updatePoiseDebuffs(['scorched'], 1);
    assert.equal(layout.classList.contains('hud-idle'), false);
    view.advance(5000);
    assert.equal(layout.classList.contains('hud-idle'), false);
    context.updatePoiseDebuffs([], 1);
    view.advance(5000);
    assert.equal(layout.classList.contains('hud-idle'), true);
    // With the icons disabled a debuff list never wakes the bars or forces the row visible.
    context.configurePoiseDebuffIcons(false);
    context.updatePoiseDebuffs(['sapped'], 1);
    assert.equal(layout.classList.contains('hud-idle'), true);
});

test('the five 14px icons are inline, self-contained SVG with distinct colors and no new DOM calls', () => {
    const html = read('view/index.html'), css = read('view/minimal.css'), js = read('view/minimal.js');
    for (const id of IDS) {
        assert.match(html, new RegExp(`<symbol id="pd-${id}" viewBox="0 0 14 14">`));
        assert.match(html, new RegExp(`<svg id="pd-icon-${id}" class="pd-icon pd-${id} pd-off" viewBox="0 0 14 14" width="14" height="14" role="img" aria-label="[^"]+"><use href="#pd-${id}"></use></svg>`));
        assert.match(css, new RegExp(`#minimal-layout \\.pd-${id} \\{ color: #[0-9a-f]{6}; \\}`));
    }
    const colors = IDS.map(id => css.match(new RegExp(`\\.pd-${id} \\{ color: (#[0-9a-f]{6}); \\}`))[1]);
    assert.equal(new Set(colors).size, 5);
    const symbols = html.slice(html.indexOf('<symbol id="pd-sapped"'), html.indexOf('</defs>'));
    assert.doesNotMatch(symbols, /href="(?!#)|xlink|<image|<script|url\(/);
    assert.ok(html.indexOf('id="poise-debuff-icons"') > html.indexOf('id="min-poise"'));
    assert.ok(html.indexOf('id="poise-debuff-icons"') < html.indexOf('id="target-poise"'));
    assert.match(css, /#minimal-layout #poise-debuff-icons\[hidden\], #minimal-layout \.pd-icon\.pd-off \{ display: none; \}/);
    assert.match(css, /#min-poise\.has-ceiling \.min-track::before \{ display: block; \}/);
    // SVG elements have no hidden property, so the icons are toggled by class; no element creation is needed.
    const icons = js.slice(js.indexOf('const POISE_DEBUFF_IDS'), js.indexOf('function updateWorldWidget') > -1 && js.indexOf('const telemetryState') > -1 ?
        js.indexOf('function telemetryNumber') : undefined);
    assert.doesNotMatch(icons, /createElement|innerHTML|appendChild|querySelector/);
    assert.equal((js.match(/setInterval\(/g) || []).length, 1);
});

test('the poise row carries one state class per active debuff and nothing else changes', () => {
    const view = createView();
    const row = view.element('min-poise');
    const states = () => IDS.map(id => row.classList.contains(`debuff-${id}`));
    view.context.updatePoiseDebuffs(['sapped', 'scorched'], 1);
    assert.deepEqual(states(), [false, false, false, false, false]);
    view.context.configurePoiseDebuffIcons(true);
    assert.deepEqual(states(), [true, false, false, false, true]);
    view.context.updatePoiseDebuffs(['enfeebled', 'numbed', 'hemorrhaging'], 0.7);
    assert.deepEqual(states(), [false, true, true, true, false]);
    assert.equal(row.classList.contains('has-ceiling'), true);
    view.context.updatePoiseDebuffs(IDS, 0.7);
    assert.deepEqual(states(), [true, true, true, true, true]);
    view.context.updatePoiseDebuffs([], 1);
    assert.deepEqual(states(), [false, false, false, false, false]);
    view.context.updatePoiseDebuffs(['numbed'], 1);
    view.context.configurePoiseDebuffIcons(false);
    assert.deepEqual(states(), [false, false, false, false, false]);
    assert.equal(view.errors.length, 0);
});

test('icons are 24px with a 6px gap and every debuff styles the poise bar through CSS only', () => {
    const css = read('view/minimal.css'), js = read('view/minimal.js');
    assert.match(css, /#minimal-layout #poise-debuff-icons \{[^}]*gap: 6px;[^}]*min-height: 24px;/);
    assert.match(css, /#minimal-layout \.pd-icon \{[^}]*width: 24px; height: 24px; filter: drop-shadow\(/);
    assert.doesNotMatch(css, /pd-icon \{[^}]*14px/);
    // Enfeebled: ceiling tick plus a striped recovery-blocked zone from the ceiling to the end of the track.
    assert.match(css, /#min-poise \.min-track::before \{[^}]*left: calc\(var\(--poise-ceiling, 1\) \* 100%\); right: 0;[^}]*border-left: 2px solid #8ee59a;/);
    assert.match(css, /#min-poise \.min-track::before \{[^}]*repeating-linear-gradient/);
    assert.match(css, /#min-poise\.has-ceiling \.min-track::before \{ display: block; \}/);
    assert.match(css, /#min-poise\.debuff-sapped \.min-poise \{ --c1: #9d7ae0;/);
    assert.match(css, /#min-poise\.debuff-numbed \.min-track \{[^}]*rgba\(63, 181, 200,/);
    assert.match(css, /@keyframes pd-bleed-pulse \{[\s\S]*?50% \{ border-color: #c43a3a; \}/);
    assert.match(css, /#min-poise\.debuff-hemorrhaging \.min-track \{ animation: pd-bleed-pulse [\d.]+s ease-in-out infinite; \}/);
    assert.match(css, /#min-poise\.debuff-scorched \{[^}]*rgba\(224, 122, 42,/);
    // No new elements or timers: the states are plain classes on the existing row.
    assert.equal((js.match(/setInterval\(/g) || []).length, 1);
    assert.match(js, /row\.classList\.toggle\(`debuff-\$\{id\}`, iconActive\.includes\(id\)\)/);
    assert.equal((read('view/index.html').match(/id="min-poise"/g) || []).length, 1);
});

test('fill-tinting debuffs share one specificity and resolve in the order Sapped, Enfeebled, Scorched, Numbed, Hemorrhaging', () => {
    const css = read('view/minimal.css');
    const rule = id => css.indexOf(`#minimal-layout #min-poise.debuff-${id} .min-poise {`);
    assert.match(css, /#min-poise\.debuff-sapped \.min-poise \{ --c1: #9d7ae0; --c2: #4f4478; \}/);
    assert.match(css, /#min-poise\.debuff-enfeebled \.min-poise \{ --c1: #5fc24a; --c2: #[0-9a-f]{6}; \}/);
    assert.match(css, /#min-poise\.debuff-scorched \.min-poise \{ --c1: #e87722; --c2: #[0-9a-f]{6}; \}/);
    assert.match(css, /#min-poise\.debuff-numbed \.min-poise \{ --c1: #4cc9e0; --c2: #[0-9a-f]{6}; \}/);
    assert.match(css, /#min-poise\.debuff-hemorrhaging \.min-poise \{ --c1: #c94040; --c2: #[0-9a-f]{6}; \}/);
    assert.ok(rule('sapped') > 0 && rule('sapped') < rule('enfeebled') && rule('enfeebled') < rule('scorched') &&
        rule('scorched') < rule('numbed') && rule('numbed') < rule('hemorrhaging'));
    // The track and border treatments stay, including the ceiling tick and the striped recovery-blocked zone.
    assert.match(css, /#min-poise \.min-track::before \{[^}]*border-left: 2px solid #8ee59a;/);
    assert.match(css, /#min-poise \.min-track::before \{[^}]*repeating-linear-gradient/);
    assert.match(css, /#min-poise\.has-ceiling \.min-track::before \{ display: block; \}/);
    assert.match(css, /#min-poise\.debuff-numbed \.min-track \{[^}]*rgba\(63, 181, 200,/);
    assert.match(css, /#min-poise\.debuff-hemorrhaging \.min-track \{ animation: pd-bleed-pulse/);
    assert.match(css, /#min-poise\.debuff-scorched \{[^}]*rgba\(224, 122, 42,/);
});

test('[Poise Debuffs] and the HUD icon key are shipped with the specified defaults and ranges', () => {
    const ini = read('TruePoise.ini'), main = read('src/main.cpp');
    const section = ini.split('[Poise Debuffs]')[1];
    assert.ok(section && !section.includes('[Poise]') && !section.includes('[HUD]'));
    assert.match(ini.split('[Poise]')[0], /^bShowPoiseDebuffIcons=true$/m);
    const keys = {
        bEnablePoiseDebuffs: 'true', bEnableSappedDebuff: 'true', bEnableEnfeebledDebuff: 'true',
        bEnableNumbedDebuff: 'true', bEnableHemorrhagingDebuff: 'true', bEnableScorchedDebuff: 'true',
        fEnfeebledRegenCeilingMin: '0.70', fEnfeebledRegenCeilingMax: '0.85', fEnfeebledMagnitudeForMin: '10.0',
        fMinGlobalRegenCeiling: '0.60', fNumbedRegenDelayBonus: '1.0', fHemorrhagingRegenRateMultiplier: '0.65',
        sHemorrhagingEffectKeywords: 'bleed,hemorrhag', fScorchedMeleeDamageMultiplier: '1.20'
    };
    for (const [key, value] of Object.entries(keys)) {
        assert.match(section, new RegExp(`^${key}=${value.replace('.', '\\.')}$`, 'm'));
        assert.match(main, new RegExp(`L"${key}"[^\\n]*L"Poise Debuffs"`));
    }
    assert.match(main, /L"fEnfeebledRegenCeilingMin"[^\n]*0\.0F, 1\.0F, L"Poise Debuffs"/);
    assert.match(main, /L"fEnfeebledMagnitudeForMin"[^\n]*0\.1F, 1000\.0F, L"Poise Debuffs"/);
    assert.match(main, /L"fNumbedRegenDelayBonus"[^\n]*0\.0F, 10\.0F, L"Poise Debuffs"/);
    assert.match(main, /L"fHemorrhagingRegenRateMultiplier"[^\n]*0\.0F, 1\.0F, L"Poise Debuffs"/);
    assert.match(main, /L"fScorchedMeleeDamageMultiplier"[^\n]*0\.5F, 3\.0F, L"Poise Debuffs"/);
    assert.match(main, /readValue\(L"sHemorrhagingEffectKeywords", L"Poise Debuffs"\)/);
    assert.match(main, /enfeebledRegenCeilingMin > poiseSettings\.enfeebledRegenCeilingMax[\s\S]*?std::swap/);
    assert.match(main, /readBool\(L"bShowPoiseDebuffIcons", "bShowPoiseDebuffIcons", settings\.showPoiseDebuffIcons\)/);
    assert.match(main, /bool showPoiseDebuffIcons = true;/);
    const state = read('src/builtin_poise_state.h');
    for (const field of ['enablePoiseDebuffs = true', 'enableSappedDebuff = true', 'enableEnfeebledDebuff = true',
        'enableNumbedDebuff = true', 'enableHemorrhagingDebuff = true', 'enableScorchedDebuff = true',
        'enfeebledRegenCeilingMin = 0.70F', 'enfeebledRegenCeilingMax = 0.85F', 'enfeebledMagnitudeForMin = 10.0F',
        'numbedRegenDelayBonus = 1.0F', 'hemorrhagingRegenRateMultiplier = 0.65F',
        'scorchedMeleeDamageMultiplier = 1.20F', 'minGlobalRegenCeiling = 0.60F']) {
        assert.ok(state.includes(field), field);
    }
    assert.match(state, /std::string hemorrhagingKeywords = "bleed,hemorrhag";/);
    assert.match(state, /float basePoise = 40\.0F;/);
});

test('the menu has a Poise Status Debuffs section led by the master switch, plus reset and HUD wiring', () => {
    const menu = read('src/menu_framework.cpp'), main = read('src/main.cpp');
    const section = menu.slice(menu.indexOf('CollapsingHeader("Poise Status Debuffs")'), menu.indexOf('// ===== SLIDER PERSISTENCE'));
    assert.ok(menu.indexOf('CollapsingHeader("Combat Balance (Poise Tuning)")') < menu.indexOf('CollapsingHeader("Poise Status Debuffs")'));
    assert.match(section, /^CollapsingHeader\("Poise Status Debuffs"\)\) \{\s*auto\* enablePoiseDebuffs[\s\S]*?Checkbox\("Enable Poise Status Debuffs##debuffs"/);
    for (const label of ['Sapped', 'Enfeebled', 'Numbed', 'Hemorrhaging', 'Scorched']) {
        assert.match(section, new RegExp(`Checkbox\\("${label} \\(`));
    }
    const keys = ['bEnablePoiseDebuffs', 'bEnableSappedDebuff', 'bEnableEnfeebledDebuff', 'bEnableNumbedDebuff',
        'bEnableHemorrhagingDebuff', 'bEnableScorchedDebuff', 'fEnfeebledRegenCeilingMin', 'fEnfeebledRegenCeilingMax',
        'fEnfeebledMagnitudeForMin', 'fMinGlobalRegenCeiling', 'fNumbedRegenDelayBonus',
        'fHemorrhagingRegenRateMultiplier', 'fScorchedMeleeDamageMultiplier'];
    const reset = menu.slice(menu.indexOf('void ResetPoiseDefaults()'));
    for (const key of keys) {
        assert.match(section, new RegExp(`QueueIniWrite\\("${key}", [^;]*"Poise Debuffs"\\)`));
        assert.match(reset, new RegExp(`QueueIniWrite\\("${key}", "[^"]+", "Poise Debuffs"\\)`));
    }
    assert.match(reset, /QueueIniWrite\("sHemorrhagingEffectKeywords", "bleed,hemorrhag", "Poise Debuffs"\)/);
    assert.match(menu, /Checkbox\("Show Poise Debuff Icons##widgets", showPoiseDebuffIcons\)/);
    assert.match(menu, /QueueIniWrite\("bShowPoiseDebuffIcons", val, "HUD"\)/);
    assert.match(menu, /QueueIniWrite\("bShowPoiseDebuffIcons", "true", "HUD"\)/);
    for (const getter of ['get_enablePoiseDebuffs', 'get_enableSappedDebuff', 'get_showPoiseDebuffIcons',
        'get_enfeebledMagnitudeForMin', 'get_minGlobalRegenCeiling']) {
        assert.match(menu, new RegExp(`${getter}\\(\\);`));
        assert.match(main, new RegExp(`${getter}\\(\\) \\{ return &`));
    }
});

test('HUD icons are driven from the poll independently of telemetry, and reapplied on reload and menu changes', () => {
    const main = read('src/main.cpp');
    const poll = main.slice(main.indexOf('void PollHud()'), main.indexOf('VOID CALLBACK TimerCallback'));
    assert.match(poll, /SendPoise\(\);\s*SendPoiseTelemetry\(\);\s*SendPoiseDebuffIcons\(\);/);
    const send = main.slice(main.indexOf('void SendPoiseDebuffIcons()'), main.indexOf('void ConfigurePoiseDebuffIconsView()'));
    assert.match(send, /if \(!settings\.showPoiseDebuffIcons && !settings\.enablePoiseShield\) \{\s*return;/);
    assert.doesNotMatch(send, /showPoiseTelemetry|TelemetryJson|ReadTelemetry/);
    assert.match(send, /BuiltinPoise::ReadPlayerDebuffs\(view\)/);
    assert.match(send, /updatePoiseDebuffs\(\[\], 1\)/);
    assert.match(send, /script == lastDebuffIconScript/);
    const configure = main.slice(main.indexOf('void ConfigurePoiseDebuffIconsView()'), main.indexOf('// showpoisedev'));
    assert.match(configure, /lastDebuffIconScript\.clear\(\);/);
    assert.match(configure, /configurePoiseDebuffIcons\(\{\}\)/);
    const domReady = main.slice(main.indexOf('void OnDomReady('), main.indexOf('void CreateHudView()'));
    assert.match(domReady, /ConfigurePoiseTelemetryView\(\);\s*ConfigurePoiseDebuffIconsView\(\);/);
    const apply = main.slice(main.indexOf('void SendSettingsToHudImpl('), main.indexOf('// Export settings for menu access'));
    assert.match(apply, /changedKey == "bShowPoiseDebuffIcons" \|\| changedKey == "resetHud"\) \{\s*ConfigurePoiseDebuffIconsView\(\);/);
    const poise = read('src/builtin_poise.cpp');
    const read_ = poise.slice(poise.indexOf('bool BuiltinPoise::ReadPlayerDebuffs('), poise.indexOf('bool BuiltinPoise::ReadTelemetry('));
    assert.match(read_, /!configuration\.enablePoiseDebuffs/);
});

test('the effect scan is gated by one atomic, mirrors ActiveFlesh, and runs where flesh does plus on every hit', () => {
    const poise = read('src/builtin_poise.cpp');
    assert.match(poise, /std::atomic<bool> gateDebuffs\{ false \};/);
    assert.match(poise, /gateDebuffs\.store\(configuration\.enablePoiseDebuffs, std::memory_order_relaxed\);/);
    const scan = poise.slice(poise.indexOf('BuiltinPoise::PoiseDebuffs ScanDebuffs('), poise.indexOf('void BuildRaceTable()') > poise.indexOf('BuiltinPoise::PoiseDebuffs ScanDebuffs(') ?
        poise.indexOf('void BuildRaceTable()') : undefined);
    assert.ok(scan.indexOf('!gateDebuffs.load(std::memory_order_relaxed)') > 0 &&
        scan.indexOf('!gateDebuffs.load(std::memory_order_relaxed)') < scan.indexOf('GetActiveEffectList()'));
    assert.match(scan, /kInactive, RE::ActiveEffect::Flag::kDispelled/);
    assert.match(scan, /conditionStatus == RE::ActiveEffect::ConditionStatus::kFalse/);
    assert.match(scan, /BuiltinPoise::EffectIsHostile\(effect->IsHostile\(\), spellHostile, effect->IsDetrimental\(\),\s*bleedCandidate\)/);
    assert.match(scan, /const bool spellHostile = active->spell && active->spell->IsHostile\(\);/);
    assert.match(scan, /BuiltinPoise::IsVanillaBleedEffect\(facts\.plugin, facts\.localFormID\)/);
    assert.doesNotMatch(scan, /caster &&[^;]*nullptr|!caster\)/);
    assert.match(scan, /effect->GetFile\(0\)/);
    assert.match(scan, /facts\.plugin = file->GetFilename\(\);\s*facts\.localFormID = effect->GetLocalFormID\(\);/);
    assert.match(read('src/builtin_poise_state.h'), /kVanillaBleedEffectFormID = 0x000C367A;/);
    assert.match(read('src/builtin_poise_state.h'), /return localFormID == kVanillaBleedEffectFormID;/);
    assert.match(scan, /facts\.selfCast = caster && caster\.get\(\) == actor;/);
    assert.match(scan, /facts\.detrimental = effect->IsDetrimental\(\);/);
    const accumulate = read('src/builtin_poise_state.h');
    assert.match(accumulate, /if \(facts\.selfCast\) \{[^}]*if \(bleed && facts\.detrimental\) \{\s*mark\(DebuffId::hemorrhaging\);\s*\}\s*return;\s*\}/);
    assert.match(scan, /EffectArchetype::kAbsorb/);
    assert.match(scan, /ActorValue::kPoisonResist/);
    assert.match(scan, /ActorValue::kResistFire/);
    assert.match(scan, /ActorValue::kResistFrost/);
    assert.match(scan, /HasKeywordString\("MagicAlchHarmful"\)/);
    assert.match(scan, /facts\.duration = active->duration;/);
    assert.match(scan, /BuiltinPoise::MatchesKeywordList\(bleedKeywords, text\)/);
    assert.match(poise, /refresh \? ScanDebuffs\(actor\)/);
    assert.match(poise, /valid \? ScanDebuffs\(actor\.get\(\)\)/);
    assert.match(poise, /refresh \? ScanDebuffs\(player\)/);
    assert.match(poise, /const auto debuffs = ScanDebuffs\(target\);/);
    assert.match(poise, /const auto debuffs = ScanDebuffs\(target\);/);
    assert.equal((poise.match(/SetDebuffs\(debuffs\)/g) || []).length >= 8, true);
    assert.match(poise, /configuration\.enablePoiseDebuffs != settings\.enablePoiseDebuffs/);
    assert.match(poise, /bleedKeywords = settings\.hemorrhagingKeywords;/);
});

test('lingering effects and absorbs never reach the direct poise damage routes', () => {
    const magic = read('src/magic_gameplay.cpp');
    const elemental = magic.slice(magic.indexOf('bool ElementalHealthEffect('), magic.indexOf('// Returns true when this spell already hit'));
    assert.match(elemental, /kResistFire \|\| resist == RE::ActorValue::kResistFrost \|\|\s*resist == RE::ActorValue::kResistShock/);
    assert.doesNotMatch(magic, /kPoisonResist|MagicAlchHarmful|ScanDebuffs/);
    // The absorb hook only notes the hit (Sapped pulse and target selection); it never produces poise damage.
    const addTarget = magic.slice(magic.indexOf('bool AddTarget('), magic.indexOf('void SpellCollision('));
    assert.match(addTarget, /HasArchetype\(RE::EffectArchetype::kAbsorb\)/);
    assert.match(addTarget, /QueueAbsorb\(absorbTarget, absorbCaster, absorbGeneration\);/);
    const queueAbsorb = magic.slice(magic.indexOf('void QueueAbsorb('), magic.indexOf('Impact MakeProjectilelessImpact('));
    assert.match(queueAbsorb, /BuiltinPoise::NoteMagicTarget\(target\.get\(\), caster\.get\(\), true\);/);
    assert.doesNotMatch(queueAbsorb, /ApplyMagicDamage|Process\(|Queue\(/);
    const state = read('src/builtin_poise_state.h');
    const debuffMath = state.slice(state.indexOf('inline void AccumulateDebuff('), state.indexOf('struct DebuffEffects'));
    assert.doesNotMatch(debuffMath, /ApplyDamage|current -=/);
    // Only recovery, delay and incoming physical hits read the debuffs; the maximum formula does not.
    const maximum = state.slice(state.indexOf('inline MaximumBreakdown ComputeMaximumPoise('), state.indexOf('[[nodiscard]] constexpr bool TierEnabled'));
    assert.doesNotMatch(maximum, /debuff|Debuff/);
    assert.match(state, /const float rate = fx\.sapped \? 0\.0F : settings\.regenRatePerSecond \* regenMultiplier \* fx\.regenRateMult;/);
    assert.match(state, /const auto fx = ResolveDebuffEffects\(settings, debuffs\);\s*regenDelay = settings\.regenDelaySeconds \+ fx\.delayBonus;/);
    assert.match(state, /if \(hit\.type != HitType::projectile\) \{\s*const float scorched = ResolveDebuffEffects/);
});

test('the target card renders its own debuff pills and the target data comes from the live NPC state', () => {
    const html = read('view/index.html');
    assert.match(html, /<div id="pt-target-debuffs" class="pt-pills"><\/div>/);
    const js = read('view/minimal.js');
    assert.match(js, /renderTelemetryPills\(document\.getElementById\(`pt-\$\{prefix\}-debuffs`\), actor\.debuffs\);/);
    assert.match(js, /if \(data\.target\) renderTelemetryActor\('target', data\.target, weapon\);/);
    const poise = read('src/builtin_poise.cpp');
    const read_ = poise.slice(poise.indexOf('bool BuiltinPoise::ReadTelemetry('), poise.indexOf('bool BuiltinPoise::ReadTarget('));
    assert.match(read_, /lastTarget\.view = DescribePoise\(it->second\.poise, lastTarget\.name, TierLabel\(lastTarget\.tier\)\);/);
    assert.match(poise, /result\.debuffs = BuiltinPoise::BuildDebuffList\(configuration, poise\.debuffs\);/);
});

test('NPC and creature debuffs are scanned outside the magic build on refresh and on every hit', () => {
    const poise = read('src/builtin_poise.cpp');
    const scanDef = poise.indexOf('BuiltinPoise::PoiseDebuffs ScanDebuffs(');
    assert.ok(scanDef > 0);
    // The scan sits in the always-compiled code: no #ifdef ARPGHUD_MAGIC_GAMEPLAY block is open where it is defined.
    const stack = [];
    for (const line of poise.slice(0, scanDef).split('\n')) {
        const t = line.trim();
        if (/^#if/.test(t)) stack.push(t);
        else if (/^#else/.test(t)) stack[stack.length - 1] = `else ${stack[stack.length - 1]}`;
        else if (/^#endif/.test(t)) stack.pop();
    }
    assert.ok(!stack.some(entry => entry === '#ifdef ARPGHUD_MAGIC_GAMEPLAY'), stack.join(' | '));
    // NPC update tick, creature refresh from the player update, physical hit, magic hit and new magic target.
    assert.match(poise, /refresh \? ScanDebuffs\(actor\)/);
    assert.match(poise, /valid \? ScanDebuffs\(actor\.get\(\)\)/);
    assert.match(poise, /const auto debuffs = ScanDebuffs\(target\);\s*const bool recordHit/);
    assert.match(poise, /entry\.poise\.SetDebuffs\(debuffs\);\s*entry\.poise\.SetPlayerTarget\(false\);\s*entry\.lastUpdate = gameplaySeconds;\s*CatchUp\(entry\);\s*result = entry\.poise\.ApplyDamage/);
});

test('channeled spells still select, track and scan the target, and telemetry rescans it live', () => {
    const magic = read('src/magic_gameplay.cpp');
    const note = magic.indexOf('BuiltinPoise::NoteMagicTarget(target.get(), attacker.get());');
    assert.ok(note > 0);
    assert.ok(note < magic.indexOf('settings.concentrationMaxPoisePerInterval <= 0.0F'));
    assert.ok(note < magic.indexOf('MagicPoise::Damage(input)'));
    assert.match(magic.slice(note - 200, note), /impact\.channeled \|\| impact\.concentration\) && attacker->IsPlayerRef\(\) && !target->IsPlayerRef\(\)/);
    const poise = read('src/builtin_poise.cpp');
    const noteDef = poise.slice(poise.indexOf('void BuiltinPoise::NoteMagicTarget('), poise.indexOf('bool BuiltinPoise::IsInstalled()'));
    assert.match(noteDef, /ScanDebuffs\(target\)/);
    assert.match(poise, /entry\.poise\.SetDebuffs\(debuffs\);\s*if \(absorb\) \{\s*entry\.poise\.NoteSapped\("Absorb"\);/);
    assert.match(poise, /state\.NoteSapped\("Absorb"\);/);
    assert.match(noteDef, /NoteTarget\(target, id, handle, tier, &entry\.poise\);/);
    const refresh = poise.slice(poise.indexOf('void BuiltinPoise::RefreshTelemetryTarget()'), poise.indexOf('bool BuiltinPoise::ReadTelemetry('));
    assert.match(refresh, /ScanDebuffs\(actor\.get\(\)\);/);
    assert.match(refresh, /it->second\.poise\.SetDebuffs\(debuffs\);/);
    const main = read('src/main.cpp');
    assert.match(main, /BuiltinPoise::RefreshTelemetryTarget\(\);\s*const auto script = BuiltinPoise::ReadTelemetry\(snapshot\)/);
});
