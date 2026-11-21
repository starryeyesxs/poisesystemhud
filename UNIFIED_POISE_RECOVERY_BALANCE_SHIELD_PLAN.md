# Unified Poise Recovery, Balance, and Shield Color Plan

## Scope

Read-only implementation plan. No source files are modified by this document.

Covered files:

- `src/builtin_poise_state.h`
- `src/builtin_poise.cpp`
- `src/main.cpp`
- `poisesystemhud.ini`
- `src/menu_framework.cpp`
- `view/index.html`
- `view/minimal.css`
- `view/minimal.js`
- Related C++ and JavaScript tests
- Documentation values asserted by tests

---

# Part 1: Keep poise at zero on break and add a post-break recovery surge

## Current break behavior

`State` is defined in `src/builtin_poise_state.h`.

The current `State::ApplyDamage()` hook is around line 765:

```cpp
HitResult ApplyDamage(const Settings& settings, float damage, PhysicalBonus raw = {})
{
    ...
    regenDelay = settings.regenDelaySeconds +
        ResolveDebuffEffects(settings, debuffs).delayBonus;

    current = (std::max)(0.0F, current - damage);
    if (current > 0.0F) {
        return { true, false, 0.0F, raw.bonus, raw.capped, false };
    }

    current = maximum;
    immunity = creatureImmunity
        ? settings.creatureStaggerImmunitySeconds
        : settings.staggerImmunitySeconds;

    return { true, true,
        std::clamp(damage / maximum, minimumBreakMagnitude, 1.0F),
        raw.bonus, raw.capped, false };
}
```

The exact refill to remove is:

```cpp
current = maximum;
```

## Current regeneration behavior

The requested `State::Update` is named `State::Advance()` in this repository, around line 795:

```cpp
void Advance(const Settings& settings, float delta)
{
    if (!settings.enabled || maximum <= 0.0F ||
        !std::isfinite(delta) || delta <= 0.0F) {
        return;
    }

    immunity = (std::max)(0.0F, immunity - delta);

    const float oldDelay = regenDelay;
    regenDelay = (std::max)(0.0F, regenDelay - delta);
    const float regenTime = (std::max)(0.0F, delta - oldDelay);

    const auto fx = ResolveDebuffEffects(settings, debuffs);
    const float rate = fx.sapped
        ? 0.0F
        : settings.regenRatePerSecond *
          regenMultiplier *
          fx.regenRateMult;

    const float ceiling = maximum * fx.ceilingRatio;
    if (current < ceiling) {
        current = (std::min)(ceiling, current + rate * regenTime);
    }

    debuffs.sappedPulse =
        (std::max)(0.0F, debuffs.sappedPulse - delta);
}
```

## Runtime call sites

`src/builtin_poise.cpp` drives regeneration through:

- player update: `state.Advance(configuration, delta)`;
- NPC catch-up: `entry.poise.Advance(configuration, elapsed)` in `CatchUp()`.

No new C++-to-JavaScript event is needed.

## State changes

Add to `State` in `src/builtin_poise_state.h`:

```cpp
bool recoveringFromBreak = false;
```

Add one named multiplier near the other file-local constants:

```cpp
inline constexpr float kPostBreakRegenMultiplier = 1.65F;
```

Reset `recoveringFromBreak` in:

- `State::Reset()`;
- invalid `SetMaximum()` reset paths;
- any path that removes the actor's valid maximum.

## `ApplyDamage()` plan

1. Preserve normal regen-delay assignment for ordinary non-breaking hits.
2. On a break:
   - leave `current = 0.0F`;
   - set `recoveringFromBreak = true`;
   - assign creature or standard stagger immunity;
   - preserve Sapped, Numbed, Hemorrhaging, and Enfeebled semantics.

Prefer:

```cpp
const auto fx = ResolveDebuffEffects(settings, debuffs);
regenDelay = settings.regenDelaySeconds + fx.delayBonus;

current = (std::max)(0.0F, current - damage);
if (current > 0.0F) {
    ...
}

current = 0.0F;
recoveringFromBreak = true;
immunity = creatureImmunity
    ? settings.creatureStaggerImmunitySeconds
    : settings.staggerImmunitySeconds;
regenDelay = fx.delayBonus;
```

