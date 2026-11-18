# Magic and Spell Poise Damage

This report documents the current magic poise implementation and INI defaults.

## Eligibility and classification

The shared effect filter, `ElementalHealthEffect` in [`src/magic_gameplay.cpp`](src/magic_gameplay.cpp), accepts effects only when they:

- are hostile;
- affect Health as the primary actor value, or affect Health as the secondary value of a `kDualValueModifier`; and
- use Fire, Frost, or Shock resistance.

Other effects are not included in this direct elemental magic poise calculation. The effect's runtime `data->magnitude` is normally used as `Impact::magnitude`. A ward impact instead uses an elemental Health effect's magnitude from the spell record; if that is non-finite or non-positive, it uses `8.0F` and logs a warning.

The implementation distinguishes two flags:

- `Impact::concentration` selects the concentration magnitude multiplier and whether the hit is pooled.
- `Impact::channeled` is set for a spell with casting type `kConcentration`; it is passed to the math function's `concentrationSpell` gate, which honors `bConcentrationSpellsDealPoise`.

For projectile impacts, `MakeImpact` sets both flags from `spell->GetCastingType() == kConcentration`. For projectileless effects, `MakeProjectilelessImpact` sets `concentration` when `castingSource == kInstant && !areaTarget`, but sets `channeled` independently from the magic item's casting type. Thus the spell casting type and the projectileless instant/non-area routing test are related but not identical.

| Category | Classification and behavior |
|---|---|
| **Magic bursts / projectiles** | A qualifying effect is added during a projectile collision. For a projectile spell not classified as `kConcentration`, `Impact::concentration` is false: the burst multiplier and soft-cap curve are used, and poise damage is applied immediately. A projectile can affect each target only once per projectile handle. |
| **Continuous / channeled spells** | A projectile spell whose casting type is `kConcentration` sets both flags. The concentration multiplier is selected and damage is pooled. When `bConcentrationSpellsDealPoise=false`, `MagicPoise::Damage` returns zero for these channeled spells. If enabled, accumulated damage is emitted on the interval and capped per interval. |
| **Cloaks / projectileless repeated effects** | A qualifying projectileless effect is classified as concentration-like when its casting source is `kInstant` and it is not an area target. It uses the concentration multiplier and pooling. The `bConcentrationSpellsDealPoise` gate applies only if it is also a `kConcentration` casting-type spell. |
| **Hazards / explosions** | A qualifying projectileless effect that does not meet the instant, non-area test is handled as a burst and applied immediately. Projectileless non-concentration impacts below magnitude `8.0` are skipped. A spell/target pair is deduplicated for 1.5 seconds. |
| **Weapon enchantments** | The damage path does not separately classify effects by weapon or enchantment type. Qualifying enchantment effects can arrive through the projectileless effect hook and use the same burst-versus-instant/non-area rules. Projectileless burst impacts below magnitude `8.0` are skipped, suppressing minor enchantment proc poise hits. Hostile `kAbsorb` effects (including absorb enchantments) are handled separately to note Sapped and do not deal direct poise damage through this calculation. |

Key routing functions are `MakeImpact`, `MakeProjectilelessImpact`, `AddTarget`, `Collision`, and `Process`, all in [`src/magic_gameplay.cpp`](src/magic_gameplay.cpp). `RecentlyHit` handles projectileless spell/target deduplication.

## Raw formula and telemetry Base

`MagicPoise::DamageInput` has `magnitude`, `stagger`, `magnitudeMultiplier`, and `staggerMultiplier`. `Process` sets the magnitude from the impact, selects the multiplier, and sets the resistance and ward inputs:

```text
K = impact.concentration
    ? settings.concentrationPoiseMultiplier
    : settings.magicBurstPoiseMultiplier

burstMagnitudeDamage =
    magnitude <= 40
    ? magnitude * settings.magicBurstPoiseMultiplier
    : min(90, 40 * settings.magicBurstPoiseMultiplier
             + (magnitude - 40) * 0.55)

raw = (impact.concentration ? magnitude * K : burstMagnitudeDamage)
    + input.stagger * settings.magicStaggerPoiseMultiplier
```

`input.stagger` is not assigned by this path and has its default value `0.0F`. Therefore the current effective raw formula is:

```text
concentration: raw = impact.magnitude * fConcentrationPoiseMultiplier
burst, magnitude <= 40: raw = magnitude * fMagicBurstPoiseMultiplier
burst, magnitude > 40: raw = min(90, 40 * fMagicBurstPoiseMultiplier
                                    + (magnitude - 40) * 0.55)
```

With the default burst multiplier of `1.20`, magnitudes 25 and 40 yield raw poise 30 and 48 respectively; magnitude 50 yields 53.5, and the curve caps at 90. The value 48 at the curve start follows from `40 * fMagicBurstPoiseMultiplier`, so custom multiplier values preserve continuity. `fMagicStaggerPoiseMultiplier` is currently ineffectual for this path; the hook logs that its stagger contribution is unavailable. The raw formula is in `MagicPoise::Damage` in [`src/magic_poise_math.h`](src/magic_poise_math.h); input construction and telemetry setup are in `Process` in [`src/magic_gameplay.cpp`](src/magic_gameplay.cpp).

For an ordinary, non-pooled impact, the telemetry hit's `baseDamage`—serialized as `"base"`—is `MagicPoise::Explain(input).raw`, before resistance, ward, NPC-caster, or blocking multipliers. There is no floor or cap on this raw value in the formula. Inputs must be finite and nonnegative; gameplay skips non-finite or non-positive impact magnitudes. Ward fallback magnitude is the exception described above.

For concentration-pool emissions, telemetry's `baseDamage` is instead the emitted pool amount, after per-impact scaling and the interval cap. This is set in `Advance` in [`src/magic_gameplay.cpp`](src/magic_gameplay.cpp). Also note that the `Magic poise impact` info log prints `impact.magnitude` in its `base=` field; that log field is not the telemetry `"base"` value.

There is no base magicka cost, spell skill, or perk scaling in this magic poise calculation.

## Multipliers and operation order

`MagicPoise::Damage` in [`src/magic_poise_math.h`](src/magic_poise_math.h) computes:

```text
elemental = 1 - clamp(elementalResistance, -100, 85) / 100
magic     = 1 - clamp(magicResistance,     -100, 85) / 100
ward      = wardIntercepted ? wardMultiplier : 1

damage = raw * elemental * magic * ward
```

The elemental and Magic Resistance values come from the target's runtime actor values. Each resistance factor ranges from `2.0` at -100 resistance to `0.15` at 85 resistance. `Process` then multiplies by `npcMagicPoiseMultiplier` for any attacker that is not the player. `ApplyMagicDamage` next multiplies by the target's armor-class setting—Clothing/Unarmored by default, then Light, with Heavy taking priority—and finally by the applicable block setting. Shields do not set an armor class. The armor multiplier and class label are included in the telemetry hit breakdown.

For a non-pooled hit, `BuiltinPoise::ApplyMagicDamage` next applies block scaling:

```text
block = shield block ? shieldBlockedMagicMultiplier
      : weapon-only block ? weaponBlockedMagicMultiplier
      : 1

appliedDamage = damage * block
```

Ward-intercepted impacts use `BlockKind::none` in `ApplyMagicDamage`, so ward and shield/weapon block mitigation do not stack. Relevant functions are `Process` in [`src/magic_gameplay.cpp`](src/magic_gameplay.cpp), and `BlockedMagicScale` and `ApplyMagicDamage` in [`src/builtin_poise.cpp`](src/builtin_poise.cpp).

### Concentration pooling

Each concentration impact is resistance-, ward-, and (if applicable) NPC-scaled before being added to its target's `ConcentrationPool`. The pool sums pending damage up to `fConcentrationMaxPoisePerInterval`, waits `fConcentrationIntervalSeconds`, then emits up to that cap. Emission calls `ApplyMagicDamage` without ward mitigation, so active shield/weapon block scaling is then applied; the per-impact resistance, ward, and NPC scalars are not applied a second time. Pool timing and caps are implemented by `ConcentrationPool` in [`src/magic_poise_math.h`](src/magic_poise_math.h) and `Advance` in [`src/magic_gameplay.cpp`](src/magic_gameplay.cpp).

### Flesh spells are not a hit-damage multiplier

