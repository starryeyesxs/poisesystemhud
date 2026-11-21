const minimalState = {
    active: false, customHudEnabled: true, weatherEnabled: false, gemEnabled: false,
    combat: false, moving: false, casting: false, debuffsVisible: false,
    theme: 'daedric', light: { sneaking: false, level: null },
    stats: Object.fromEntries(['health', 'magicka', 'stamina', 'poise'].map(key =>
        [key, { current: 0, value: 0, max: 0, drainDelta: 0, trail: 0, hitAt: 0,
            initialized: false, enabled: key !== 'poise' }]))
};

const HUD_THEMES = ['daedric', 'minimalist'];

const MIN_BAR_BASE_WIDTH = 200;
const MIN_BAR_MAX_WIDTH = 480;
const MIN_BAR_PX_PER_POINT = 0.5;
const POISE_SHIELD_BREAK_MS = 360;
const RESOURCE_IDLE_TOLERANCE = 1.5;
const RESOURCE_IDLE_PERCENT = 98.5;
const visibility = { alwaysOn: false, dynamicHud: true, idleOpacity: 0, fadeDelaySeconds: 3, idle: false, timer: null };
const poiseShieldState = {
    enabled: true, offsetX: 0, offsetY: 0, initialized: false, previousPercent: 1,
    breakUntil: 0, breakTimer: null
};

function statIsFull(key, stat) {
    if (key === 'poise') return stat.current >= 100;
    const deficit = Math.max(0, stat.max - stat.value);
    return stat.drainDelta < RESOURCE_IDLE_TOLERANCE &&
        (deficit <= RESOURCE_IDLE_TOLERANCE || stat.current >= RESOURCE_IDLE_PERCENT);
}

function refreshVisibility(force = false) {
    if (!minimalState.active) return;
    const stats = Object.entries(minimalState.stats);
    const idle = stats.slice(0, 3).every(([, stat]) => stat.initialized) &&
        !minimalState.combat && !minimalState.moving && !minimalState.casting && !minimalState.debuffsVisible &&
        stats.every(([key, stat]) => !stat.enabled || statIsFull(key, stat));
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
    if (!(minimalState.active && gemEnabled)) {
        minimalState.light = { sneaking: false, level: null };
        renderLightGem();
    }
    if (minimalState.active) buildMinimalBars();
}

function configureCustomHud(enabled) {
    if (typeof enabled !== 'boolean') {
        console.error('Invalid custom HUD setting', enabled);
        return;
    }
    minimalState.customHudEnabled = enabled;
    document.getElementById('minimal-layout').classList.toggle('custom-hud-disabled', !enabled);
    if (minimalState.active) refreshVisibility(true);
}

function configureHudTheme(theme) {
    if (!HUD_THEMES.includes(theme)) {
        console.error('Invalid HUD theme', theme);
        return;
    }
    minimalState.theme = theme;
    const layout = document.getElementById('minimal-layout');
    layout.classList.toggle('theme-daedric', theme === 'daedric');
    layout.classList.toggle('theme-minimalist', theme === 'minimalist');
    positionPoiseReadout(theme);
    renderLightGem();
    renderMinimalStat('poise');
    renderPoiseDebuffIcons();
}

function positionPoiseReadout(theme) {
    const readout = document.getElementById('min-poise-numbers');
    const destination = document.getElementById(
        theme === 'daedric' && poiseShieldState.enabled ? 'poise-shield-widget' : 'min-poise');
    if (typeof destination.appendChild === 'function' && readout.parentElement !== destination) {
        destination.appendChild(readout);
    }
}

function configurePoiseShield(enabled, offsetX, offsetY) {
    if (typeof enabled !== 'boolean' || !Number.isInteger(offsetX) || offsetX < -500 || offsetX > 2000 ||
        !Number.isInteger(offsetY) || offsetY < -200 || offsetY > 2000) {
        console.error('Invalid poise shield settings', enabled, offsetX, offsetY);
        return;
    }
    Object.assign(poiseShieldState, { enabled, offsetX, offsetY });
    const layout = document.getElementById('minimal-layout');
    layout.classList.toggle('poise-shield-disabled', !enabled);
    layout.style.setProperty('--poise-shield-x', `${offsetX}px`);
    layout.style.setProperty('--poise-shield-y', `${offsetY}px`);
    positionPoiseReadout(minimalState.theme);
    renderMinimalStat('poise');
    renderPoiseDebuffIcons();
}