This keeps an active Numbed delay while avoiding adding the normal post-hit delay on top of the stagger-immunity interval.

## `Advance()` plan

1. Capture the prior immunity value:

```cpp
const float oldImmunity = immunity;
immunity = (std::max)(0.0F, immunity - delta);
```

2. Do not regenerate while `oldImmunity > 0`, except for the part of a long update after immunity expires:

```cpp
const float recoveryDelta = oldImmunity > 0.0F
    ? (std::max)(0.0F, delta - oldImmunity)
    : delta;
```

3. Apply `regenDelay` only to that recovery portion:

```cpp
const float oldDelay = regenDelay;
regenDelay = (std::max)(0.0F, regenDelay - recoveryDelta);
const float regenTime =
    (std::max)(0.0F, recoveryDelta - oldDelay);
```

4. Continue decrementing debuff pulse durations with the full `delta`.

5. Resolve the normal rate exactly as today:

```cpp
float rate = fx.sapped
    ? 0.0F
    : settings.regenRatePerSecond *
      regenMultiplier *
      fx.regenRateMult;
```

6. If `recoveringFromBreak` and `current < maximum * 0.5F`:
   - spend only enough regeneration time to reach 50% at `rate * 1.65`;
   - spend the remaining time in that tick at normal `rate`;
   - clear `recoveringFromBreak` when 50% is reached.

Do not multiply an entire large NPC catch-up tick by 1.65 because time spent above 50% would be incorrectly accelerated.

7. Keep the existing recovery ceiling:

```cpp
const float ceiling = maximum * fx.ceilingRatio;
```

8. Preserve:
   - Sapped: rate zero;
   - Hemorrhaging: multiplied recovery rate;
   - Numbed: delay bonus;
   - Enfeebled: recovery ceiling;
   - standard/boss tier regen multipliers.

9. Regular damage that merely leaves poise below 50% must remain at 1.0×. The surge must depend on `recoveringFromBreak`.

## View consequence

`src/main.cpp` already sends:

```cpp
updateMinimalPoise(enabled, current, maximum, showNumbers)
```

After this change, `minimalStat()` in `view/minimal.js` naturally receives:

```js
current === 0
```

and computes zero percent.

`renderPoiseShield()` already detects:

```js
poiseShieldState.previousPercent > 0 && stat.current <= 0
```

then applies `.broken` for the existing 360ms wobble/crimson-flash lifetime. Removing the refill-on-break is the gameplay-side condition needed for reliable visual triggering.

## Break/recovery tests

Update `tests/builtin-poise.test.cpp`:

- replace break assertions expecting `state.current == state.maximum` with `state.current == 0`;
- confirm hits are absorbed during remaining immunity;
- assert immunity expiry alone does not skip from zero to full;
- assert no regeneration during immunity;
- assert recovery below 50% uses normal rate × 1.65;
- assert normal rate resumes at/above 50%;
- assert a long tick splits correctly at the 50% threshold;
- preserve boss-tier regeneration by multiplying it with the surge;
- update standard and creature immunity tests that currently expect refill.

Update `tests/poise-debuffs.test.cpp`:

- change the old “poise break still refills to the maximum” assertion;
- assert Enfeebled still leaves break poise at zero;
- assert Sapped keeps post-break recovery at zero;
- assert Hemorrhaging scales the 1.65× surge;
- assert Numbed delay applies after immunity;
- assert Enfeebled caps final recovery;
- assert regular low-poise recovery remains normal speed unless it follows a break.

Existing `tests/poise-shield.test.js` already covers positive-to-zero break triggering and the 360ms lifetime. Add only an explicit `updateMinimalPoise(true, 0, max)` integration assertion if needed.

---

# Part 2: Cohesive weapon and target poise defaults

## `src/builtin_poise_state.h`

Change `Settings`:

```cpp
float baseHitDamage = 1.0F;
float weaponWeightDamageMultiplier = 0.40F;
float twoHandedMultiplier = 0.95F;
float creatureMultiplier = 1.45F;
float projectileMultiplier = 0.65F;
float unblockedPhysicalMultiplier = 2.2F;
```

Do not change formulas or validation ranges.

## `poisesystemhud.ini`

Set:

