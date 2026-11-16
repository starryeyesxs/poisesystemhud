# Design Specification: Creature Poise Polish, Shield vs. Weapon Block, 2D Target Bar & Non-NPC Magic

## Overview
Consolidate all findings from the Creature Poise in-game verification session into one cohesive update:
1. **Creature Lifecycle & Tier Polish:** Promote Ice Wolves to `Medium`, bump Tier 3 (`Heavy`) poise multiplier from `2.75` to `3.75` (`~150` poise), move creature equipment/Flesh-spell refreshes and stale-entry cleanup onto `PlayerUpdate`, add tiered outgoing creature poise damage (including `2.25x` for Bosses), add `fCreatureStaggerImmunitySeconds = 2.0`, and boost commanded summons (`1.50x`).
2. **Shield vs. Weapon Physical Block Separation:** Split physical block mitigation so blocking with a shield (`0.25x`) protects posture significantly better than blocking with a bare weapon (`0.45x`).
3. **2D Target Poise Bar (HUD Testing & Gameplay Feature):** Add a lightweight top-center Minimalist HUD bar showing the last-hit/targeted actor's name, tier, and live `current / maximum` poise numbers.
4. **Non-`ActorTypeNPC` Magic Caster Fix & Balanced Boss Spell Scaling:** Fix incoming spells from non-humanoid/boss casters (Dragon Priests, Dragons, Hagravens) and `kDualValueModifier` / cloak spell archetypes, paired with `fNPCMagicPoiseMultiplier = 0.60` while keeping active Ward magic poise mitigation at `~75–80%` until magicka-drain-on-ward-hit is implemented later.

---

## 1. Creature Classification & `PlayerUpdate` Lifecycle Polish

### A. Promote Ice Wolves (`Light` → `Medium`) & Bump Tier 3 Heavy (`2.75` → `3.75`)
- **Ice Wolf Promotion (`creature_tier.h`):** Vanilla Ice Wolves share `WolfRace` (`race=WolfRace tier=light`). Match actor/base display name for `"Ice Wolf"` (case-insensitive) prior to the generic `WolfRace` fallback and classify Ice Wolves as `CreatureTier::Medium` (`1.35x` `fBasePoise` ≈ `54.0` poise).
- **Tier 3 Heavy Multiplier (`fHeavyCreaturePoiseMultiplier`):** Change default from `2.75` (`110` poise, which broke in ~2 one-handed hits) to **`3.75`** (`150` poise at `40` base), creating a clean `1 -> 2 -> 4-5 -> 8-10` one-handed hit progression across Light (`0.75`), Medium (`1.35`), Heavy (`3.75`), and Boss (`6.00`).

### B. Move Creature Refresh & Stale Cleanup onto `PlayerUpdate`
- **Problem:** Skyrim's per-actor `CharacterUpdate` vtable hook does not tick for non-`ActorTypeNPC` creatures. While lazy `CatchUp()` handles regen and immunity on hit, mid-fight spell buffs (such as a **Dragon Priest casting Ebonyflesh**) and flying/death entry cleanup should not rely on `CharacterUpdate`.
- **Change (`builtin_poise.cpp`):** Run periodic equipment/Flesh-spell `MaximumPoise` refreshes and untrackable/flying/dead cleanup for creature entries inside `PlayerUpdate` (throttled cleanly alongside existing housekeeping).

---

## 2. Tiered Outgoing Creature Damage, Post-Break Immunity & Summons

### A. 4-Tier Outgoing Creature Damage Curve (`fCreatureMultiplier = 0.80` base)
Multiply `settings.creatureMultiplier` by `CreatureDamageTierMultiplier(settings, aggressorTier)` in `ProcessHit`:
- `fLightCreatureDamageMultiplier = 0.60` *(Tier 1 Light: `9.6` unblocked)*
- `fMediumCreatureDamageMultiplier = 1.20` *(Tier 2 Medium: `19.2` unblocked — punishes R1 trading against Sabre Cats, Ice Wolves, and Chaurus)*
- `fHeavyCreatureDamageMultiplier = 1.375` *(Tier 3 Heavy: `22.0` unblocked — validated live against Frost Trolls and Bears)*
- `fBossCreatureDamageMultiplier = 2.25` *(Tier 4 Boss: `36.0` unblocked — cracks Light Armor in 2 slams and Heavy Armor in 3–4 unblocked slams; bleeds `9.0` through a shield or `16.2` through a weapon block)*
- `CreatureTier::Unknown` / `None` / `Excluded` defaults to `1.00F`.

### B. Non-Blocking Creature Post-Break Immunity (`fCreatureStaggerImmunitySeconds`)
- Add `fCreatureStaggerImmunitySeconds = 2.0` (`settings.creatureStaggerImmunitySeconds`). When a non-`ActorTypeNPC` creature suffers a poise break, set `poise.immunity = settings.creatureStaggerImmunitySeconds` so it recovers from its ~`1.3s` stagger animation and gets a fair counterattack window instead of being stunlocked by light-attack spam.