// Scales the whole player cluster (bars, debuff icons and crest) uniformly from its bottom-left corner.
function configureHudScale(scale) {
    if (!Number.isFinite(scale) || scale < 0.7 || scale > 1.5) {
        console.error('Invalid HUD scale', scale);
        return;
    }
    document.getElementById('minimal-layout').style.setProperty('--hud-scale', String(scale));
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

// Daedric Iron frame markup, copied from the approved prototype. The 26px spearhead caps keep a fixed size and the
// middle rails use non-scaling strokes, so a wider or narrower bar never stretches the caps, rails or runes.
function daedricCapMarkup(isRight) {
    return [
        `<svg class="dd-cap ${isRight ? 'right' : ''}" viewBox="0 0 26 24" aria-hidden="true">`,
        `<polygon points="1,12 11,7.5 9.5,12 11,16.5" fill="none" stroke="#090a0d" stroke-width="1.4" stroke-linejoin="miter"/>`,
        `<path d="M26,2 L15,2 L9.5,12 L15,22 L26,22 L26,18.5 L17,18.5 L13.5,12 L17,5.5 L26,5.5 Z" fill="none" stroke="#090a0d" stroke-width="1.4" stroke-linejoin="miter"/>`,
        `<polygon points="1,12 11,7.5 9.5,12 11,16.5" fill="url(#spearBlade)" stroke="#111" stroke-width="0.8"/>`,
        `<path d="M26,2 L15,2 L9.5,12 L15,22 L26,22 L26,18.5 L17,18.5 L13.5,12 L17,5.5 L26,5.5 Z" fill="url(#ironGrad)" />`,
        `<polyline points="26,2 15,2 9.5,12 15,22 26,22" fill="none" stroke="url(#silverBevel)" stroke-width="1"/>`,
        `<polyline points="26,5.5 17,5.5 13.5,12 17,18.5 26,18.5" fill="none" stroke="url(#silverBevel)" stroke-width="0.9"/>`,
        `<path d="M1.5,12 L11,7.5 M26,2 L15,2 L9.5,12" fill="none" stroke="#e6ebf5" stroke-width="0.65" opacity="0.9"/>`,
        `<path d="M1.5,12 L11,16.5 M9.5,12 L15,22 L26,22" fill="none" stroke="#9aa1b0" stroke-width="0.55" opacity="0.7"/>`,
        `<path d="M2,12.35 H9.5" stroke="#121317" stroke-width="0.65"/>`,
        `<path d="M2,11.75 H9.5" stroke="#dfe4ef" stroke-width="0.65"/>`,
        `<circle cx="20.5" cy="3.75" r="1.35" fill="#0b0c0f"/>`,
        `<circle cx="20.5" cy="3.75" r="0.95" fill="#838996" stroke="#cfd5e0" stroke-width="0.3"/>`,
        `<circle cx="20.2" cy="3.45" r="0.32" fill="#f0f4fc"/>`,
        `<circle cx="20.5" cy="20.25" r="1.35" fill="#0b0c0f"/>`,
        `<circle cx="20.5" cy="20.25" r="0.95" fill="#838996" stroke="#cfd5e0" stroke-width="0.3"/>`,
        `<circle cx="20.2" cy="19.95" r="0.32" fill="#f0f4fc"/>`,
        `</svg>`
    ].join('');
}

function daedricFrameMarkup() {
    return [
        `<div class="dd-frame" aria-hidden="true">`,
        daedricCapMarkup(false),
        `<div class="dd-mid">`,
        `<svg class="dd-mid-svg" viewBox="0 0 100 24" preserveAspectRatio="none">`,
        `<rect x="0" y="2" width="100" height="3.5" fill="url(#ironGrad)" />`,
        `<rect x="0" y="18.5" width="100" height="3.5" fill="url(#ironGrad)" />`,
        `<line x1="0" y1="2" x2="100" y2="2" stroke="url(#silverBevel)" stroke-width="1" vector-effect="non-scaling-stroke"/>`,
        `<line x1="0" y1="5.5" x2="100" y2="5.5" stroke="url(#silverBevel)" stroke-width="0.9" vector-effect="non-scaling-stroke"/>`,
        `<line x1="0" y1="18.5" x2="100" y2="18.5" stroke="url(#silverBevel)" stroke-width="0.9" vector-effect="non-scaling-stroke"/>`,
        `<line x1="0" y1="22" x2="100" y2="22" stroke="url(#silverBevel)" stroke-width="1" vector-effect="non-scaling-stroke"/>`,
        `</svg>`,
        `<svg class="dd-sigil" viewBox="0 0 96 24">`,
        `<path d="M40,3.8 Q48,0.5 56,3.8 Q48,7.1 40,3.8 Z" fill="#1b1c20" stroke="url(#silverBevel)" stroke-width="0.85"/>`,
        `<ellipse cx="48" cy="3.8" rx="1.2" ry="2.2" fill="#c9cdd6"/>`,
        `<g stroke="#cfd5e0" stroke-width="0.85" fill="none" stroke-linecap="round" opacity="0.95">`,
        `<path d="M12,2.6 L12,5 M12,3.6 L14.5,2.6 M18,5 L18,2.6 L20.5,4.8 M24,2.6 L26,3.8 L24,5 M30,2.6 L30,5 M30,3.8 L32.5,3.8" />`,
        `<path d="M63.5,3.8 L66,3.8 M66,2.6 L66,5 M72,2.6 L70,3.8 L72,5 M75.5,4.8 L78,2.6 L78,5 M81.5,2.6 L84,3.6 M84,2.6 L84,5" />`,
        `<path d="M14,19.2 L14,21.4 M12.5,20.3 L15.5,20.3 M20,19.2 L22,21.4 M22,19.2 L20,21.4 M27,19.2 L27,21.4 L29.5,20.3" />`,
        `<path d="M43,20.3 L48,18.6 L53,20.3 L48,22 Z" stroke="url(#silverBevel)" stroke-width="0.8"/>`,
        `<path d="M66.5,20.3 L69,21.4 L69,19.2 M74,19.2 L76,21.4 M76,19.2 L74,21.4 M80.5,20.3 L83.5,20.3 M82,19.2 L82,21.4" />`,
        `</g>`,
        `</svg>`,
        `</div>`,
        daedricCapMarkup(true),
        `</div>`
    ].join('');
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
            daedricFrameMarkup();
    });
}

