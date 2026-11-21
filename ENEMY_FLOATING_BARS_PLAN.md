# Floating Enemy Health / Magicka / Poise Bars — Implementation Plan

**Status:** READY FOR IMPLEMENTATION (no code written yet)
**Scope:** Compact overhead enemy widget rendered through the existing Prisma UI 1.5.1 view, styled to match the Daedric Iron player bars.

---

## 0. Recon Summary

### 0.1 What TrueHUD does (github.com/ersh1/TrueHUD)

| Concern | TrueHUD technique | Source |
|---|---|---|
| **Head/body anchor** | `Utils::GetTargetPos()` → `BGSBodyPartData->parts[1]` (head) or `parts[0]` (body) `targetName` node via `Get3D2()->GetObjectByName()`; falls back to `GetLookingAtLocation()` (eye level) then `GetPosition()` for non-characters | `src/Utils.cpp` |
| **3D→2D projection** | `NiCamera::WorldPtToScreenPt3(worldToCamMatrix, viewport, pos, x, y, depth, 1e-5f)`; `depth < 0` ⇒ behind camera / offscreen | `src/Widgets/ActorInfoBar.cpp` |
| **Coordinate mapping** | `WorldPtToScreenPt3` returns **normalized [0,1]** coords (y-up); TrueHUD flips y (`1.0f - y`) and maps into `GFxMovieView::GetVisibleFrameRect()` | `ActorInfoBar.cpp` |
| **Distance scaling** | Linearized depth `fNear*fFar/(fFar + depth*(fNear-fFar))` remapped 500→2000 units to 100%→50% scale | `ActorInfoBar.cpp` |
| **Target discovery** | Event-driven: `TESCombatEvent` (add on combat start, remove on end), `TESHitEvent` (add on hit), `TESDeathEvent` / `TESEnterBleedoutEvent` (delayed removal) | `src/HUDHandler.cpp` |
| **Filtering** | `IsHostileToActor(player)`, `IsPlayerTeammateOrSummon()`, `IsInCombat()`, `HasLineOfSight()`, invisibility AV check | `InfoBarBase.cpp` |
| **Crowd cap** | Boss bars capped by `uBossBarMaxCount` with an overflow queue | `TrueHUDMenu.cpp` |
| **Camera matrices** | `g_worldToCamMatrix`, `g_viewPort`, `g_fNear`, `g_fFar` via `RELOCATION_ID` | `src/Offsets.h` |

### 0.2 What our repo already has (reuse, don't rebuild)

- **Prisma UI bridge** — `PRISMA_UI_API::IVPrismaUI1` (`RequestPluginAPI`, `CreateView`, `Invoke`, `InteropCall`, `RegisterJSListener`) in [PrismaUI_API.h](src/PrismaUI_API.h). The view is created at `TruePoise/index.html` and driven by a 100 ms Windows timer → `PollHud()` → per-widget `Send*()` functions that build a JS call string and `Invoke` it, deduped against the last-sent string ([main.cpp](src/main.cpp)).
- **Enemy poise state** — `BuiltinPoise::npcStates` (`unordered_map<FormID, NPCState>`) already tracks every managed enemy's `State { current, maximum, immunity, regenDelay, regenMultiplier, debuffs }`, tier, and handle. `ReadTarget()` already exposes the last-hit target's poise to the HUD ([builtin_poise.cpp](src/builtin_poise.cpp), [builtin_poise_state.h](src/builtin_poise_state.h)).
- **Enemy eligibility** — `TrackedTier()` already filters out the player, unloaded 3D, dead/bleeding-out/kill-move actors, excluded ambient critters, and flying creatures ([builtin_poise.cpp](src/builtin_poise.cpp)).
- **Health/Magicka read pattern** — `SendPlayerStats()` already reads `GetActorValue()` + `GetActorValueModifier(kDamage)` for current/max; the same pattern applies verbatim to any `RE::Actor*`.
- **Daedric Iron styling** — `daedricFrameMarkup()` / `daedricCapMarkup()`, gradients `ironGrad`/`silverBevel`/`spearBlade`, `.min-track`/`.min-layer`/`.min-fill`/`.min-trail` (the "phantom trace" is `.min-trail`, a bright `rgba(255,245,235,.78)` chunk that lags the fill), and per-stat `--c1/--c2` palettes ([minimal.js](view/minimal.js), [minimal.css](view/minimal.css)).
- **Config plumbing** — `HudSettings` struct + `menu_exports::get_*` pointers + `SendSettingsToHudImpl(changedKey)` + `RegisterMenuSection` ([main.cpp](src/main.cpp), [menu_framework.cpp](src/menu_framework.cpp)).

