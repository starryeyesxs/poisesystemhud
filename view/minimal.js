const minimalState = {
    active: false, weatherEnabled: false, gemEnabled: false, combat: false, moving: false, debuffsVisible: false,
    stats: Object.fromEntries(['health', 'magicka', 'stamina', 'poise'].map(key =>
        [key, { current: 0, max: 0, trail: 0, hitAt: 0, initialized: false, enabled: key !== 'poise' }]))
};

const MIN_BAR_BASE_WIDTH = 200;
const MIN_BAR_MAX_WIDTH = 480;
const MIN_BAR_PX_PER_POINT = 0.5;
const visibility = { alwaysOn: false, dynamicHud: true, idleOpacity: 0, fadeDelaySeconds: 3, idle: false, timer: null };

function refreshVisibility(force = false) {
    if (!minimalState.active) return;
    const stats = Object.values(minimalState.stats);
    const idle = stats.slice(0, 3).every(stat => stat.initialized) &&
        !minimalState.combat && !minimalState.moving && !minimalState.debuffsVisible &&
        stats.every(stat => !stat.enabled || stat.current >= 100);
    if (!force && visibility.idle === idle) return;
    visibility.idle = idle;
    clearTimeout(visibility.timer);
    visibility.timer = null;
    const layout = document.getElementById('minimal-layout');
    layout.classList.toggle('hud-idle', false);
    if (idle && visibility.dynamicHud && !visibility.alwaysOn) {
        visibility.timer = setTimeout(() => layout.classList.toggle('hud-idle', true),
            visibility.fadeDelaySeconds * 1000);
    }
    for (const key of Object.keys(minimalState.stats)) renderMinimalStat(key);
}

function configureVisibility(alwaysOn, dynamicHud, idleOpacity, fadeDelaySeconds) {
    if (typeof alwaysOn !== 'boolean' || typeof dynamicHud !== 'boolean' ||
        !Number.isFinite(idleOpacity) || idleOpacity < 0 || idleOpacity > 1 ||
        !Number.isFinite(fadeDelaySeconds) || fadeDelaySeconds < 0 || fadeDelaySeconds > 600) {
        console.error('Invalid visibility settings', alwaysOn, dynamicHud, idleOpacity, fadeDelaySeconds);
        return;
    }
    Object.assign(visibility, { alwaysOn, dynamicHud, idleOpacity, fadeDelaySeconds });
    document.getElementById('minimal-layout').style.setProperty('--idle-opacity', idleOpacity);
    refreshVisibility(true);
}

function updateHudPosition(left, bottom) {
    if (!Number.isInteger(left) || left < -500 || left > 2000 ||
        !Number.isInteger(bottom) || bottom < -200 || bottom > 2000) {
        console.error('Invalid HUD position', left, bottom);
        return;
    }
    document.documentElement.style.setProperty('--side-offset-left', `${left}px`);
    document.documentElement.style.setProperty('--bottom-offset', `${bottom}px`);
}

function minimalBarWidth(max) {
    return Math.min(MIN_BAR_MAX_WIDTH, MIN_BAR_BASE_WIDTH + Math.max(0, max - 100) * MIN_BAR_PX_PER_POINT);
}

function configureLayouts(weatherEnabled, gemEnabled) {
    if (typeof weatherEnabled !== 'boolean' || typeof gemEnabled !== 'boolean') {
        console.error('Invalid layout settings', weatherEnabled, gemEnabled);
        return;
    }
    minimalState.active = true;
    minimalState.weatherEnabled = weatherEnabled;
    minimalState.gemEnabled = gemEnabled;
    document.getElementById('minimal-layout').hidden = false;
    document.getElementById('world-widget').hidden = !weatherEnabled;
    document.getElementById('min-emblem').hidden = !(minimalState.active && gemEnabled);
    if (!(minimalState.active && gemEnabled)) document.getElementById('light-gem').hidden = true;
    if (minimalState.active) buildMinimalBars();
}