function resetMinimal() {
    if (!minimalState.active) return;
    minimalState.combat = false;
    minimalState.moving = false;
    minimalState.casting = false;
    for (const [key, stat] of Object.entries(minimalState.stats)) {
        Object.assign(stat, { current: 0, value: 0, max: 0, drainDelta: 0, trail: 0, hitAt: 0,
            initialized: false, enabled: false });
        renderMinimalStat(key);
    }
    minimalState.light = { sneaking: false, level: null };
    clearTimeout(poiseShieldState.breakTimer);
    Object.assign(poiseShieldState, { initialized: false, previousPercent: 1, breakUntil: 0, breakTimer: null });
    document.getElementById('poise-shield-widget').classList.toggle('broken', false);
    renderLightGem();
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
    stat.drainDelta = stat.initialized ? Math.max(0, stat.value - current) : 0;
    if (!stat.initialized || stat.max !== max || !stat.enabled) {
        stat.trail = percent;
    } else if (percent < stat.current) {
        stat.hitAt = performance.now();
    } else {
        stat.trail = Math.max(stat.trail, percent);
    }
    stat.current = percent;
    stat.value = current;
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
        (key === 'magicka' && minimalState.casting) || ((key === 'stamina' || key === 'poise') && minimalState.moving) ||
        !statIsFull(key, stat) ||
        (key === 'poise' && minimalState.debuffsVisible));
    row.classList.toggle('hidden', !show);

    // Poise keeps its fixed CSS width so armor weight and Dragonhide change hit size, not bar length.
    if (key !== 'poise' && stat.max > 0) row.style.width = `${minimalBarWidth(stat.max)}px`;

    row.querySelector('.min-fill').style.transform = `scaleX(${stat.current / 100})`;
    row.querySelector('.min-trail').style.transform = `scaleX(${stat.trail / 100})`;
    if (key === 'poise') renderPoiseShield(stat, show);
}

