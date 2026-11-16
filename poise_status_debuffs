*DESIGN SPECIFICATION: POISE STATUS DEBUFFS (CODE-ALIGNED)
=========================================================

OVERVIEW & ARCHITECTURE FIT
---------------------------
Instead of dealing direct poise damage, hostile damage-over-time (DoT) and drain/leech active effects apply distinct posture debuffs. Each debuff modifies a single existing variable in BuiltinPoise::State or the physical damage calculation, requiring zero new tick timers or separate damage pools.

Detection follows the exact same active-effect iteration pattern already used by ActiveFlesh (skipping null, inactive, dispelled, false-condition, or missing-effect entries on the player and tracked NPCState entries).

THE 5 POISE DEBUFFS (EXACT CODE MAPPING)
----------------------------------------
1. Sapped (Hostile Drains / Absorb Effects)
   - Trigger: Hostile active effect with archetype kAbsorb (e.g., Vampiric Drain, Absorb Health/Stamina/Magicka)
   - Code Variable Modified: Effective regenRate in per-tick regeneration
   - Exact Formula Change:
     effectiveRegenRate = isSapped ? 0.0F : (settings.regenRatePerSecond * regenRateMult)
   - INI / Settings Member:
     settings.enableSappedDebuff <- bEnableSappedDebuff = true
   - Direct Poise Damage: None (bypasses MagicPoise::Damage and ApplyDamage)

2. Enfeebled (Hostile Poisons / Venoms)
   - Trigger: Hostile active effect with Poison resist/harmful flag (MagicAlchHarmful / kResistPoison)
   - Code Variable Modified: Recovery ceiling in per-tick regeneration (clamps current recovery without shrinking state.maximum)
   - Exact Formula Change:
     regenCeiling = state.maximum * effectiveCeilingRatio
     state.current = min(regenCeiling, state.current + effectiveRegenRate * regenTime)
     (Note: Only applies when state.current < regenCeiling; never drops existing poise below the ceiling)
   - INI / Settings Members:
     settings.enfeebledRegenCeilingMin <- fEnfeebledRegenCeilingMin = 0.70
     settings.enfeebledRegenCeilingMax <- fEnfeebledRegenCeilingMax = 0.85
     (Strongest active poison magnitude scales ceiling between max and min)
   - Direct Poise Damage: None

3. Numbed (Lingering Frost / Chill DoTs)
   - Trigger: Hostile lingering Frost damage-over-time active effect (kResistFrost with duration > 0)
   - Code Variable Modified: state.regenDelay assignment on hit/tick or active delay tracking
   - Exact Formula Change:
     While Numbed is active, effective regen delay threshold adds settings.numbedRegenDelayBonus (+1.0F seconds) on top of settings.regenDelaySeconds.
   - INI / Settings Member:
     settings.numbedRegenDelayBonus <- fNumbedRegenDelayBonus = 1.0
   - Direct Poise Damage: None on lingering chill ticks (Burst frost impacts still route through MagicPoise::Damage)

4. Hemorrhaging (Hostile Bleed Effects)
   - Trigger: Hostile bleed active effect (kValueModifier on kHealth with bleed keyword/editorID or unresisted physical DoT)
   - Code Variable Modified: settings.regenRatePerSecond multiplier in per-tick regeneration
   - Exact Formula Change:
     regenRateMult = isHemorrhaging ? settings.hemorrhagingRegenRateMultiplier : 1.0F
     state.current = min(regenCeiling, state.current + (settings.regenRatePerSecond * regenRateMult) * regenTime)
   - INI / Settings Member:
     settings.hemorrhagingRegenRateMultiplier <- fHemorrhagingRegenRateMultiplier = 0.65
   - Direct Poise Damage: None

5. Scorched (Lingering Fire Burn DoTs)
   - Trigger: Hostile lingering Fire burn damage-over-time active effect (kResistFire with duration > 0)
   - Code Variable Modified: Incoming physical melee damage calculation
   - Exact Formula Change:
     In the incoming physical poise damage formula, if target is Scorched and hit is melee (not projectile):
     damage *= settings.scorchedMeleeDamageMultiplier
   - INI / Settings Member:
     settings.scorchedMeleeDamageMultiplier <- fScorchedMeleeDamageMultiplier = 1.20
   - Direct Poise Damage: None on lingering burn ticks (Burst and AoE explosion fire hits still route through MagicPoise::Damage)