### C. Summon & Commanded Actor Poise Bonus (`fSummonPoiseMultiplier`)
- Add `fSummonPoiseMultiplier = 1.50` (`settings.summonPoiseMultiplier`). In `MaximumPoise`, multiply computed max poise by `settings.summonPoiseMultiplier` when `actor->IsCommandedActor()` is `true` (bumping a *Familiar* from `30.0` to `45.0` poise and scaling Atronachs/Dremora proportionally).

---

## 3. Shield vs. Weapon Physical Block Separation

- **Problem:** Currently both shield blocks and bare-weapon blocks share `fBlockedPhysicalMultiplier` (`0.20`), making weapon blocking as effective as a shield and allowing Heavy Armor + block to absorb ~25 consecutive Giant slams before breaking.
- **Change (`BuiltinPoise::Settings` & `ProcessHit`):**
  - Replace/split physical block mitigation into:
    - `fShieldBlockedPhysicalMultiplier = 0.25` *(Blocks `75%` of incoming physical poise damage when the defender has a shield equipped in their left hand / `GetEquippedShield() != nullptr`)*
    - `fWeaponBlockedPhysicalMultiplier = 0.45` *(Blocks `55%` of incoming physical poise damage when blocking with a weapon without a shield)*
  - Keep backwards compatibility if legacy `fBlockedPhysicalMultiplier` is read, or replace it cleanly across `arpghud.ini`, `main.cpp`, `menu_framework.cpp`, `ResetPoiseDefaults`, and tests.

---

## 4. 2D Target Poise Bar (Minimalist HUD Testing & Gameplay Feature)

- **Setting & INI Key (`[HUD]`):** `bShowTargetPoiseBar = false` (`settings.showTargetPoiseBar`), exposed as a checkbox in the SKSE Menu Framework HUD tab (included in reset defaults and live-reloaded on **F10**).
- **Backend Tracking (`BuiltinPoise`):** Track the most recently hit (or crosshair-targeted) managed actor—storing its FormID, display name, tier label (`NPC`, `Light`, `Medium`, `Heavy`, `Boss`, `Unknown`), `current` poise, `maximum` poise, and active `immunity > 0.0F` state.
- **UI Rendering (`minimal.html` / `minimal.css` / `minimal.js`):** Display a clean, compact top-center bar formatted as `<Target Name> [<Tier>]  <current> / <maximum>` (1 decimal place) with a yellow fill bar, fading out smoothly after `4.0` seconds of inactivity or when `bShowTargetPoiseBar` is disabled.

---

## 5. Non-`ActorTypeNPC` Magic Caster Fix & Balanced Boss Spell Scaling

### A. Fix Incoming Magic Poise From Dragon Priests & Creatures
- **Finding:** Player-cast *Chain Lightning* properly drains and staggers a Dragon Priest (and properly deals `0` poise damage to 100%-shock-immune Storm Atronachs), **but** a Dragon Priest's outgoing lightning attacks (*Thunderbolt* / *Chain Lightning* / *Lightning Cloak*) deal **0 poise damage** to the player.
- **Fix:** Ensure incoming magic hit routing does **not** filter out non-`ActorTypeNPC` casters (Dragon Priests, Dragons, Hagravens, Wispmothers, Spriggans) and routes `Aimed` travel projectiles, `TargetLocation` / `TargetActor` AoE explosions, `kDualValueModifier` (Health + Magicka drain spells), and cloak/hazard spells into `ApplyMagicDamage`.

### B. Balanced NPC/Boss Magic Multiplier & Ward Rule
- Add `fNPCMagicPoiseMultiplier = 0.60` (`settings.npcMagicPoiseMultiplier`) under `[Poise]`, applied whenever `!caster->IsPlayerRef()`, so Dragon Priest bolts and Dragon breath threaten posture without stunlocking melee or archer builds across the room.
- Keep active Ward magic poise mitigation at its current partial reduction (`~75–80%`, **not** `100%`) until the extra magicka-drain-on-ward-hit mechanic is implemented in a future pass.

---

## 6. Configuration, UI & Testing Checklist
- Add all new `[Poise]` and `[HUD]` keys to `arpghud.ini`, `main.cpp` loader/exports, the **Combat Balance** and **HUD** menus, `ResetPoiseDefaults`, `README.md`, and `POISE_GUIDE.txt`.
- Extend `builtin-poise.test.cpp`, `creature-tier.test.cpp`, and `builtin-poise-settings.test.js` to cover all new multipliers, shield-vs-weapon block splitting, Ice Wolf name promotion, and `bShowTargetPoiseBar`.
- Build all three targets (`default`, `arpghud_stagger_diagnostic`, `arpghud_magic_diagnostic`) and verify all C++ and JS unit tests pass.