---

## 1. Visual Layout & Design Direction

A single compact **overhead widget** per enemy, anchored above the actor's head, reusing the Daedric Iron frame language scaled down.

```
        ┌──────────────────────────────┐   ← Health (red)   — full widget width, 28px→~14px tall
        │  ████████████████░░░░░░░░░░  │
        ├──────────────────────────────┤   ← Magicka (blue) — same width, connected directly below
        │  ████████░░░░░░░░░░░░░░░░░░  │
        └──────────────────────────────┘
                 ┌──────────────┐          ← Poise (yellow) — ~60% width, thinner, centered below
                 │  ███████░░░░  │
                 └──────────────┘
```

- **Health** — red `--c1:#c43a3a --c2:#7a1414`, full widget width.
- **Magicka** — blue `--c1:#3d6fd0 --c2:#16336f`, identical width, flush against Health (no gap).
- **Poise** — yellow `--c1:#e0b43a --c2:#8a6410`, ~60% of the Health/Magicka width, ~70% of their height, horizontally centered, flush under Magicka.
- **No stamina bar** — excluded entirely.
- **Phantom trace** — reuse `.min-trail` semantics: a bright chunk that lags the fill on damage and decays over ~600 ms (mirror the existing `setInterval` trail decay in [minimal.js](view/minimal.js)).
- **Frame** — reuse `daedricFrameMarkup()` but with a **scaled-down variant** (smaller `--bar-h`, smaller cap `viewBox` scale) so the spearhead caps and runes stay proportional at overhead size. The existing frame already uses `preserveAspectRatio="none"` + `vector-effect="non-scaling-stroke"`, so it stretches cleanly at any width.
- **Reactive behaviors** — optionally mirror the existing reactive classes (`health-fire`, `bar-shock`, `debuff-*`) onto enemy bars later; **out of scope for the first pass** but the DOM structure should leave class hooks in place.

---

## 2. C++ Architecture

### 2.1 New module: `src/enemy_bars.h` / `src/enemy_bars.cpp`

A self-contained module that owns discovery, projection, and payload assembly. It does **not** touch poise math — it reads `BuiltinPoise` state and vanilla actor values.

```cpp
namespace EnemyBars
{
    struct Entry {
        RE::FormID id;
        RE::ActorHandle handle;
        float health, maxHealth;
        float magicka, maxMagicka;
        float poise, maxPoise;
        bool  poiseImmune;
        float screenX, screenY;   // normalized [0,1], y-down
        float depth;              // linearized, for scale + culling
        bool  visible;            // on-screen, in front of camera, LOS
    };

    void Install();                       // register combat/hit/death event sinks
    void Configure(bool enabled);         // master toggle (config flag)
    [[nodiscard]] bool Collect(std::vector<Entry>& out, std::size_t maxCount);
}
```

### 2.2 Target discovery & filtering (event-driven, like TrueHUD)

Register `BSTEventSink` for `TESCombatEvent`, `TESHitEvent`, `TESDeathEvent`, `TESEnterBleedoutEvent` (same set TrueHUD uses). Maintain a small `unordered_map<FormID, ActorHandle>` of "active bar candidates":