STACKING, GUARDRAILS & CONSTRAINTS
----------------------------------
- No Same-Type Stacking: Multiple active effects in the same debuff category never stack additively. Always select the single strongest active effect (lowest ceiling ratio for Enfeebled; single boolean/multiplier flag for Sapped, Numbed, Hemorrhaging, and Scorched).
- Cross-Type Coexistence: Different debuff types may apply simultaneously on the same BuiltinPoise::State, bounded by the global recovery floor.
- Global Recovery Floor:
  settings.minGlobalRegenCeiling <- fMinGlobalRegenCeiling = 0.60
  effectiveCeilingRatio = max(settings.minGlobalRegenCeiling, enfeebledCeilingRatio)
- Baseline Protection: Keep settings.basePoise (fBasePoise = 40.0) unchanged until these debuffs are implemented and playtested.
- Configuration & UI: Expose under [Poise Debuffs] in arpghud.ini and inside a dedicated collapsing "Poise Status Debuffs" header in the SKSE Menu Framework menu (with live F10 reload and Poise reset support).

MASTER TOGGLE & ACCESSIBILITY
-----------------------------
- Master INI Key ([Poise Debuffs]):
  settings.enablePoiseDebuffs <- bEnablePoiseDebuffs = true
- Behavior When False:
  - Skip the hostile ActiveDebuffs evaluation completely (zero runtime cost).
  - Keep ceilingRatio = 1.0F, regenRateMult = 1.0F, numbedDelayBonus = 0.0F, and scorchedMeleeMult = 1.0F.
  - Hide all HUD status debuff icons and report debuffs: [] in telemetry.
- Menu Integration: Place the "Enable Poise Status Debuffs" checkbox at the very top of the collapsing "Poise Status Debuffs" section so players who find debuffs too punishing can disable the entire system in one click, and include it in Poise Reset defaults.

MINIMALIST HUD STATUS DEBUFF ICONS (PRISMA UI)
----------------------------------------------
Prevent players from mistaking active poise debuffs (such as paused regen or a capped recovery ceiling) for a HUD bug by displaying compact, self-explanatory visual indicators directly on the player poise bar whenever bEnablePoiseDebuffs is true and at least one debuff is active:

- Zero External Asset Dependencies: Render crisp 14px x 14px inline SVG vector icons inside a #poise-debuff-icons strip docked neatly next to or beneath the player poise bar (completely hidden when debuffs is empty).
- Icon & Visual Cue Mapping:
  1. Sapped (Purple Drain / Lock Icon): Signals posture regeneration is paused (0.0/s) while an absorb/drain effect is active.
  2. Enfeebled (Green Poison Drop + Ceiling Marker): Displays a green poison icon and renders a subtle vertical tick marker on the player poise bar at effectiveCeilingRatio (70%–85%) so players visually see where recovery stops.
  3. Numbed (Cyan Frost Crystal Icon): Signals frost chill has extended the posture regeneration delay (+1.0s).
  4. Hemorrhaging (Crimson Blood Drop Icon): Signals active bleeding is slowing posture regeneration speed (0.65x).
  5. Scorched (Orange Flame Icon): Signals burning/fire DoT is increasing incoming melee posture damage (1.20x).
- HUD Visibility Toggle ([HUD]):
  - Add bShowPoiseDebuffIcons = true (hudSettings.showPoiseDebuffIcons) in arpghud.ini and a "Show Poise Debuff Icons" checkbox under "Widget Visibility & Display" in SKSE Menu Framework (included in Reset HUD defaults).
- Shared Payload Path: Drive both the Minimalist HUD debuff icons and the Live Poise Telemetry Overlay (showpoisedev) from the same debuffs array and ceilingRatio fields (sending player debuff IDs + ceilingRatio whenever bShowPoiseDebuffIcons is true, even when bShowPoiseTelemetry is false).