function setPoiseShieldLevel(name, percent) {
    const ratio = Math.max(0, Math.min(1, percent));
    document.getElementById('poise-shield-cavity').style
        .setProperty(name, `${(ratio * 100).toFixed(3)}%`);
}

function renderPoiseShield(stat, show) {
    const widget = document.getElementById('poise-shield-widget');
    const shieldVisible = minimalState.theme === 'daedric' && poiseShieldState.enabled &&
        show && stat.enabled && stat.max > 0;
    widget.hidden = !shieldVisible;
    setPoiseShieldLevel('--shield-fill-level', stat.current / 100);
    setPoiseShieldLevel('--shield-trail-level', stat.trail / 100);
    const meniscus = document.getElementById('poise-shield-meniscus');
    meniscus.style.opacity = stat.current > 1 && stat.current < 99 ? '0.9' : '0';
    widget.classList.toggle('low-poise', stat.current > 0 && stat.current <= 25);

    if (poiseShieldState.initialized && stat.enabled && poiseShieldState.previousPercent > 0 && stat.current <= 0) {
        clearTimeout(poiseShieldState.breakTimer);
        const stage = document.getElementById('poise-shield-stage');
        widget.classList.toggle('broken', false);
        void (stage ? stage.offsetWidth : widget.offsetWidth);
        widget.classList.toggle('broken', true);
        poiseShieldState.breakUntil = performance.now() + POISE_SHIELD_BREAK_MS;
        poiseShieldState.breakTimer = setTimeout(() => {
            poiseShieldState.breakUntil = 0;
            poiseShieldState.breakTimer = null;
            widget.classList.toggle('broken', false);
        }, POISE_SHIELD_BREAK_MS);
    } else if (performance.now() >= poiseShieldState.breakUntil && (stat.current > 0 || !stat.enabled)) {
        widget.classList.toggle('broken', false);
    }
    poiseShieldState.previousPercent = stat.current;
    poiseShieldState.initialized = poiseShieldState.initialized || stat.initialized;
}

function updateMinimalStats(h, hm, m, mm, s, sm, combat, moving, casting = false) {
    if (!minimalState.active) return;
    if (typeof combat !== 'boolean' || typeof moving !== 'boolean' || typeof casting !== 'boolean') {
        console.error('Invalid minimalist visibility state'); return;
    }
    minimalState.combat = combat;
    minimalState.moving = moving;
    minimalState.casting = casting;
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
    minimalState.light = { sneaking: isSneaking, level: lightLevel };
    renderLightGem();
}