function updateMinimalGraphics(filigree, shine, shadow, notches) {
    if (typeof filigree !== 'boolean' || typeof notches !== 'boolean' ||
        !Number.isFinite(shine) || shine < 0 || shine > 2 || !Number.isFinite(shadow) || shadow < 0 || shadow > 2) {
        console.error('Invalid minimalist graphics', filigree, shine, shadow, notches);
        return;
    }
    const layout = document.getElementById('minimal-layout');
    const alpha = (base, multiplier) => String(Math.min(1, base * multiplier));
    layout.classList.toggle('no-filigree', !filigree);
    layout.classList.toggle('show-notches', notches);
    layout.style.setProperty('--min-shine-alpha', alpha(0.35, shine));
    layout.style.setProperty('--min-edge-alpha', alpha(0.55, shine));
    layout.style.setProperty('--min-drop-alpha', alpha(0.8, shadow));
    layout.style.setProperty('--min-drop-blur', `${2 * shadow}px`);
    layout.style.setProperty('--min-inner-alpha', alpha(0.5, shadow));
    layout.style.setProperty('--min-inner-blur', `${2 * shadow}px`);
}

function buildMinimalBars() {
    document.querySelectorAll('#minimal-bars .min-row').forEach(row => {
        const key = row.dataset.stat;
        if (row.firstElementChild) return;
        const cap = key === 'poise' ? 'min-cap-thin' : 'min-cap';
        row.innerHTML = `<svg class="min-cap"><use href="#${cap}-left"></use></svg>` +
            `<div class="min-track"><div class="min-layer min-trail min-${key}"></div>` +
            `<div class="min-layer min-fill min-${key}"></div></div>` +
            `<svg class="min-cap"><use href="#${cap}-right"></use></svg>` +
            (key === 'poise' ? '<span id="min-poise-numbers" hidden></span>' : '');
    });
}

function resetMinimal() {
    if (!minimalState.active) return;
    minimalState.combat = false;
    minimalState.moving = false;
    for (const [key, stat] of Object.entries(minimalState.stats)) {
        Object.assign(stat, { current: 0, max: 0, trail: 0, hitAt: 0, initialized: false, enabled: false });
        renderMinimalStat(key);
    }
    document.getElementById('light-gem').hidden = true;
    document.getElementById('min-poise-numbers').hidden = true;
    refreshVisibility(true);
}

function minimalStat(key, current, max, enabled = true) {
    const stat = minimalState.stats[key];
    if (!stat || !Number.isFinite(current) || !Number.isFinite(max) || typeof enabled !== 'boolean') {
        console.error('Invalid minimalist stat', key, current, max, enabled);
        return;
    }
    const percent = max > 0 ? Math.max(0, Math.min(100, 100 * current / max)) : 0;
    if (!stat.initialized || stat.max !== max || !stat.enabled) {
        stat.trail = percent;
    } else if (percent < stat.current) {
        stat.hitAt = performance.now();
    } else {
        stat.trail = Math.max(stat.trail, percent);
    }
    stat.current = percent;
    stat.max = max;
    stat.enabled = enabled && max > 0;
    stat.initialized = true;
    renderMinimalStat(key);
}

function renderMinimalStat(key) {
    if (!minimalState.active) return;
    const stat = minimalState.stats[key];
    const row = document.getElementById(`min-${key}`);
    const show = stat.enabled && (visibility.alwaysOn || !visibility.dynamicHud || visibility.idle || minimalState.combat ||
        ((key === 'stamina' || key === 'poise') && minimalState.moving) || stat.current < 100 ||
        (key === 'poise' && minimalState.debuffsVisible));
    row.classList.toggle('hidden', !show);

    // Poise keeps its fixed CSS width so armor weight and Dragonhide change hit size, not bar length.
    if (key !== 'poise' && stat.max > 0) row.style.width = `${minimalBarWidth(stat.max)}px`;

    row.querySelector('.min-fill').style.transform = `scaleX(${stat.current / 100})`;
    row.querySelector('.min-trail').style.transform = `scaleX(${stat.trail / 100})`;
}

