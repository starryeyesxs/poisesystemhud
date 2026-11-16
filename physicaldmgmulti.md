# Design Specification: Core Baseline Progression (Damage Scaling, Sneak Attacks, Light Armor Tiering & Flesh Efficiency)

## Goal
Establish the permanent offensive and defensive posture baseline before implementing and testing Poise Status Debuffs:
1. **Physical Damage & Sneak Attack Poise Scaling:** Add a capped fraction of incoming physical hit damage (including sneak-attack multipliers) into `ProcessHit` so weapon tiers, stealth attacks, creature sizes, and heavy boss slams naturally deal more posture damage.
2. **Light Armor Base Rating Scaling:** Add a capped bonus from raw base Light Armor rating into `MaximumPoise` so endgame Light Armor (Glass/Dragonscale) progresses meaningfully over early Leather without overtaking Heavy Armor.
3. **Armor-Tiered Flesh Spell Efficiency:** Scale Alteration Flesh spells and Dragonhide poise bonuses by equipped armor class (`1.00x` Cloth, `0.75x` Light, `0.45x` Heavy) to prevent Heavy Armor + Flesh poise bloat.

---

## 1. Offensive Baseline: Physical Damage & Sneak Attack Poise Scaling (`ProcessHit`)

### Design Rationale
- **Include Sneak Attack Damage Naturally:** Read `hit.physicalDamage` directly from `HitData` **with sneak-attack multipliers included** rather than stripping them out.
- **Rewarding Stealth Investment:** Early- and mid-game stealth attacks (`2x`–`6x` multipliers on bows or daggers) gain meaningful extra posture damage (`+1.5` to `+3.0` base poise before multipliers), creating a fair stagger opening when an ambush fails to kill outright.
- **Bounded by Hard Cap:** High-investment endgame sneak attacks (`15x`–`30x` dagger perks) already one-shot standard targets; against surviving bosses or Dragon Priests, `fPhysicalDamagePoiseCap` (`6.0`) cleanly clamps the bonus so stealth pokes cannot instant-break boss posture pools.
- **Perk Overhaul Compatibility:** Consuming `HitData` physical damage as-is avoids fragile perk-stripping workarounds and works seamlessly with perk overhauls (e.g., Vokrii, Ordinator).

### Formula Integration
In `ProcessHit`, read the incoming physical hit damage (`hit.physicalDamage`, clamped `>= 0.0F`) and add a capped bonus to the initial `damage` term **before** weapon-type, power-attack, or block/unblocked multipliers are applied:

```cpp
float rawDamageBonus = std::min(
    settings.physicalDamagePoiseCap,
    std::max(0.0F, hit.physicalDamage) * settings.physicalDamagePoiseMultiplier
);

float damage = settings.baseHitDamage
             + (weight * settings.weaponWeightDamageMultiplier)
             + rawDamageBonus;
```

### New `[Poise]` Settings & INI Keys
- `settings.physicalDamagePoiseMultiplier` ← `fPhysicalDamagePoiseMultiplier = 0.05` *(Range: `0.00` – `0.25`; adds 5% of physical hit damage—including sneak multipliers—to base poise damage before multipliers)*
- `settings.physicalDamagePoiseCap` ← `fPhysicalDamagePoiseCap = 6.0` *(Range: `0.0` – `25.0`; hard cap preventing tempered weapons or high-tier sneak multipliers from breaking posture balance)*

### Telemetry & Debugging Note
- Expose `RawDmg +X.X` (and `[CAP]` tag when `rawDamageBonus >= settings.physicalDamagePoiseCap`) in the Prisma UI **Live Poise Telemetry** hit breakdown so sneak-attack and weapon-tier contributions can be verified live in-game.

---

## 2. Defensive Baseline: Light Armor Tier Scaling (`MaximumPoise`)

### Formula Integration
During the equipped armor scan in `MaximumPoise`:
- Continue summing `weight` across all worn Light Armor, Heavy Armor, and Shields.
- For worn pieces where `armor->IsLightArmor()` is `true`, sum raw base `armor->GetArmorRating()` (clamped per item to `[0.0F, 500.0F]`, total capped at `2000.0F`) into `lightArmorRating`.
- Do **not** read the actor's modified `DamageResist` ActorValue (prevents smithing tempering loops from inflating poise) and do **not** apply to Heavy Armor.

```cpp
float lightTierBonus = std::min(
    settings.lightArmorRatingPoiseCap,
    lightArmorRating * settings.lightArmorRatingPoiseMultiplier
);
```

### New `[Poise]` Settings & INI Keys
- `settings.lightArmorRatingPoiseMultiplier` ← `fLightArmorRatingPoiseMultiplier = 0.15` *(Range: `0.00` – `0.50`)*
- `settings.lightArmorRatingPoiseCap` ← `fLightArmorRatingPoiseCap = 15.0` *(Range: `0.0` – `50.0`)*

---

## 3. Defensive Baseline: Armor-Tiered Flesh Spell Efficiency (`MaximumPoise`)

### Formula Integration
Determine `fleshEfficiency` using equipped armor class priority:
1. If any equipped armor piece has `IsHeavyArmor() == true` → `fleshEfficiency = settings.heavyArmorFleshMultiplier` (`0.45F`)
2. Else if any equipped armor piece has `IsLightArmor() == true` → `fleshEfficiency = settings.lightArmorFleshMultiplier` (`0.75F`)
3. Else (Unarmored / Cloth) → `fleshEfficiency = 1.00F`

```cpp
float fleshPoise = (
    flesh.armor * settings.fleshSpellPoiseMultiplier +
    (flesh.dragonhide ? settings.dragonhidePoiseBonus : 0.0F)
) * fleshEfficiency;

float maxPoise = settings.basePoise
               + (weight * settings.armorWeightMultiplier)
               + lightTierBonus
               + fleshPoise;
```

### New `[Poise]` Settings & INI Keys
- `settings.lightArmorFleshMultiplier` ← `fLightArmorFleshMultiplier = 0.75` *(Range: `0.00` – `1.00`)*
- `settings.heavyArmorFleshMultiplier` ← `fHeavyArmorFleshMultiplier = 0.45` *(Range: `0.00` – `1.00`; set to `0.0` via INI + F10 to disable Flesh poise on Heavy Armor completely)*

---

## 4. Configuration, Menu & Testing Checklist
- [ ] Add all 6 new `[Poise]` keys to `arpghud.ini`, `main.cpp` loader/exports, the **Combat Balance** menu, and **Poise Reset** defaults, with live **F10** reload support.
- [ ] Include `RawDmg +X.X` (with cap indicator) and `Flesh +X.X` in the Prisma UI **Live Poise Telemetry** readout.
- [ ] Extend `builtin-poise.test.cpp` and `builtin-poise-settings.test.js` to cover capped physical/sneak damage scaling, Light Armor raw rating progression, and Cloth/Light/Heavy Flesh efficiency priority.
- [ ] Compile all targets and verify all unit tests pass.
