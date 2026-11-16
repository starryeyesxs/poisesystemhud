# Implementation Checklist: Poise Status Debuffs & Live Prisma UI Telemetry Overlay

## Goal
Implement the 5 code-aligned **Poise Status Debuffs** (`Sapped`, `Enfeebled`, `Numbed`, `Hemorrhaging`, `Scorched`) alongside the toggleable **Live Poise Telemetry & Dev Overlay** (`bShowPoiseTelemetry`) in a single cohesive pass, without altering `fBasePoise` (`40.0`) or existing armor/flesh maximum poise formulas.

---

## Phase 1: INI Configuration, Settings Struct & Native Menu Integration

- [ ] **Extend `BuiltinPoise::Settings` (`builtin_poise.h`):**
  - Add `[Poise Debuffs]` settings:
    - `bool enableSappedDebuff = true;` (`bEnableSappedDebuff`)
    - `float enfeebledRegenCeilingMin = 0.70F;` (`fEnfeebledRegenCeilingMin`)
    - `float enfeebledRegenCeilingMax = 0.85F;` (`fEnfeebledRegenCeilingMax`)
    - `float numbedRegenDelayBonus = 1.0F;` (`fNumbedRegenDelayBonus`)
    - `float hemorrhagingRegenRateMultiplier = 0.65F;` (`fHemorrhagingRegenRateMultiplier`)
    - `float scorchedMeleeDamageMultiplier = 1.20F;` (`fScorchedMeleeDamageMultiplier`)
    - `float minGlobalRegenCeiling = 0.60F;` (`fMinGlobalRegenCeiling`)
  - Add `[HUD]` telemetry toggle:
    - `bool showPoiseTelemetry = false;` (`bShowPoiseTelemetry`)
- [ ] **Update `arpghud.ini` & `main.cpp` Loader:**
  - Add the `[Poise Debuffs]` section and `bShowPoiseTelemetry = false` under `[HUD]` with safe min/max clamping on load.
  - Ensure **F10** live reload updates all new settings immediately.
- [ ] **Expose in SKSE Menu Framework (`menu_framework.cpp`):**
  - Add `[Poise Debuffs]` sliders/toggles to the **Combat Balance** menu and include them in **Poise Reset** defaults.
  - Add **"Show Live Poise Telemetry (Dev)"** checkbox (`bShowPoiseTelemetry`) to the **HUD** tab.

---

## Phase 2: `ActiveEffect` Scanner & Debuff State Resolution (`builtin_poise.cpp`)

- [ ] **Create `ActiveDebuffs` Evaluation Helper (Mirroring `ActiveFlesh`):**
  - Iterate `actor->AsMagicTarget()->GetActiveEffectList()`, skipping null, inactive, dispelled, false-condition, or missing `EffectSetting` entries, and requiring hostile effects (`effect->IsHostile()`).
  - Resolve the **single strongest** active effect per category (no same-type additive stacking):
    1. **Sapped:** `enableSappedDebuff && archetype == Archetype::kAbsorb` → sets `sapped = true` (record source spell/effect name for telemetry).
    2. **Enfeebled:** `resistVariable == ActorValue::kPoisonResist` or `MagicAlchHarmful` keyword → scale `enfeebledCeilingRatio` between `enfeebledRegenCeilingMax` (`0.85F`) and `enfeebledRegenCeilingMin` (`0.70F`) based on magnitude; retain the lowest ratio across active poisons.
    3. **Numbed:** `resistVariable == ActorValue::kFrostResist` with `duration > 0.0F` (lingering DoT) → sets `numbed = true`.
    4. **Hemorrhaging:** `archetype == Archetype::kValueModifier` on `ActorValue::kHealth` matching bleed keyword/editorID (`Bleed` / unresisted physical DoT) → sets `hemorrhaging = true`.
    5. **Scorched:** `resistVariable == ActorValue::kFireResist` with `duration > 0.0F` (lingering DoT) → sets `scorched = true`.
- [ ] **Cache `ActiveDebuffs` on Player & `NPCState`:**
  - Refresh alongside existing `ActiveFlesh` checks and on hit/magic impact so per-tick `CatchUp()` reads cached debuff state cleanly without scanning effect lists every frame.

---

## Phase 3: Formula Integration (Regen, Delays & Incoming Melee)

- [ ] **Update Per-Tick Regeneration (`CatchUp` / `PlayerUpdate`):**
  - Compute `effectiveCeilingRatio = std::max(settings.minGlobalRegenCeiling, activeDebuffs.enfeebledCeilingRatio)` (`1.0F` when not Enfeebled).
  - Compute `regenCeiling = state.maximum * effectiveCeilingRatio`.
  - Compute `regenRateMult = activeDebuffs.hemorrhaging ? settings.hemorrhagingRegenRateMultiplier : 1.0F`.
  - Compute `effectiveRegenRate = activeDebuffs.sapped ? 0.0F : (settings.regenRatePerSecond * regenRateMult)`.
  - Clamp recovery strictly upward (`if (state.current < regenCeiling) state.current = std::min(regenCeiling, state.current + effectiveRegenRate * regenTime)`), never dropping existing poise if `state.current` is already above `regenCeiling`.