function updateMinimalStats(h, hm, m, mm, s, sm, combat, moving) {
    if (!minimalState.active) return;
    if (typeof combat !== 'boolean' || typeof moving !== 'boolean') {
        console.error('Invalid minimalist visibility state'); return;
    }
    minimalState.combat = combat;
    minimalState.moving = moving;
    minimalStat('health', h, hm); minimalStat('magicka', m, mm); minimalStat('stamina', s, sm);
    renderMinimalStat('poise');
    refreshVisibility();
}

function updateMinimalPoise(enabled, current, max, showNumbers = false) {
    if (minimalState.active) {
        if (typeof enabled !== 'boolean' || typeof showNumbers !== 'boolean' ||
            !Number.isFinite(current) || !Number.isFinite(max)) {
            console.error('Invalid minimalist poise', enabled, current, max, showNumbers);
            return;
        }
        minimalStat('poise', current, max, enabled);
        const label = document.getElementById('min-poise-numbers');
        label.hidden = !showNumbers || !enabled || max <= 0;
        if (!label.hidden) label.textContent = `${current.toFixed(1)} / ${max.toFixed(1)}`;
        refreshVisibility();
    }
}

function updateTargetPoise(visible, name, tier, current, max, immune) {
    const root = document.getElementById('target-poise');
    if (typeof visible !== 'boolean' || typeof name !== 'string' || typeof tier !== 'string' ||
        !Number.isFinite(current) || !Number.isFinite(max) || typeof immune !== 'boolean') {
        console.error('Invalid target poise', visible, name, tier, current, max, immune);
        return;
    }
    if (!visible || max <= 0) {
        root.classList.remove('show');
        root.setAttribute('aria-hidden', 'true');
        return;
    }
    document.getElementById('target-poise-name').textContent = tier ? `${name} [${tier}]` : name;
    document.getElementById('target-poise-values').textContent = `${current.toFixed(1)} / ${max.toFixed(1)}`;
    document.getElementById('target-poise-fill').style.transform = `scaleX(${Math.min(1, Math.max(0, current / max))})`;
    root.classList.toggle('immune', immune);
    root.classList.add('show');
    root.setAttribute('aria-hidden', 'false');
}

setInterval(() => {
    if (!minimalState.active) return;
    const now = performance.now();
    for (const [key, stat] of Object.entries(minimalState.stats)) {
        if (stat.enabled && stat.trail > stat.current && now - stat.hitAt > 600) {
            const gap = stat.trail - stat.current;
            stat.trail = gap < 0.4 ? stat.current : Math.max(stat.current, stat.trail - Math.max(0.4, gap * 0.1));
            renderMinimalStat(key);
        }
    }
}, 33);

function updateWorldWidget(location, hour, minute, day, month, year, weather, isInterior) {
    if (!minimalState.weatherEnabled) return;
    const months = ['Morning Star', "Sun's Dawn", 'First Seed', "Rain's Hand", 'Second Seed', 'Mid Year',
        "Sun's Height", 'Last Seed', 'Heart Fire', 'Frostfall', "Sun's Dusk", 'Evening Star'];
    const icons = { clear: 'sun', cloudy: 'cloud', rain: 'rain', snow: 'snow', storm: 'storm', unknown: 'unknown' };
    if (typeof location !== 'string' || !location || !Number.isInteger(hour) || hour < 0 || hour > 23 ||
        !Number.isInteger(minute) || minute < 0 || minute > 59 || !Number.isInteger(day) || day < 1 || day > 31 ||
        !Number.isInteger(month) || month < 1 || month > 12 || !Number.isInteger(year) || year < 1 ||
        !Object.prototype.hasOwnProperty.call(icons, weather) || typeof isInterior !== 'boolean') {
        console.error('Invalid world telemetry'); return;
    }
    const icon = isInterior ? 'indoors' : weather === 'clear' && (hour < 6 || hour >= 20) ? 'moon' : icons[weather];
    document.getElementById('world-location').textContent = location;
    document.getElementById('world-time').textContent = `${String(hour).padStart(2, '0')}:${String(minute).padStart(2, '0')}`;
    document.getElementById('world-date').textContent = `${day} ${months[month - 1]}, 4E ${year}`;
    document.getElementById('world-weather').textContent = isInterior ? 'Indoors' : weather === 'unknown' ? 'Unknown' : weather;
    document.getElementById('world-icon').setAttribute('href', `#weather-${icon}`);
    document.getElementById('world-widget').classList.toggle('is-interior', isInterior);
}