// Drives the light gem of the active theme from the last light telemetry and hides the other theme's gem.
function renderLightGem() {
    const { sneaking, level } = minimalState.light;
    const visible = minimalState.active && minimalState.gemEnabled && sneaking && level !== null;
    const gem = document.getElementById('light-gem');
    const core = document.getElementById('daedric-light-gem');
    gem.hidden = !(visible && minimalState.theme === 'minimalist');
    core.hidden = !(visible && minimalState.theme === 'daedric');
    if (!visible) return;
    const lightLevel = level;
    if (minimalState.theme === 'daedric') {
        const b = lightLevel / 100;
        core.setAttribute('aria-label', `Light level: ${lightLevel} out of 100`);
        core.style.opacity = String(0.25 + b * 0.75);
        core.style.boxShadow = `0 0 ${Math.round(b * 16)}px rgba(240, 198, 92, ${0.2 + b * 0.75})`;
        return;
    }
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
    const iconActive = debuffIconState.enabled ? debuffIconState.ids : [];
    const shieldActive = minimalState.theme === 'daedric' && poiseShieldState.enabled ?
        debuffIconState.ids : [];
    document.getElementById('poise-debuff-icons').hidden = iconActive.length === 0;
    for (const id of POISE_DEBUFF_IDS) {
        document.getElementById(`pd-icon-${id}`).classList.toggle('pd-off', !iconActive.includes(id));
    }
    const ceiling = debuffIconState.ceilingRatio;
    const row = document.getElementById('min-poise');
    const widget = document.getElementById('poise-shield-widget');
    for (const id of POISE_DEBUFF_IDS) {
        row.classList.toggle(`debuff-${id}`, iconActive.includes(id));
        widget.classList.toggle(`debuff-${id}`, shieldActive.includes(id));
    }
    row.classList.toggle('has-ceiling', iconActive.includes('enfeebled') && ceiling < 1);
    row.style.setProperty('--poise-ceiling', String(ceiling));
    const shieldCeilingActive = shieldActive.includes('enfeebled') && ceiling < 1;
    const shieldCeiling = document.getElementById('poise-shield-ceiling');
    shieldCeiling.style.opacity = shieldCeilingActive ? '0.95' : '0';
    widget.classList.toggle('has-ceiling', shieldCeilingActive);
    widget.style.setProperty('--poise-ceiling', String(ceiling));
    widget.style.setProperty('--poise-shield-ceiling', `${(ceiling * 100).toFixed(3)}%`);
    renderPoiseShieldEffects();
    const visible = (minimalState.theme === 'daedric' ? shieldActive : iconActive).length > 0;
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
    'stamina-frost': ['min-health', 'min-stamina']
};
const reactiveBarState = { enabled: false, ids: [] };
const REACTIVE_SHIELD_EFFECTS = {
    'health-sapped': 'effect-sapped',
    'health-poison': 'effect-poison',
    'health-fire': 'effect-fire',
    'stamina-frost': 'effect-cold',
    'health-bleed': 'effect-bleed'
};

function renderReactiveBars() {
    const active = reactiveBarState.enabled ? reactiveBarState.ids : [];
    for (const [effect, rows] of Object.entries(REACTIVE_BAR_EFFECTS)) {
        for (const row of rows) document.getElementById(row).classList.toggle(effect, active.includes(effect));
    }
    const shield = document.getElementById('poise-shield-widget');
    for (const [effect, shieldClass] of Object.entries(REACTIVE_SHIELD_EFFECTS)) {
        shield.classList.toggle(shieldClass, active.includes(effect));
    }
    renderPoiseShieldEffects();
}

function renderPoiseShieldEffects() {
    const widget = document.getElementById('poise-shield-widget');
    const paints = [
        [['effect-sapped', 'debuff-sapped'], '#e0c2ff', '#55308a', '#d5a7ff'],
        [['effect-poison', 'debuff-enfeebled'], '#d7ff5c', '#3f8c16', '#c4ff4d'],
        [['effect-fire', 'debuff-scorched'], '#ffd166', '#b82d08', '#ff6a1f'],
        [['effect-cold', 'debuff-numbed'], '#d9fbff', '#1d7fa3', '#9af0ff'],
        [['effect-bleed', 'debuff-hemorrhaging'], '#ff8a82', '#8c1018', '#ff4d4d']
    ];
    let c1 = '#ffe066';
    let c2 = '#8a6410';
    let stroke = '#d8b54b';
    const selected = paints.find(([states]) =>
        states.some(state => widget.classList.contains(state)));
    if (selected) {
        c1 = selected[1];
        c2 = selected[2];
        stroke = selected[3];
    }
    const cavity = document.getElementById('poise-shield-cavity');
    cavity.style.setProperty('--shield-c1', c1);
    cavity.style.setProperty('--shield-c2', c2);
    document.getElementById('poise-shield-effect-frame').setAttribute('stroke', stroke);
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

function updateRogueStaggerAlert(alert) {
    if (typeof alert !== 'string') {
        console.error('Invalid rogue stagger alert');
        return;
    }
    const row = document.getElementById('pt-rogue-stagger');
    row.hidden = !alert;
    row.textContent = alert;
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