Active Flesh spells and Dragonhide affect the target's **maximum poise**, not incoming magic hit damage. `ActiveFlesh` in [`src/builtin_poise.cpp`](src/builtin_poise.cpp) gathers active Alteration armor effects and recognizes Dragonhide. `ComputeMaximumPoise` in [`src/builtin_poise_state.h`](src/builtin_poise_state.h) adds:

```text
fleshPoise =
    (activeFleshArmorMagnitude * fleshSpellPoiseMultiplier
     + (dragonhideActive ? dragonhidePoiseBonus : 0))
    * fleshEfficiency
```

`FleshEfficiency` uses the Heavy Armor value if Heavy armor is worn; otherwise it uses the Light Armor value if Light armor is worn, or `1.0` if neither is worn. The maximum-poise calculation has a floor of `10.0`; that floor is not a magic-damage floor. Flesh effects only contribute when the magic gameplay hooks are built in.

## Variables, INI keys, and current defaults

These are the relevant C++ settings and their defaults in [`src/builtin_poise_state.h`](src/builtin_poise_state.h), with current values from [`poisesystemhud.ini`](poisesystemhud.ini).

| C++ setting / input | INI key | Current default | Meaning |
|---|---|---:|---|
| `magicBurstPoiseMultiplier` | `fMagicBurstPoiseMultiplier` | `1.20` | Raw multiplier through magnitude 40; above 40 uses `min(90, 40 * multiplier + (magnitude - 40) * 0.55)` |
| `clothingMagicPoiseMultiplier` | `fClothingMagicPoiseMultiplier` | `0.70` | Incoming magic poise scalar for clothing/unarmored targets |
| `lightArmorMagicPoiseMultiplier` | `fLightArmorMagicPoiseMultiplier` | `1.00` | Incoming magic poise scalar for Light Armor |
| `heavyArmorMagicPoiseMultiplier` | `fHeavyArmorMagicPoiseMultiplier` | `1.15` | Incoming magic poise scalar for Heavy Armor |
| `concentrationPoiseMultiplier` | `fConcentrationPoiseMultiplier` | `1.0` | Raw magnitude multiplier when `Impact::concentration` is true |
| `concentrationSpellsDealPoise` | `bConcentrationSpellsDealPoise` | `false` | Allows channeled `kConcentration` spells through the direct-poise gate |
| `concentrationIntervalSeconds` | `fConcentrationIntervalSeconds` | `1.0` | Time interval for pooled damage emission |
| `concentrationMaxPoisePerInterval` | `fConcentrationMaxPoisePerInterval` | `15.0` | Maximum pooled damage emitted per interval |
| `magicStaggerPoiseMultiplier` | `fMagicStaggerPoiseMultiplier` | `10.0` | Multiplier for `input.stagger`; currently no effect because this path leaves it zero |
| `wardMagicDamageMultiplier` | `fWardMagicDamageMultiplier` | `0.35` | Applied when the impact is ward-intercepted |
| `npcMagicPoiseMultiplier` | `fNPCMagicPoiseMultiplier` | `0.6` | Applied for a non-player caster |
| `shieldBlockedMagicMultiplier` | `fShieldBlockedMagicMultiplier` | `0.4` | Applied when magic poise damage is shield-blocked |
| `weaponBlockedMagicMultiplier` | `fWeaponBlockedMagicMultiplier` | `0.7` | Applied when magic poise damage is blocked with a weapon |
| `fleshSpellPoiseMultiplier` | `fFleshSpellPoiseMultiplier` | `0.45` | Converts active Flesh armor magnitude into maximum poise |
| `dragonhidePoiseBonus` | `fDragonhidePoiseBonus` | `60.0` | Additional maximum poise while Dragonhide is active |
| `lightArmorFleshMultiplier` | `fLightArmorFleshMultiplier` | `0.75` | Flesh efficiency when Light armor is worn without Heavy armor |
| `heavyArmorFleshMultiplier` | `fHeavyArmorFleshMultiplier` | `0.45` | Flesh efficiency when Heavy armor is worn |
| `basePoise` | `fBasePoise` | `40.0` | Base of the maximum-poise pool, not per-hit magic Base |

Resistance values (`elementalResistance` and `magicResistance`) are runtime target actor values, not INI settings. The input field `magnitudeMultiplier`, `wardIntercepted`, `concentrationSpell`, and `concentrationAllowed` are set by `Process`; `stagger` remains at its `DamageInput` default of `0.0F`.