function updateLightGem(isSneaking, lightLevel) {
    if (!minimalState.active || !minimalState.gemEnabled) return;
    if (typeof isSneaking !== 'boolean' || (lightLevel !== null && (!Number.isFinite(lightLevel) || lightLevel < 0 || lightLevel > 100))) {
        console.error('Invalid light telemetry', isSneaking, lightLevel); return;
    }
    const gem = document.getElementById('light-gem');
    gem.hidden = !isSneaking || lightLevel === null;
    if (gem.hidden) return;
    const stops = [{ level: 0, color: [52,54,55] }, { level: 25, color: [72,65,57] },
        { level: 60, color: [216,155,64] }, { level: 100, color: [255,244,197] }];
    const index = Math.max(1, stops.findIndex(stop => lightLevel <= stop.level));
    const lower = stops[index - 1], upper = stops[index];
    const fraction = (lightLevel - lower.level) / (upper.level - lower.level);
    const color = lower.color.map((v, i) => Math.round(v + (upper.color[i] - v) * fraction));
    const height = 34 * lightLevel / 100;
    gem.setAttribute('aria-label', `Light level: ${lightLevel} out of 100`);
    gem.style.setProperty('--gem-color', `rgb(${color.join(', ')})`);
    gem.style.setProperty('--gem-glow', String(Math.max(0, (lightLevel - 60) / 40) * .65));
    document.getElementById('light-gem-fill').setAttribute('y', String(41 - height));
    document.getElementById('light-gem-fill').setAttribute('height', String(height));
}

const telemetryState = { enabled: false };

const POISE_DEBUFF_IDS = ['sapped', 'enfeebled', 'numbed', 'hemorrhaging', 'scorched'];
const debuffIconState = { enabled: false, ids: [], ceilingRatio: 1 };

// Shows the active debuff icons, the Enfeebled ceiling tick and keeps the poise row visible while any debuff is active.
// Uses only getElementById, hidden, classList and style so it works with the layout test's stub DOM.
function renderPoiseDebuffIcons() {
    const active = debuffIconState.enabled ? debuffIconState.ids : [];
    document.getElementById('poise-debuff-icons').hidden = active.length === 0;
    for (const id of POISE_DEBUFF_IDS) {
        document.getElementById(`pd-icon-${id}`).classList.toggle('pd-off', !active.includes(id));
    }
    const ceiling = debuffIconState.ceilingRatio;
    const row = document.getElementById('min-poise');
    for (const id of POISE_DEBUFF_IDS) row.classList.toggle(`debuff-${id}`, active.includes(id));
    row.classList.toggle('has-ceiling', active.includes('enfeebled') && ceiling < 1);
    row.style.setProperty('--poise-ceiling', String(ceiling));
    const visible = active.length > 0;
    if (minimalState.debuffsVisible !== visible) {
        minimalState.debuffsVisible = visible;
        if (minimalState.active) {
            renderMinimalStat('poise');
            refreshVisibility();
        }
    }
}

function configurePoiseDebuffIcons(enabled) {
    if (typeof enabled !== 'boolean') {
        console.error('Invalid poise debuff icon setting', enabled);
        return;
    }
    debuffIconState.enabled = enabled;
    renderPoiseDebuffIcons();
}

function updatePoiseDebuffs(ids, ceilingRatio) {
    if (!Array.isArray(ids) || !ids.every(id => POISE_DEBUFF_IDS.includes(id)) ||
        !Number.isFinite(ceilingRatio) || ceilingRatio < 0 || ceilingRatio > 1) {
        console.error('Invalid poise debuffs', ids, ceilingRatio);
        return;
    }
    debuffIconState.ids = ids;
    debuffIconState.ceilingRatio = ceilingRatio;
    renderPoiseDebuffIcons();
}

