# Physical and Melee Poise Damage: Calculation and Runtime Flow

This guide documents how the built-in system classifies a physical hit, computes poise damage, applies it to the target's poise pool, and turns a poise break into a stagger. The formulas and defaults below reflect the implementation and current `[Poise]` settings.

## 1. The core physical-hit formula

For an eligible hit, the calculation starts with a base amount plus weapon weight and, usually, a bonus derived from the hit's reported physical damage:

```text
base = fBaseHitPoiseDamage
     + clamp(weaponWeight, 0, 1000) * fWeaponWeightDamageMultiplier

physicalInput = isfinite(hit.physicalDamage) ? max(0, hit.physicalDamage) : 0
rawBonus = min(
    max(0, fPhysicalDamagePoiseCap),
    physicalInput * fPhysicalDamagePoiseMultiplier
)

startingDamage = base + rawBonus
```

Defaults are `6.0` base poise damage, `0.375` per weapon-weight unit, `0.05` physical-damage scaling, and a `6.0` cap on that physical-damage bonus. Non-finite or negative physical damage contributes zero. The bonus is calculated from `HitData::physicalDamage` as supplied by the game; this plugin does not reconstruct weapon damage, remove perk or sneak multipliers, or derive it from health lost. Consequently, any sneak/perk multipliers already present in that value contribute to the bonus, subject to the cap.

The cap applies to the **bonus**, not the whole hit. With defaults, the bonus reaches its cap at 120 physical damage (`120 * 0.05 = 6`). On eligible hits, that bonus is added before type, power-attack, Scorched, and block multipliers, so those later multipliers also scale the bonus.

One exception is a **shield-blocked** hit: its raw physical-damage bonus is set to zero. Weapon-only blocks and unblocked hits retain it. The weapon-weight and base terms still apply to shield blocks.

## 2. Multipliers by physical hit type

After adding the base and raw bonus, the hit gets the applicable type multiplier:

| Hit type | Type multiplier | How the type is determined |
|---|---:|---|
| One-handed melee | `1.0` | NPC attacker with a non-staff weapon; no extra one-handed multiplier |
| Two-handed melee | `fTwoHandedMultiplier` (`0.85`) | Weapon type is two-handed sword or two-handed axe |
| Shield bash | `fShieldBashMultiplier` (`1.0`) | Bash hit while the attacker has a shield equipped |
| Weapon/pommel bash | `fWeaponBashMultiplier` (`0.35`) | Bash hit without an equipped shield |
| Creature physical hit | `fCreatureMultiplier` (`0.8`) × creature-attacker tier multiplier | Non-NPC attacker classified as a creature |
| Projectile | `fProjectileMultiplier` (`0.8`) | Valid projectile with both ammo and weapon source |

Two-handed weapons therefore currently deal **15% less** poise damage than the same-weight, same-physical-damage one-handed weapon before later modifiers. Bows are routed as projectiles, not as melee hits. Staff hits are ignored by this physical-hit path.

Creature-attacker tier multipliers are applied in addition to `fCreatureMultiplier`:

| Attacker tier | Default tier damage multiplier |
|---|---:|
| Light | `fLightCreatureDamageMultiplier = 0.6` |
| Medium | `fMediumCreatureDamageMultiplier = 1.2` |
| Heavy | `fHeavyCreatureDamageMultiplier = 1.375` |
| Boss | `fBossCreatureDamageMultiplier = 2.25` |
| Standard/unknown | `1.0` |

Creature hits do not use the race's unarmed weapon as a weapon-weight input: the runtime deliberately clears that weapon when it classifies a non-NPC attacker as a creature. Their base is normally therefore just `fBaseHitPoiseDamage`, plus any eligible raw physical-damage bonus.

### Remaining multiplier order

The following steps happen in this order after the type multiplier:

1. If the hit is flagged as a power attack, multiply by `fPowerAttackMultiplier` (`1.5`).
2. For non-projectile physical hits, multiply by the Scorched melee damage multiplier when that debuff is active (`fScorchedMeleeDamageMultiplier = 1.20` by default). This covers melee, bash, and creature physical hits; projectiles are explicitly excluded.
3. Always multiply by `fUnblockedPhysicalMultiplier` (`2.5`). This is the common physical-damage scale, including for blocked hits.
4. If blocked, multiply by the block share:
   - Shield block: `fShieldBlockedPhysicalMultiplier = 0.25`.
   - Weapon-only block: `fWeaponBlockedPhysicalMultiplier = 0.45`.