- [ ] **Update Regen Delay (`Numbed`):**
  - Apply `settings.numbedRegenDelayBonus` (`+1.0F`s) on top of `settings.regenDelaySeconds` while `activeDebuffs.numbed` is active.
- [ ] **Update Incoming Physical Melee Damage (`ProcessHit`):**
  - If defender `activeDebuffs.scorched` is `true` and hit is melee (`!hit.projectile`), multiply `damage *= settings.scorchedMeleeDamageMultiplier` (`1.20F`).
- [ ] **Verify Zero Direct Poise Damage from Lingering DoTs & `kAbsorb`:**
  - Confirm `MagicPoise::Damage` and `ApplyDamage` ignore lingering DoT ticks and `kAbsorb` drain beams while continuing to process initial burst impacts and pooled concentration spells.

---

## Phase 4: C++ Telemetry Snapshot & Hit Ring Buffer

- [ ] **Gate Behind `settings.showPoiseTelemetry`:**
  - Skip all telemetry string formatting and JSON serialization when `bShowPoiseTelemetry == false`.
- [ ] **Populate `TelemetrySnapshot`:**
  - **Player & Last-Hit Target:** `name`, `tier`, `current`, `maximum`, `effectiveCeilingRatio`, `effectiveRegenRate`, `regenDelayRemaining`, `effectiveRegenDelayMax`, `immunityRemaining`, `fleshBonusPoise`, and active `debuffs` array (`id`, `label`, `detail`, `source`).
  - **Weapon Benchmarks:** Compute `playerUnblockedLightHit` and `playerUnblockedPowerHit` on equipment refresh for live "hits-to-break" division.
  - **Rolling Hit Math Ring Buffer (Last 5 Hits):** Record `direction` (`OUT` / `IN`), `attackerName`, `targetName`, `hitType`, `baseDamage`, ordered `multipliers` list, `finalDamage`, `brokePoise`, and debuff-application events (e.g., `kAbsorb -> Applied SAPPED`).
- [ ] **Throttle Interop Calls:**
  - Push `updatePoiseTelemetry(jsonPayload)` to Prisma UI at **10 Hz (`0.10s`)** during `PlayerUpdate`, plus immediately on any hit event involving the player.
  - Clear target telemetry and hit log buffer on save load, cell change, or **Poise Reset**.

---

## Phase 5: Prisma UI Telemetry Overlay (`minimal.html` / `minimal.css` / `minimal.js`)

- [ ] **Add Non-Interactive Overlay Container (`minimal.html` & `minimal.css`):**
  - Anchor in upper-left/mid-left screen space with `pointer-events: none` and translucent dark/gold styling matching the Minimalist HUD.
  - Hide completely (`display: none`) when `bShowPoiseTelemetry` is `false`.
- [ ] **Implement `updatePoiseTelemetry(payload)` (`minimal.js`):**
  - Render **Header** (`POISE TELEMETRY [F10]` + player equipped weapon light/power poise damage).
  - Render **Player Posture Card** (`current / max`, `Flesh +X`, live `Regen +X.X/s`, `Cap %`, `Delay` / `Immune` timers, and color-coded debuff pills).
  - Render **Target Posture Card** (`Name [Tier]`, `current / max`, live `~X.X light hits` estimate, timers, and active enemy debuff pills).
  - Render **Recent Hit Math Feed** (last 5 hits, newest first, showing exact multiplier chain and `[STAGGER!]` badge on poise breaks).

---

## Phase 6: Unit Tests & Build Verification

- [ ] **Extend C++ Unit Tests (`builtin-poise.test.cpp`):**
  - Test all 5 debuffs individually (`Sapped`, `Enfeebled`, `Numbed`, `Hemorrhaging`, `Scorched`).
  - Test cross-type coexistence and `fMinGlobalRegenCeiling` (`0.60F`) clamping.
  - Test same-type non-stacking (strongest poison ceiling wins).
  - Test `TelemetrySnapshot` serialization and verify zero overhead when `bShowPoiseTelemetry = false`.
- [ ] **Extend JS Unit Tests (`builtin-poise-settings.test.js`):**
  - Verify `updatePoiseTelemetry` DOM rendering, debuff pill formatting, and `~hits` math calculation.
- [ ] **Build & Verify:**
  - Compile all targets cleanly and confirm all C++ and JS unit tests pass.