// Reactive resource bar effects: each active hostile effect toggles one state class on the Health, Magicka or
// Stamina row. Only getElementById and classList.toggle are used, so no elements or timers are added.
const REACTIVE_BAR_EFFECTS = {
    'health-poison': ['min-health'],
    'health-fire': ['min-health'],
    'health-bleed': ['min-health'],
    'health-sapped': ['min-health'],
    'bar-shock': ['min-health', 'min-magicka'],
    'stamina-frost': ['min-stamina']
};
const reactiveBarState = { enabled: false, ids: [] };

function renderReactiveBars() {
    const active = reactiveBarState.enabled ? reactiveBarState.ids : [];
    for (const [effect, rows] of Object.entries(REACTIVE_BAR_EFFECTS)) {
        for (const row of rows) document.getElementById(row).classList.toggle(effect, active.includes(effect));
    }
}

function configureReactiveBars(enabled) {
    if (typeof enabled !== 'boolean') {
        console.error('Invalid reactive bar setting', enabled);
        return;
    }
    reactiveBarState.enabled = enabled;
    renderReactiveBars();
}

function updateReactiveBars(ids) {
    if (!Array.isArray(ids) || !ids.every(id => Object.prototype.hasOwnProperty.call(REACTIVE_BAR_EFFECTS, id))) {
        console.error('Invalid reactive bar effects', ids);
        return;
    }
    reactiveBarState.ids = ids;
    renderReactiveBars();
}

function telemetryNumber(value) {
    return Number.isFinite(value) ? value : 0;
}

function telemetryFixed(value, digits = 1) {
    return telemetryNumber(value).toFixed(digits);
}

function formatTelemetryHit(hit) {
    const parts = [`Base ${telemetryFixed(hit.base)}`];
    if (telemetryNumber(hit.raw) > 0) parts.push(`+ RawDmg ${telemetryFixed(hit.raw)}${hit.rawCapped ? ' [CAP]' : ''}`);
    for (const [label, factor] of hit.mults) parts.push(`* ${label} ${telemetryFixed(factor, 2)}`);
    const damage = hit.immune ? '0.0 (IMMUNE)' : telemetryFixed(hit.final);
    return `[${hit.dir}] ${hit.attacker} -> ${hit.target} (${hit.type}): ${damage} [${parts.join(' ')}]`;
}

function validTelemetryActor(actor) {
    return actor && typeof actor.name === 'string' && typeof actor.tier === 'string' && Array.isArray(actor.debuffs) &&
        typeof actor.stale === 'boolean' &&
        ['current', 'max', 'ceilingRatio', 'ceilingPoise', 'regen', 'delay', 'delayMax', 'immune', 'flesh', 'lightArmor']
            .every(key => Number.isFinite(actor[key]));
}

function validTelemetryHit(hit) {
    return hit && (hit.dir === 'IN' || hit.dir === 'OUT') && typeof hit.attacker === 'string' &&
        typeof hit.target === 'string' && typeof hit.type === 'string' && Array.isArray(hit.mults) &&
        typeof hit.broke === 'boolean' && typeof hit.immune === 'boolean' &&
        hit.mults.every(tag => Array.isArray(tag) && typeof tag[0] === 'string' && Number.isFinite(tag[1]));
}

function renderTelemetryPills(container, debuffs) {
    container.textContent = '';
    const list = debuffs.length ? debuffs : [{ id: 'none', label: 'None', detail: '', source: '' }];
    for (const debuff of list) {
        const pill = document.createElement('span');
        pill.className = `pt-pill pt-${debuff.id}`;
        pill.textContent = debuff.detail ? `${debuff.label} ${debuff.detail}` : debuff.label;
        if (debuff.source) pill.title = debuff.source;
        container.appendChild(pill);
    }
}