```ini
fBaseHitPoiseDamage=1.0
fWeaponWeightDamageMultiplier=0.40
fUnblockedPhysicalMultiplier=2.2
fTwoHandedMultiplier=0.95
fProjectileMultiplier=0.65
fCreatureMultiplier=1.45
```

Update adjacent comments that describe the old balance assumptions.

## `src/main.cpp`

`src/main.cpp` loads all six through `readRange()` around lines 240–257. There are no hard-coded defaults there.

Keep the ranges:

- base hit: `0..10000`
- weapon weight: `0..100`
- multipliers: `0..10`

No new default mechanism is needed.

## `src/menu_framework.cpp`

`ResetPoiseDefaults()` currently writes:

```cpp
QueueIniWrite("fBaseHitPoiseDamage", "6.0", "Poise");
QueueIniWrite("fWeaponWeightDamageMultiplier", "0.375", "Poise");
QueueIniWrite("fTwoHandedMultiplier", "0.85", "Poise");
QueueIniWrite("fCreatureMultiplier", "0.8", "Poise");
QueueIniWrite("fProjectileMultiplier", "0.8", "Poise");
QueueIniWrite("fUnblockedPhysicalMultiplier", "2.5", "Poise");
```

Change reset values to:

```text
1.0
0.40
0.95
1.45
0.65
2.2
```

## C++ tests

### `tests/builtin-poise.test.cpp`

Update:

- `twoHandedMultiplier` default `0.85 → 0.95`;
- base hit and weapon-weight assertions;
- formulas containing:
  - `6.0F`
  - `0.375F`
  - `2.5F`
  - `0.85F`
  - creature `0.8F`
  - projectile `0.8F`
- weighted hits, bashes, blocked/unblocked hits, creature tier damage, and benchmark examples;
- `tiers.creatureMultiplier` expectation `0.8 → 1.45`.

Do not indiscriminately replace:

- `fPhysicalDamagePoiseCap = 6.0`;
- raw damage cap assertions;
- Boss tier multiplier `2.25`;
- base poise `40.0`.

### `tests/poise-telemetry.test.cpp`

Update telemetry breakdown expectations for:

- base damage;
- 2H factor;
- Unblocked factor;
- Creature factor;
- projectile benchmark;
- final damage.

Recalculate whole formulas instead of changing isolated constants.

## JavaScript/static tests

### `tests/builtin-poise-settings.test.js`

Update:

- shipped INI defaults;
- menu reset defaults;
- POISE_GUIDE defaults;
- old `6.0` and `0.375` expectations.

### `tests/creature-poise.test.js`

Update the creature multiplier from `0.8` to `1.45` in:

- INI assertion;
- menu reset assertion;
- test name.

### `tests/reactive-bars.test.js`

Update the two-handed-default test:

- title `0.85 → 0.95`;
- INI assertion;
- menu reset assertion.

### `tests/poise-telemetry.test.js`

Leave arbitrary telemetry fixtures alone unless a test explicitly represents shipped defaults. Do not mechanically replace hand-authored historical multipliers in rendering tests.

## Documentation

Update:

- `POISE_GUIDE.txt`: all six values;
- `README.md`: at least the `fUnblockedPhysicalMultiplier=2.5` statement, plus any descriptions affected by the six defaults;
- any other text assertions found by the test suite.

---

# Part 3: Make shield colors vivid and reliable in Ultralight

## Current markup

`view/index.html` currently has one shared gradient:

```html
<linearGradient id="poise-shield-fill-grad" ...>
    <stop id="poise-shield-fill-top" ... stop-color="#ffe066" />
    <stop id="poise-shield-fill-middle" ... stop-color="#e0b43a" />
    <stop id="poise-shield-fill-bottom" ... stop-color="#8a6410" />
</linearGradient>
```

The fill uses:

```html
<rect id="poise-shield-fill"
      ...
      fill="url(#poise-shield-fill-grad)"/>
```

The liquid is still covered by:

```html
<polygon ... fill="url(#poise-shield-specular-grad)"/>
```

with white opacity up to `0.28`, plus dark divider lines.

## Current runtime update

`renderPoiseShieldEffects()` in `view/minimal.js` mutates one shared gradient:

```js
document.getElementById('poise-shield-fill-top')
    .setAttribute('stop-color', colors[0]);
document.getElementById('poise-shield-fill-middle')
    .setAttribute('stop-color', colors[1]);
document.getElementById('poise-shield-fill-bottom')
    .setAttribute('stop-color', colors[2]);
```

The actual stop classes do not currently control colors in CSS. Reactive CSS primarily affects the shield frame.

`view/minimal.css` also uses custom-property stroke resolution:

```css
stroke: var(--poise-shield-frame-color, #d8b54b);
```

That is another potential Ultralight compatibility risk.

## Root cause

1. All states share one SVG paint server in the zero-size top-level `<defs>` SVG.
2. Mutating `<stop>` attributes can leave Ultralight using a cached gradient representation.
3. The stop attributes may be correct while every `url(#gradient)` consumer does not repaint immediately.
4. CSS classes/custom properties on SVG presentation attributes are less reliable than literal attributes in this environment.
5. White specular and dark divider overlays still soften apparent saturation.

## Robust gradient fix

Replace the mutable single gradient with dedicated literal gradients:

```html
<linearGradient id="poise-shield-fill-golden" ...>
    <stop offset="0%" stop-color="#ffe066" stop-opacity="1"/>
    <stop offset="48%" stop-color="#e0b43a" stop-opacity="1"/>
    <stop offset="100%" stop-color="#8a6410" stop-opacity="1"/>
</linearGradient>
```

Add dedicated gradients:

- `poise-shield-fill-sapped`
- `poise-shield-fill-poison`
- `poise-shield-fill-fire`
- `poise-shield-fill-cold`
- `poise-shield-fill-bleed`

Every stop must use explicit vivid hex colors and `stop-opacity="1"`. Do not use CSS custom properties in stop elements.

## Runtime switching

Change `renderPoiseShieldEffects()` to select one gradient ID and set the fill reference:

```js
fill.setAttribute('fill', `url(#${gradientId})`);
```

Do not mutate shared stop nodes.

Preserve deterministic precedence:

1. Sapped
2. Poison/Enfeebled
3. Fire/Scorched
4. Cold/Numbed
5. Bleed/Hemorrhaging
6. Golden/default

Always restore:

```text
url(#poise-shield-fill-golden)
```

when no state remains.

Shock must not participate in this mapping or in shield tinting.

## Frame colors

Avoid SVG CSS custom-property stroke resolution for Ultralight.

Use one of:

- explicit JS `stroke` attribute updates on `#poise-shield-effect-frame`;
- dedicated pre-colored frame paths selected by display/class state.

Retain CSS for:

- opacity;
- display;
- transform;
- animation selection.

## Layer cleanup

Make the fill visually dominant:

- reduce `poise-shield-specular-grad` maximum opacity from about `0.28` to roughly `0.12–0.16`;
- reduce dark cavity divider opacity;
- keep the trail as a distinct damage layer;
- never reintroduce a full-size dark basin overlay;
- keep the meniscus thin and bright.

## Shield tests

Update `tests/poise-shield.test.js`:

- assert every dedicated gradient exists exactly once;
- assert literal vivid stop attributes;
- assert the fill begins with `url(#poise-shield-fill-golden)`;
- assert state changes mutate the fill `fill` attribute;
- assert clearing states restores golden;
- assert Shock never tints or animates the shield;
- assert the removed basin gradient/overlay is not reintroduced.

Update `tests/minimal-style.test.js`:

- update any checks tied to the one shared gradient;
- retain duplicate-ID protection for `ironGrad`, `silverBevel`, and `spearBlade`;
- assert SVG gradient stops do not depend on CSS custom properties.

---

# Recommended execution order

1. Update poise state break and immunity logic in `src/builtin_poise_state.h`.
2. Update and expand C++ break/debuff recovery tests.
3. Update the six defaults in settings, INI, reset defaults, guides, and static tests.
4. Recalculate all C++ formulas and telemetry expectations.
5. Add dedicated Ultralight-safe SVG gradients and switch fill references in JavaScript.
6. Reduce remaining specular/divider suppression without changing shield geometry.
7. Run focused C++ tests.
8. Run all JavaScript tests.
9. Build with the repository's existing `xmake` command.
