# Design Specification: Live Poise Telemetry & Dev Overlay (Prisma UI)

## Overview
Add a lightweight, toggleable, non-interactive **Live Poise Telemetry & Dev Overlay** to the Minimalist HUD (Prisma UI) so all posture mechanics, regen timers, active poise debuffs, and hit-by-hit damage formulas can be verified live in-game without diagnostic builds or log parsing.

- **Delivery Method:** Rendered inside the existing Prisma UI overlay (`minimal.html` / `minimal.css` / `minimal.js`), gated behind a new `[HUD]` INI setting (`bShowPoiseTelemetry = false`) and a checkbox in the SKSE Menu Framework **HUD** tab.
- **Zero Input Interference:** Uses `pointer-events: none` and requires no cursor focus or input blocking, allowing full real-time combat testing.
- **Live Reload Support:** Toggles cleanly on **F10** INI reload or menu checkbox change without requiring a game restart or save reload.

---

## 1. Configuration & UI Settings (`[HUD]`)

Add the following setting to `arpghud.ini`, `main.cpp` loader/exports, the SKSE Menu Framework **HUD** tab, and `ResetHUDDefaults` (or `ResetPoiseDefaults`):

- **INI Key (`[HUD]`):** `bShowPoiseTelemetry = false` (`settings.showPoiseTelemetry`)
- **Menu Control:** Checkbox labeled **"Show Live Poise Telemetry (Dev)"** in the **HUD** tab.
- **Behavior When `false`:**
  - C++ skips building/serializing the telemetry payload (zero runtime overhead during normal play).
  - Prisma UI hides the telemetry container (`display: none`).

---

## 2. Telemetry Data Requirements (C++ Backend)

Populate a lightweight `BuiltinPoise::TelemetrySnapshot` at the end of `PlayerUpdate` and inside `ProcessHit` / `MagicPoise::Damage` **only** when `settings.showPoiseTelemetry` is `true`.

### A. Actor Live State (`Player` and `Last-Hit / Crosshair Target`)
For both the player (`BuiltinPoise::state`) and the currently tracked target (`npcStates` entry from the 2D target bar), expose:

1. **Identity & Tier:**
   - `name` (`std::string`, e.g., `"Player"`, `"Draugr Scourge"`, `"Giant Frostbite Spider"`)
   - `tier` (`std::string`, e.g., `"NPC"`, `"Light"`, `"Medium"`, `"Heavy"`, `"Boss"`, `"Summon"`)
2. **Posture Pool & Ceilings:**
   - `current` (`float`, 1 decimal place)
   - `maximum` (`float`, 1 decimal place)
   - `effectiveCeilingRatio` (`float`, e.g., `1.00F` normally, `0.70F–0.85F` when **Enfeebled**, clamped by `fMinGlobalRegenCeiling`)
   - `effectiveCeilingPoise` (`float`, `maximum * effectiveCeilingRatio`)
3. **Regeneration & Timers:**
   - `effectiveRegenRate` (`float`, per-second rate after **Sapped** `0.0F` and **Hemorrhaging** `0.65x`)
   - `regenDelayRemaining` (`float`, current `state.regenDelay` countdown in seconds)
   - `effectiveRegenDelayMax` (`float`, `settings.regenDelaySeconds + (isNumbed ? settings.numbedRegenDelayBonus : 0.0F)`)
   - `immunityRemaining` (`float`, current `state.immunity` post-break grace period in seconds)
4. **Active Poise Debuffs List:**
   - Array of active debuff structs (`id`, `label`, `detail`, `sourceEffectName`):
     - **Sapped:** `{ id: "sapped", label: "SAPPED", detail: "0.0/s regen", source: <ActiveEffect name> }`
     - **Enfeebled:** `{ id: "enfeebled", label: "ENFEEBLED", detail: "Cap 74%", source: <ActiveEffect name> }`
     - **Numbed:** `{ id: "numbed", label: "NUMBED", detail: "+1.0s delay", source: <ActiveEffect name> }`
     - **Hemorrhaging:** `{ id: "hemorrhaging", label: "HEMORRHAGING", detail: "0.65x regen", source: <ActiveEffect name> }`
     - **Scorched:** `{ id: "scorched", label: "SCORCHED", detail: "1.20x melee taken", source: <ActiveEffect name> }`
5. **Active Buffs / Flesh Summary (Optional Sanity Check):**
   - `fleshBonusPoise` (`float`, total poise currently granted by active Alteration Flesh spells / Dragonhide)

### B. Rolling Hit Math Log (Last 5 Events)
Store a ring buffer of the last **5** valid poise hit events involving the player (either `Player -> Target` or `Attacker -> Player`) so rapid multi-hit exchanges can be read clearly:

- `direction`: `"OUT"` (Player dealt poise damage) or `"IN"` (Player took poise damage)
- `attackerName` & `targetName` (`std::string`)
- `hitType`: `"1H Melee"`, `"2H Melee"`, `"Shield Bash"`, `"Weapon Bash"`, `"Creature"`, `"Projectile"`, `"Magic Burst"`, `"Concentration"`
- `baseDamage`: Raw starting value (`baseHitDamage + weaponWeight * weaponWeightDamageMultiplier` or `magnitude * magnitudeMultiplier`)
- `multipliers`: Ordered list of applied multiplier tags (`std::vector<std::string>`), e.g.:
  - `["Unblocked 2.50x", "Scorched 1.20x"]`
  - `["Shield Block 0.25x", "Boss 2.25x"]`
  - `["Ward 0.35x", "Resist 0.50x", "NPC Magic 0.60x"]`
- `finalDamage`: Exact poise subtracted (`float`)
- `brokePoise`: `bool` (`true` if this hit reduced `current` to `0.0F` and triggered a stagger)

### C. Equipped Weapon Benchmark (Player)
Compute on player equipment change so the UI can show live "Hits-to-Break" estimates:
- `playerUnblockedLightHit`: `(settings.baseHitDamage + rightHandWeight * settings.weaponWeightDamageMultiplier) * (isTwoHanded ? settings.twoHandedMultiplier : 1.0F) * settings.unblockedPhysicalMultiplier`
- `playerUnblockedPowerHit`: `playerUnblockedLightHit * settings.powerAttackMultiplier`

---

## 3. Prisma UI Layout & Visual Design (`minimal.html` / `minimal.css` / `minimal.js`)

- **Screen Placement:** Anchored to the **upper-left** (or mid-left below the weather widget), completely clear of the center crosshair, bottom-left player bars, and top-center 2D target bar.
- **Visual Style:** Compact dark translucent card (`rgba(12, 14, 18, 0.78)` with subtle `1px` border and monospace numeric alignment) matching the Minimalist HUD aesthetic.

### Panel Sections
1. **Header Bar:**
   - Title: `POISE TELEMETRY [F10 RELOAD]`
   - Equipped Weapon Readout: `1H Light: 26.2 | Power: 39.3`
2. **Player Posture Card:**
   - Bar/Numbers: `Poise: 55.0 / 73.0 (Flesh +18.0)`
   - Regen & Ceiling Line: `Regen: +8.0/s | Cap: 100% (73.0)`
   - Timer Badges (highlight when `> 0.0s`): `Delay: 0.0s / 3.0s` | `Immune: 0.0s`
   - Active Debuff Pills: Color-coded badges (e.g., Purple for **Sapped**, Green for **Enfeebled**, Cyan for **Numbed**, Crimson for **Hemorrhaging**, Orange for **Scorched**), displaying `None` when clean.
3. **Target Posture Card (Last Hit / Targeted Actor):**
   - Header: `Frost Troll [Heavy]`
   - Bar/Numbers + Estimator: `Poise: 112.0 / 150.0 (~4.3 light hits)`
   - Regen & Timer Line: `Regen: +8.0/s | Delay: 2.4s | Immune: 0.0s`
   - Active Debuff Pills: Shows live debuffs currently active on that specific enemy.
4. **Recent Hit Math Log (Last 5 Hits):**
   - Compact 5-line feed (newest on top), color-coded left border (gold for `OUT`, red for `IN`):
     - `[OUT] Player -> Frost Troll (2H Melee): 38.0 [Base 13.6 * 2H 1.40 * Unblocked 2.50 * Scorched 1.20]`
     - `[IN] Vampire -> Player (Magic Burst): 0.0 [kAbsorb -> Applied SAPPED]`
     - `[IN] Giant -> Player (Boss 2.25x): 9.0 [Base 8.0 * Creature 0.80 * Boss 2.25 * Shield 0.25]`
     - Hits that trigger a posture break append a bold **`[STAGGER!]`** tag.

---

## 4. Interop & Performance Guardrails

- **Throttled UI Updates:** Push `updatePoiseTelemetry(jsonPayload)` from `PlayerUpdate` at **10 Hz (`0.10s` interval)** plus immediately on any `ProcessHit` / `MagicPoise::Damage` event so countdown timers (`regenDelay`, `immunity`) animate smoothly without flooding Prisma UI every frame.
- **Clean State Reset:** Clear the target telemetry card and hit log buffer on save load, cell change, or Poise Reset button press.
- **Unit Test Coverage:**
  - Verify `TelemetrySnapshot` serializes all 5 debuff states, ceiling clamps, and hit multiplier breakdowns accurately.
  - Verify `bShowPoiseTelemetry = false` bypasses telemetry serialization completely.