Thus, with `T` denoting the applicable type multiplier, `P` the power multiplier (1.5 for a power attack, otherwise 1), `S` the Scorched multiplier (active value for non-projectiles, otherwise 1), and `B` the block share (0.25/0.45 when blocked, otherwise 1), the default physical calculation is:

```text
poiseDamage = (base + rawBonus) * T * P * S * fUnblockedPhysicalMultiplier * B
```

For a shield block, use `rawBonus = 0` in that expression. For an ordinary hit, `B = 1`. For a projectile, `S = 1` regardless of Scorched.

## 3. Worked examples with current defaults

Assume a one-handed weapon of weight 10 and a reported physical hit damage of 60:

```text
base       = 6 + 10 * 0.375 = 9.75
rawBonus   = min(6, 60 * 0.05) = 3
open hit   = (9.75 + 3) * 2.5 = 31.875 poise
power hit  = 31.875 * 1.5 = 47.8125 poise
```

For the same hit with a two-handed sword/axe:

```text
two-handed open hit = 12.75 * 0.85 * 2.5 = 27.09375 poise
```

For that same one-handed hit against a target blocking with a weapon:

```text
weapon-blocked = 12.75 * 2.5 * 0.45 = 14.34375 poise
```

For a shield block, the raw bonus is removed:

```text
shield-blocked = 9.75 * 2.5 * 0.25 = 6.09375 poise
```

These examples assume no Scorched effect. When active, multiply the non-projectile results by its configured multiplier as an additional step.

## 4. How the runtime identifies and accepts a hit

The hook classifies a hit before applying poise:

- It ignores missing hit data, predicted-damage hits, and explosions.
- A non-projectile source object is not processed through this path. A projectile is accepted only when its runtime data has both an ammo source and a weapon source.
- A creature attacker is classified by its NPC keyword, rather than by the race's unarmed weapon that may appear in hit data.
- For NPC attackers, a bash flag takes precedence over ordinary weapon classification. A shield-equipped bash uses the shield-bash multiplier; otherwise it is a weapon bash.
- For non-bash NPC hits, a staff is ignored. A weapon is treated as melee. A hit without a weapon is treated as a creature hit only when its skill is `None`; otherwise it is not classified here.
- The target's blocked flag is taken from the hit data. If blocked, the target's equipped items determine the kind: a shield means shield block; otherwise a held non-staff weapon means weapon-only block.
- Self-hits are not applied. The target must also be eligible for poise tracking: for example, dead, bleeding-out, kill-move, unloaded, excluded, or (for creatures) flying actors are not tracked.

For an accepted player or tracked NPC/creature target, the same physical formula is applied to that target's poise state. Creature tier here has two distinct roles: the **attacker's** tier scales creature-hit damage; the **target's** tier sets its maximum poise and its break/recovery behavior.

The built-in system must be active (`bEnableBuiltInPoise`). Player poise is controlled by that system; standard NPC poise additionally requires `bEnableNPCPoise`, and creature poise requires `bEnableCreaturePoise`. These switches are separate from classifying the incoming attacker.

## 5. The target's maximum poise

Poise damage is subtracted from the target's current poise, so the pool size determines how many hits are needed to break. The maximum is calculated separately from hit damage:

```text
targetBase = fBasePoise * targetTierPoiseMultiplier
lightArmorBonus = min(fLightArmorRatingPoiseCap,
                      clamp(summedLightArmorBaseRating, 0, 2000)
                      * fLightArmorRatingPoiseMultiplier)
fleshBonus = (activeFleshArmor * fFleshSpellPoiseMultiplier
              + (Dragonhide active ? fDragonhidePoiseBonus : 0))
             * fleshEfficiency

maximumPoise = max(10,
    (targetBase
     + wornNonShieldArmorWeight * fArmorWeightMultiplier
     + shieldWeight * shieldWeightMultiplier
     + lightArmorBonus
     + fleshBonus)
    * summonScale
)
```

Relevant defaults and rules:

- `fBasePoise = 40`. Target-tier multipliers scale this base term only: Light `0.75`, Medium `1.7`, Heavy `3.75`, Boss `6.0`; standard NPCs and unlisted/unknown creatures use `1.0`.
- Worn armor weight is additive at `fArmorWeightMultiplier = 1.0`. Shield weight is tracked separately. For the player it uses `fShieldWeightPoiseMultiplier = 0.0`; for NPCs it uses `fArmorWeightMultiplier` by default (`bApplyShieldWeightToNPCs = true`).
- Only worn Light Armor contributes to the Light Armor rating bonus. Its default rate is `0.15` per rating point and its bonus caps at `15`. Shields are excluded from armor class and Light Armor rating; Light Armor rating is read from the armor's base rating, with template rating fallback.
- Flesh efficiency is `1.0` with no Light/Heavy armor, `0.75` with Light Armor, and `0.45` with Heavy Armor. Heavy takes priority if both classes are present. Shields do not affect that choice.
- A summon/commanded actor can have its pool scaled by `fSummonPoiseMultiplier = 1.5`.
- The final pool has a hard floor of 10.

When a maximum changes, current poise is preserved as the same proportion of the old maximum (clamped to 0–100%) and mapped to the new maximum.

## 6. Applying damage, breaks, stagger, and recovery

- If the target has no initialized poise pool or built-in poise is disabled, the poise state does not handle the hit.
- During stagger immunity, an otherwise classified hit is marked handled but removes no poise. The hit's vanilla stagger value is cleared for handled hits.
- Otherwise, finite positive poise damage is subtracted, floored at zero. A hit that leaves poise above zero does not stagger.
- When poise reaches zero, the state immediately refills to maximum, starts stagger immunity, and reports a stagger magnitude:
  `clamp(hitPoiseDamage / maximumPoise, minimumBreakMagnitude, 1.0)`.
  The minimum is `0.25` for ordinary targets and non-boss creature tiers. Boss-tier targets use `1.0`, so a boss poise break always requests a full-strength stagger.
- Immunity lasts `fStaggerImmunitySeconds = 1.0` for standard targets and `fCreatureStaggerImmunitySeconds = 2.0` for creature tiers. Boss poise regenerates at `fBossCreatureRegenMultiplier = 1.5` times the normal rate.
- A positive applied hit starts a `fRegenDelaySeconds = 3.0` delay, plus any Numbed delay bonus. After the delay, default recovery is `fRegenRatePerSecond = 8.0` per second, modified by relevant debuffs and tier. Recovery stops at the current recovery ceiling and never reduces poise already above that ceiling.
- The hit hook calls the original game hit routine. For a handled hit, it clears the vanilla hit stagger; if the poise state broke, it then issues the system's directional stagger using the calculated break magnitude. This makes poise breaks, rather than each ordinary managed hit, drive the system's physical-hit staggers.
- The `[Poise]` values are read from the INI, validated/clamped to their configured ranges, and can be reloaded live with F10. The INI's default values are the ones used in the tables and examples above.

Debuffs do not all change hit damage: Scorched multiplies non-projectile physical poise damage. Sapped, Enfeebled, Numbed, and Hemorrhaging affect recovery or delay rather than adding direct physical-hit poise damage.

## 7. HUD benchmark versus a real hit

The equipped-weapon telemetry benchmark is an estimate, not the hit calculation. It uses weapon base damage as the input to the capped physical bonus, then applies the one-/two-handed or ranged factor and the unblocked multiplier; its power estimate adds the power-attack multiplier. It cannot know the actual hit's damage, sneak/perk outcomes, target block, or active Scorched state. A real hit uses `HitData::physicalDamage` and the target's actual classification and block status.

## 8. Source of truth

- [Hit classification and runtime application](./src/builtin_poise.cpp) (`ClassifyHit`, `ProcessHit`)
- [Physical formulas, defaults, poise pool, and state transitions](./src/builtin_poise_state.h) (`PhysicalDamageBonus`, `ComputeMaximumPoise`, `State::Apply`, `State::ApplyDamage`)
- [Current INI defaults and setting descriptions](./poisesystemhud.ini) (`[Poise]`, `[Poise Debuffs]`)
- [INI setting loading and accepted ranges](./src/main.cpp)
- [Formula and edge-case tests](./tests/builtin-poise.test.cpp)