- **Add** on `TESCombatEvent` (newState == kCombat) and `TESHitEvent` where the player (or a teammate) is the cause and the target is hostile.
- **Remove** on combat end, death, and bleedout (delayed removal so the bar fades rather than pops).
- **Filter** each candidate on collect:
  1. `TrackedTier(actor)` has a value (reuses existing eligibility: not player, 3D loaded, alive, not excluded/flying).
  2. `actor->IsHostileToActor(player)` (hostiles only for pass 1).
  3. `actor->IsInCombat()`.
  4. `player->HasLineOfSight(actor, dummy)` — skip occluded enemies.
  5. Invisibility AV == 0 (mirror TrueHUD).
- **Cap** simultaneous bars at a configurable `iMaxEnemyBars` (default 8). Sort candidates by linearized depth (nearest first) and keep the nearest N so crowded fights stay smooth and readable.

> Note: we already have a `npcStates` map in `BuiltinPoise` keyed by FormID. The enemy-bar candidate set is a *separate, smaller* map because it only needs actors that are currently hostile + in combat + on screen, not every tracked NPC. Do not couple the two.

### 2.3 3D→2D projection (head anchor + culling)

Reuse TrueHUD's proven approach, adapted to CommonLibSSE-NG:

1. **Anchor point** — resolve per actor:
   - `Get3D2()` → `GetObjectByName(race->bodyPartData->parts[1]->targetName)` (head node) if present;
   - else `parts[0]->targetName` (body/torso);
   - else `GetLookingAtLocation()` (eye level);
   - else `GetPosition()` (creatures without body-part data).
   - Add a small configurable Z offset (`fEnemyBarOffsetZ`, default ~30 units) so the widget floats above the head.
2. **Project** — `RE::NiCamera::WorldPtToScreenPt3(worldToCam, port, pos, x, y, depth, 1e-5f)` using `RE::Main::WorldRootCamera()`'s `worldToCam` matrix and `port` (both exposed in CommonLibSSE-NG; no raw `RELOCATION_ID` needed — see [NiCamera.h](lib/commonlibsse-ng/include/RE/N/NiCamera.h) and [Main.h](lib/commonlibsse-ng/include/RE/M/Main.h)).
3. **Cull** — `depth < 0` ⇒ behind camera ⇒ `visible = false`. Also clamp/flag when normalized x/y fall outside `[0,1]` (off-screen edges).
4. **Normalize** — flip y (`y = 1.0f - y`) so the payload is y-down, and pass **normalized [0,1] coordinates** to JS. JS multiplies by `window.innerWidth`/`innerHeight`, which makes the mapping resolution-independent and sidesteps any Prisma UI viewport-scaling ambiguity.
5. **Depth** — compute linearized depth `fNear*fFar/(fFar + depth*(fNear-fFar))` for distance-based scale and nearest-first sorting.

### 2.4 Reading Health / Magicka / Poise

- **Health/Magicka** — same pattern as `SendPlayerStats()`:
  ```cpp
  auto* av = actor->AsActorValueOwner();
  float health = av->GetActorValue(RE::ActorValue::kHealth);
  float maxHealth = health - actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth);
  // same for kMagicka
  ```
- **Poise** — add a new `BuiltinPoise::ReadEnemy(RE::Actor*, Values&)` accessor (or a `ReadEnemyPoise(FormID, ...)` that looks up `npcStates` under `stateMutex`, calls `CatchUp`, and returns `current/maximum/immunity`). This is the **only** change needed inside `builtin_poise.*` — a read-only accessor, no new state. If the actor isn't tracked (poise off, or not yet hit), report `poise = maxPoise = 0` and let JS hide the poise tier.

### 2.5 Payload → JS (extend the existing pipeline)

Add a `SendEnemyBars()` function in [main.cpp](src/main.cpp), called from `PollHud()` alongside `SendPlayerStats()`/`SendPoise()`. It:

1. Early-returns unless `settings.enableEnemyBars` (the config flag) and `viewReady`.
2. Calls `EnemyBars::Collect(entries, settings.maxEnemyBars)`.
3. Serializes to a compact JSON array and `Invoke`s `updateEnemyBars(<json>)`, deduped against `lastEnemyBarsScript` (same pattern as `SendPoiseTelemetry`).

Prefer `InteropCall(hudView, "updateEnemyBars", json)` over `Invoke` for the hot path (it's the documented high-performance path in [PrismaUI_API.h](src/PrismaUI_API.h)); fall back to `Invoke` if `InteropCall` proves unreliable in testing.

**JSON contract (one object per enemy):**
```json
[{"id":123456,"x":0.52,"y":0.31,"depth":1200.0,
  "health":80,"maxHealth":100,"magicka":40,"maxMagicka":60,
  "poise":55,"maxPoise":80,"immune":false}]
```
`id` is the FormID (stable key for JS pooling). `x`/`y` are normalized [0,1] y-down.

### 2.6 Config flag & SKSE Menu Framework wiring

- Add to `HudSettings`: `bool enableEnemyBars = false;` and `int maxEnemyBars = 8;` plus `float enemyBarScale = 1.0f;` and `float enemyBarOffsetZ = 30.0f;`.
- Read them in `LoadSettings()` (`bEnableEnemyBars`, `iMaxEnemyBars`, `fEnemyBarScale`, `fEnemyBarOffsetZ`).
- Add `menu_exports::get_enableEnemyBars()` / `get_maxEnemyBars()` / `get_enemyBarScale()` / `get_enemyBarOffsetZ()`.
- Add a "Enemy Bars" section in `OnMenuRender()` (checkbox + sliders) that calls `QueueIniWrite` + `SendSettingsToHudImpl("bEnableEnemyBars")`.
- In `SendSettingsToHudImpl`, on `bEnableEnemyBars` change, call `EnemyBars::Configure(settings.enableEnemyBars)` and clear `lastEnemyBarsScript`.
- On `OnDomReady`, push `configureEnemyBars(enabled, scale)` so the JS layer builds/hides the pool.

This keeps the feature behind a single `bool` so it can be surfaced in SKSE Menu Framework now and toggled live without a DOM reload.

---

## 3. UI Architecture (HTML / CSS / JS)

### 3.1 DOM template (pooled, not per-enemy)

Add one container and a fixed pool of N widget slots to [index.html](view/index.html):

```html
<div id="enemy-bars" hidden></div>
```

`minimal.js` builds the pool once (N = `maxEnemyBars`, default 8) on `configureEnemyBars()`:

```html
<div class="enemy-bar" data-id="" hidden>
  <div class="eb-row eb-health">
    <div class="eb-track"><div class="eb-trail"></div><div class="eb-fill"></div></div>
  </div>
  <div class="eb-row eb-magicka">
    <div class="eb-track"><div class="eb-trail"></div><div class="eb-fill"></div></div>
  </div>
  <div class="eb-row eb-poise">
    <div class="eb-track"><div class="eb-trail"></div><div class="eb-fill"></div></div>
  </div>
</div>
```

Each `.eb-row` reuses the Daedric Iron frame via a **scaled-down** `daedricFrameMarkup()` variant (smaller `--bar-h`, smaller cap scale). The `.eb-track`/`.eb-fill`/`.eb-trail` classes reuse the existing `.min-track`/`.min-fill`/`.min-trail` CSS rules by sharing selectors (or a small `.eb-*` alias block that reuses the same custom properties `--c1/--c2`, `--min-shine-alpha`, etc.).

### 3.2 Pooling & positioning (hardware-friendly)

- **Pool, never create/destroy per frame.** `updateEnemyBars(json)` walks the pool:
  - For each incoming enemy, claim the next free slot (or the slot already bound to that `id`), set `data-id`, and update `transform`.
  - For each slot not matched, set `hidden`.
- **Position with `transform: translate3d(x, y, 0)`** (GPU-composited, no layout thrash), where `x = entry.x * innerWidth`, `y = entry.y * innerHeight`. The widget is `position: fixed; top:0; left:0;` with a `translate(-50%, -100%)` anchor so it centers above the head point.
- **Scale with `transform: scale(...)`** composed into the same transform (distance-based scale from `depth`, multiplied by `enemyBarScale`).
- **Fill updates** use `transform: scaleX(pct)` on `.eb-fill` (same as the player bars) — no width reflow.
- **Trail (phantom)** — per-slot `trail` value that lags the fill and decays in the existing 33 ms `setInterval` loop (extend it to also decay enemy trails).
- **Poise tier** — hide `.eb-poise` when `maxPoise <= 0`; apply `.immune` class (dimmed fill) when `immune` is true.

### 3.3 JS API surface (new functions in [minimal.js](view/minimal.js))

- `configureEnemyBars(enabled, scale)` — build/hide the pool, set scale.
- `updateEnemyBars(json)` — validate, diff against current slot bindings, update transforms/fills/trails.
- `resetEnemyBars()` — clear all slots (called from `resetMinimal()`).

Validation mirrors the existing defensive style (finite checks, array shape checks, `console.error` on bad input).

---

## 4. Edge Cases & Risks

| # | Risk | Mitigation |
|---|---|---|
| 1 | **Creature head bones** — many creatures lack `parts[1]` (head) or any body-part data; some (dragons, giants) have huge/offset head nodes. | Anchor fallback chain: head → torso → `GetLookingAtLocation()` → `GetPosition()`. Add `fEnemyBarOffsetZ` so oversized actors can be tuned. Boss-tier actors can use a larger offset. |
| 2 | **Prisma UI viewport mapping** — unknown whether Prisma UI's HTML viewport is 1:1 with game pixels or scaled. | Pass **normalized [0,1]** coords from C++ and multiply by `window.innerWidth/innerHeight` in JS. This is resolution- and scale-independent and avoids any assumption about Prisma UI's internal scaling. |
| 3 | **Behind-camera / off-screen** — `WorldPtToScreenPt3` returns garbage or negative depth for points behind the camera. | Cull on `depth < 0`; clamp/flag normalized x/y outside `[0,1]`; hide the slot rather than rendering at a bogus position. |
| 4 | **DOM pooling churn** — creating/destroying nodes per frame kills frame rate. | Fixed pool sized to `maxEnemyBars`; only `transform`/`hidden`/`scaleX` mutate per update. No `innerHTML` writes after initial build. |
| 5 | **Crowded fights** — dozens of hostiles in combat. | Cap at `iMaxEnemyBars` (default 8), sort by depth, keep nearest. LOS + `IsInCombat` filters already prune most. |
| 6 | **Poise not tracked yet** — an enemy that hasn't been hit has no `npcStates` entry. | `ReadEnemy` returns `maxPoise = 0`; JS hides the poise tier. Poise tier appears once the actor is tracked (first hit). |
| 7 | **Thread safety** — `npcStates` is guarded by `stateMutex`; `Collect` runs on the UI task. | `ReadEnemy` takes `stateMutex` (same as `ReadTarget`). `Collect` runs inside `PollHud()` (already on the UI task via `AddUITask`). |
| 8 | **Event-sink lifetime** — combat/hit/death sinks must not fire after unload. | Register in `Install()` at `kDataLoaded`, mirror `StaggerSuppression::Install()`; clear the candidate map in `BeginGame`/`EndGame` (same lifecycle as `BuiltinPoise`). |
| 9 | **Vanilla enemy bars overlap** — vanilla already shows a target health bar. | Out of scope for pass 1; document that users should disable vanilla/TrueHUD enemy bars, or add a `bHideVanillaEnemyBar` follow-up (TrueHUD has `bHideVanillaTargetBar` as a precedent). |
| 10 | **InteropCall vs Invoke** — `InteropCall` is faster but less battle-tested in this codebase. | Default to `Invoke` (proven in every existing `Send*`), switch to `InteropCall` only after profiling shows the 100 ms JSON payload is a bottleneck. |

---

## 5. Step-by-Step Implementation Phases

### Phase 1 — Poise read accessor (smallest, unblocks everything)
1. Add `BuiltinPoise::ReadEnemy(RE::Actor*, Values&)` (or FormID variant) to [builtin_poise.h](src/builtin_poise.h) / [builtin_poise.cpp](src/builtin_poise.cpp): lock `stateMutex`, find `npcStates`, `CatchUp`, return `current/maximum/immunity`.
2. Build + run existing `builtin-poise` tests to confirm no regression.

### Phase 2 — `EnemyBars` module (discovery + projection)
1. Create [enemy_bars.h](src/enemy_bars.h) / [enemy_bars.cpp](src/enemy_bars.cpp): candidate map, event sinks, `Collect()` with filtering/cap/sort, head-anchor resolution, `WorldPtToScreenPt3` projection, normalized output.
2. Add `Install()`/`Configure()` and wire into `SKSEMessageHandler` (`kDataLoaded`) and `BeginGame`/`EndGame` lifecycle.
3. Add a temporary debug log (or reuse the telemetry overlay) to verify projection math against a known actor.

### Phase 3 — Config flag + settings plumbing
1. Add `enableEnemyBars`, `maxEnemyBars`, `enemyBarScale`, `enemyBarOffsetZ` to `HudSettings` + `LoadSettings()`.
2. Add `menu_exports::get_*` and a "Enemy Bars" menu section in [menu_framework.cpp](src/menu_framework.cpp).
3. Add `SendSettingsToHudImpl` handling for `bEnableEnemyBars`.

### Phase 4 — JS/HTML/CSS widget
1. Add `#enemy-bars` container to [index.html](view/index.html).
2. Add `configureEnemyBars` / `updateEnemyBars` / `resetEnemyBars` + pool builder + trail decay to [minimal.js](view/minimal.js).
3. Add `.eb-*` styles to [minimal.css](view/minimal.css) reusing the Daedric Iron frame + `--c1/--c2` palettes + `.min-trail` phantom.
4. Wire `configureEnemyBars` into `OnDomReady` and `resetEnemyBars` into `resetMinimal()`.

### Phase 5 — `SendEnemyBars()` integration
1. Add `SendEnemyBars()` to [main.cpp](src/main.cpp), call from `PollHud()`, dedup via `lastEnemyBarsScript`.
2. Clear `lastEnemyBarsScript` on reload/reset.

### Phase 6 — Verification
1. Build via `xmake` (view files are auto-copied to `PrismaUI/views/TruePoise/` by [xmake.lua](xmake.lua)).
2. In-game: spawn multiple hostiles, verify bars appear above heads, track movement, cull behind camera, cap at N, hide poise tier for untracked enemies, fade on death.
3. Toggle `bEnableEnemyBars` live via SKSE Menu Framework and confirm no DOM reload is required.

---

## 6. Files Touched (summary)

| File | Change |
|---|---|
| `src/enemy_bars.h` / `src/enemy_bars.cpp` | **new** — discovery, projection, `Collect()` |
| `src/builtin_poise.h` / `.cpp` | add `ReadEnemy()` read-only accessor |
| `src/main.cpp` | settings fields, `LoadSettings`, `SendEnemyBars`, `OnDomReady`, `SendSettingsToHudImpl`, `menu_exports` |
| `src/menu_framework.cpp` | "Enemy Bars" menu section |
| `view/index.html` | `#enemy-bars` container |
| `view/minimal.js` | pool builder, `configureEnemyBars`/`updateEnemyBars`/`resetEnemyBars`, trail decay |
| `view/minimal.css` | `.eb-*` styles reusing Daedric Iron frame |
| `TruePoise.ini` | `bEnableEnemyBars`, `iMaxEnemyBars`, `fEnemyBarScale`, `fEnemyBarOffsetZ` |
| `README.md` | document the new feature + settings |