function renderTelemetryActor(prefix, actor, weapon) {
    let title = actor.tier && actor.tier !== actor.name ? `${actor.name} [${actor.tier}]` : actor.name;
    if (actor.stale) title += ' (last known)';
    document.getElementById(`pt-${prefix}-name`).textContent = title;
    const extras = [];
    if (actor.flesh > 0) extras.push(`Flesh +${telemetryFixed(actor.flesh)}`);
    if (actor.lightArmor > 0) extras.push(`Light Armor +${telemetryFixed(actor.lightArmor)}`);
    let poise = `Poise: ${telemetryFixed(actor.current)} / ${telemetryFixed(actor.max)}`;
    if (extras.length) poise += ` (${extras.join(', ')})`;
    if (prefix === 'target' && weapon.armed && weapon.light > 0) poise += ` (~${telemetryFixed(actor.current / weapon.light)} light hits)`;
    document.getElementById(`pt-${prefix}-poise`).textContent = poise;
    document.getElementById(`pt-${prefix}-regen`).textContent =
        `Regen: +${telemetryFixed(actor.regen)}/s | Cap: ${telemetryFixed(actor.ceilingRatio * 100, 0)}% (${telemetryFixed(actor.ceilingPoise)})`;
    const delay = document.getElementById(`pt-${prefix}-delay`);
    delay.textContent = `Delay: ${telemetryFixed(actor.delay)}s / ${telemetryFixed(actor.delayMax)}s`;
    delay.classList.toggle('active', actor.delay > 0);
    const immune = document.getElementById(`pt-${prefix}-immune`);
    immune.textContent = `Immune: ${telemetryFixed(actor.immune)}s`;
    immune.classList.toggle('active', actor.immune > 0);
    renderTelemetryPills(document.getElementById(`pt-${prefix}-debuffs`), actor.debuffs);
}

function renderTelemetryHit(index, hit) {
    const row = document.getElementById(`pt-hit-${index}`);
    row.textContent = '';
    if (!hit) {
        row.hidden = true;
        return;
    }
    row.hidden = false;
    row.classList.toggle('pt-out', hit.dir === 'OUT');
    row.classList.toggle('pt-in', hit.dir === 'IN');
    row.appendChild(document.createTextNode(formatTelemetryHit(hit)));
    if (hit.broke) {
        const tag = document.createElement('span');
        tag.className = 'pt-break';
        tag.textContent = ' [STAGGER!]';
        row.appendChild(tag);
    }
}

function configureTelemetry(enabled) {
    if (typeof enabled !== 'boolean') {
        console.error('Invalid poise telemetry setting', enabled);
        return;
    }
    telemetryState.enabled = enabled;
    const root = document.getElementById('poise-telemetry');
    root.hidden = !enabled;
    root.setAttribute('aria-hidden', enabled ? 'false' : 'true');
}

function updatePoiseTelemetry(data) {
    if (!telemetryState.enabled) return;
    const unavailable = data === null;
    if (!unavailable && (!data || !validTelemetryActor(data.player) || !data.weapon || !Array.isArray(data.hits) ||
        (data.target !== null && !validTelemetryActor(data.target)) || !data.hits.every(validTelemetryHit) ||
        !['light', 'power', 'rawEstimate'].every(key => Number.isFinite(data.weapon[key])))) {
        console.error('Invalid poise telemetry');
        return;
    }
    document.getElementById('pt-status').hidden = !unavailable;
    document.getElementById('pt-player').hidden = unavailable;
    document.getElementById('pt-hits').hidden = unavailable;
    if (unavailable) {
        document.getElementById('pt-target').hidden = true;
        document.getElementById('pt-weapon').textContent = '';
        return;
    }
    const weapon = data.weapon;
    let benchmark = 'Unarmed';
    if (weapon.armed) {
        const kind = weapon.ranged ? 'Ranged' : weapon.twoHanded ? '2H' : '1H';
        benchmark = `${kind} Light: ${telemetryFixed(weapon.light)} | Power: ${telemetryFixed(weapon.power)}` +
            ` (est. dmg bonus +${telemetryFixed(weapon.rawEstimate)}${weapon.rawCapped ? ' [CAP]' : ''})`;
    }
    document.getElementById('pt-weapon').textContent = benchmark;
    renderTelemetryActor('player', data.player, weapon);
    document.getElementById('pt-target').hidden = !data.target;
    if (data.target) renderTelemetryActor('target', data.target, weapon);
    for (let i = 0; i < 5; ++i) renderTelemetryHit(i, data.hits[i]);
}
