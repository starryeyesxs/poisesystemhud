# Current Melee and Physical Poise Damage Reference

This document describes the **current implemented** built-in physical-poise system. It covers melee, bash, creature, projectile, blocking, physical-damage scaling, poise pools, stagger breaks, recovery, and physical poise status debuffs.

Primary implementation sources:

- [`builtin_poise_state.h`](src/builtin_poise_state.h)
- [`builtin_poise.cpp`](src/builtin_poise.cpp)
- [`main.cpp`](src/main.cpp)
- [`poisesystemhud.ini`](poisesystemhud.ini)

## 1. Master Rules

Physical poise is disabled for an actor or hit if:

- `bEnableBuiltInPoise=false`
- The target has no valid positive poise maximum
- A projectile or hit is excluded from classification
- Stagger immunity is currently active

Additional tracking rules:

- Regular NPC poise is controlled by `bEnableNPCPoise`.
- Creature-tier poise is controlled by `bEnableCreaturePoise`.
- `bSuppressVanillaStaggers=true` allows the poise system to suppress otherwise handled vanilla staggers.
- `bAllowTimedBashStaggers=false` disables vanilla timed bash stagger behavior.
- Player eligibility also requires the player to be alive, not bleeding out, not in a kill move, and 3D-loaded.
- Ambient/excluded creatures are not tracked.

Defaults:

| Setting | INI key | Default |
|---|---|---:|
| Built-in poise enabled | `bEnableBuiltInPoise` | `true` |
| NPC poise enabled | `bEnableNPCPoise` | `true` |
| Creature poise enabled | `bEnableCreaturePoise` | `true` |
| Suppress vanilla staggers | `bSuppressVanillaStaggers` | `true` |
| Allow timed bash staggers | `bAllowTimedBashStaggers` | `false` |

## 2. Maximum Poise Formula

```text
maximumPoise =
    max(
        10,
        (
            basePoiseScaledByCreatureTier
            + wornArmorWeight * armorWeightMultiplier
            + wornShieldWeight * shieldPoiseMultiplier
            + lightArmorRatingBonus
            + fleshPoise
        ) * summonScale
    )
```

The complete clamped/priority rules are:

```text
armorWeight        = clamp(sum(worn Light Armor + Heavy Armor weights), 0, 10000)
shieldWeight       = clamp(sum(worn shield weights), 0, 10000)
lightArmorRating   = clamp(sum(base GetArmorRating() of worn Light Armor),
                           0, 2000)
```

Each light armor piece has its base armor rating clamped to `500` before addition. If a piece has no usable rating, the code falls back to `templateArmor->GetArmorRating()` when available. Tempering/modifier resistance values are not used.

For the shield weight term:

- Player: `fShieldWeightPoiseMultiplier`
- NPC, when `bApplyShieldWeightToNPCs=true`: `fArmorWeightMultiplier`
- NPC, when `bApplyShieldWeightToNPCs=false`: `fShieldWeightPoiseMultiplier`

Default values:

| Component | INI key | Default |
|---|---|---:|
| Base poise | `fBasePoise` | `40.0` |
| Armor weight contribution | `fArmorWeightMultiplier` | `1.0` per armor weight |
| Player shield weight contribution | `fShieldWeightPoiseMultiplier` | `0.0` |
| Apply normal armor weight to NPC shields | `bApplyShieldWeightToNPCs` | `true` |

### Creature-tier base pool scaling

Creature-tier scaling applies only to the base poise pool. Armor and spell bonuses remain additive.

```text
TierBasePoise = fBasePoise * tierPoiseMultiplier
```

| Creature tier | Poise pool multiplier |
|---|---:|
| Standard | `1.0` |
| Unknown | `1.0` |
| Light | `0.75` |
| Medium | `1.7` |
| Heavy | `3.75` |
| Boss | `6.0` |
| Excluded | Not tracked |

Summons and commanded actors multiply the whole resulting maximum by `fSummonPoiseMultiplier`, default `1.5`.

## 3. Physical-Damage Bonus

The incoming `HitData.physicalDamage` is used directly. Its sneak-attack, perk, and weapon multipliers do not need to be removed manually; those values are already included in that value.

```text
rawBonus =
    min(
        fPhysicalDamagePoiseCap,
        max(0, HitData.physicalDamage) * fPhysicalDamagePoiseMultiplier
    )
```

Defaults:

| Component | INI key | Default |
|---|---|---:|
| Physical-damage multiplier | `fPhysicalDamagePoiseMultiplier` | `0.05` |
| Physical-damage bonus cap | `fPhysicalDamagePoiseCap` | `6.0` |

Guardrails:

- Negative or non-finite `physicalDamage` is clamped to zero.
- The configured cap itself is clamped to at least zero.
- A result is reported as capped when the scaled result is greater than or equal to the cap and positive.
- Shield-blocked hits do not receive this physical-damage bonus.
- Weapon-blocked and unblocked hits do receive it.

## 4. Base Melee/Physical Hit Formula

The formula is built in this order:

```text
damage =
    fBaseHitPoiseDamage
    + weaponWeight * fWeaponWeightDamageMultiplier
    + rawPhysicalDamageBonus
```

After that, hit-type and status multipliers are applied.

### Weapon weight

```text
weaponWeight = clamp(hit.weaponWeight, 0, 1000)
```

For bash attacks:

- A shield bash uses the equipped shield weight.
- A weapon pommel bash uses no weight unless a shield is found.
- An otherwise missing bash weight falls back to the equipped right-hand weapon weight.

Defaults:

| Component | INI key | Default |
|---|---|---:|
| Humanoid/weapon base hit damage | `fBaseHitPoiseDamage` | `6.0` |
| Creature base hit damage | `fCreatureBaseHitPoiseDamage` | `5.5` |
| Weapon weight multiplier | `fWeaponWeightDamageMultiplier` | `0.35` |

## 5. Hit-Type Multipliers

### Melee

```text
if twoHanded:
    damage *= fTwoHandedMultiplier
```

Default: `1.15`.

Two-handed sword and two-handed axe weapons use this path. A bow or crossbow classifies as a projectile instead.

### Bash

```text
if shield equipped for bash:
    damage *= fShieldBashMultiplier
else:
    damage *= fWeaponBashMultiplier
```

Defaults:

| Bash kind | INI key | Default |
|---|---|---:|
| Shield bash | `fShieldBashMultiplier` | `1.0` |
| Weapon pommel bash | `fWeaponBashMultiplier` | `0.35` |

### Creature attack

```text
damage = (fCreatureBaseHitPoiseDamage + rawPhysicalDamageBonus)
       * creatureDamageTierMultiplier
```

Creature physical hits use the creature attacker path unless the hit is a valid projectile.

| Creature tier | Creature damage multiplier |
|---|---:|
| Light | `0.75` |
| Medium | `1.15` |
| Heavy | `1.65` |
| Boss | `2.40` |
| Standard / Unknown | `1.0` |

Default `fCreatureBaseHitPoiseDamage` is `5.5`. Creature hits do not use weapon weight.

### Projectile

```text
damage *= fProjectileMultiplier
```

Default: `0.65`.

The projectile path only applies when the hit source is a projectile with a valid ammo source and weapon source. Staff hits are excluded.

### Power attack

```text
if HitData kPowerAttack:
    damage *= fPowerAttackMultiplier
```

Default: `1.5`.

### Scorched

Scorched is applied to every non-projectile physical hit:

```text
damage *= fScorchedMeleeDamageMultiplier
```

This includes melee, bash, and creature attacks.

Default: `1.20`. Projectiles are unaffected.

## 6. Block Multipliers

Blocking is the final physical scaling stage:

```text
if the hit is blocked:
    damage *= block share
```

Defaults:

| Component | INI key | Default |
|---|---|---:|
| Shield block share | `fShieldBlockedPhysicalMultiplier` | `0.25` |
| Weapon block share | `fWeaponBlockedPhysicalMultiplier` | `0.45` |

Therefore:

```text
Unblocked damage = fullDamage
Shield-blocked damage = fullDamage * 0.25
Weapon-blocked damage = fullDamage * 0.45
```

### Complete shield/weapon block formula

```text
finalDamage =
    (
        fBaseHitPoiseDamage
        + weaponWeight * fWeaponWeightDamageMultiplier
        + rawPhysicalDamageBonus
    )
    * hitTypeMultiplier
    * powerAttackMultiplier
    * scorchedMultiplier
    * blockMultiplier
```

Rules:

- `hitTypeMultiplier` is `1.15` for two-handed melee, `1.0/0.35` for shield/weapon bash, the creature tier formula for creature hits, or `0.65` for projectile.
- `powerAttackMultiplier` is omitted unless the hit is marked as a power attack.
- Scorched is omitted for projectiles.
- Shield block takes priority in the target block scan.
- Shield-blocked hits skip `rawPhysicalDamageBonus`; weapon-blocked hits retain it.

## 7. Worked Default Examples

Assume no Scorched or power attack.

### 10-weight one-handed melee, unblocked

```text
base = 6.0 + 10 * 0.35 = 9.5
final = 9.5
```

### 10-weight, two-handed, power attack

```text
base = 6.0 + 10 * 0.35 = 9.5
final = 9.5 * 1.15 * 1.5 = 16.3875
```

### 20-weight, two-handed, power attack, weapon block

```text
base = 6.0 + 20 * 0.35 = 13.0
final = 13.0 * 1.15 * 1.5 * 0.45 = 10.09125
```

### Shield bash

```text
final = (base + weight + rawBonus) * 1.0
```

### Weapon pommel bash

```text
final = (base + weight + rawBonus) * 0.35
```

## 8. Light Armor Poise Bonus

Every worn light armor piece contributes raw base armor rating:

```text
perPieceRating = clamp(GetArmorRating, 0, 500)
lightArmorRating = clamp(sum(perPieceRating), 0, 2000)

lightArmorBonus =
    min(fLightArmorRatingPoiseCap,
        lightArmorRating * fLightArmorRatingPoiseMultiplier)
```

Defaults:

| Component | INI key | Default |
|---|---|---:|
| Rating multiplier | `fLightArmorRatingPoiseMultiplier` | `0.15` |
| Rating bonus cap | `fLightArmorRatingPoiseCap` | `15.0` |

Shields never contribute light armor rating and never set light/heavy armor flags.

## 9. Flesh and Dragonhide Poise

Flesh poise is separate from armor weight and light armor rating:

```text
fleshPoise =
    (
        sumActiveFleshMagnitudes * fFleshSpellPoiseMultiplier
        + dragonhideBonus
    ) * armorFleshEfficiency
```

Armor efficiency priority:

```text
if any equipped Heavy Armor:
    efficiency = fHeavyArmorFleshMultiplier
else if any equipped Light Armor:
    efficiency = fLightArmorFleshMultiplier
else:
    efficiency = 1.0
```

Defaults:

| Component | INI key | Default |
|---|---|---:|
| Alteration Flesh spell multiplier | `fFleshSpellPoiseMultiplier` | `0.45` |
| Dragonhide bonus | `fDragonhidePoiseBonus` | `60.0` |
| Light armor flesh efficiency | `fLightArmorFleshMultiplier` | `0.75` |
| Heavy armor flesh efficiency | `fHeavyArmorFleshMultiplier` | `0.45` |

Heavy armor takes priority over light armor.

Active Flesh is detected only when the Alteration effect has:

- associated skill `Alteration`
- primary actor value `DamageResist`
- archetype `ValueModifier` or `PeakValueModifier`
- positive finite magnitude

Dragonhide is detected by the implementation's specific spell/effect form pair:

- Spell form `0xCDB70`
- Effect form `0xCDB75`

## 10. Poise Damage Application

When a valid hit reaches `Apply`:

```text
if poise maximum <= 0 or built-in poise disabled:
    no hit

if stagger immunity > 0:
    hit is absorbed; no poise removed

if finalDamage is not finite or <= 0:
    no poise removed

currentPoise = max(0, currentPoise - finalDamage)
```

If poise stays above zero, the hit does not break posture.

### Poise break

The hit breaks when `currentPoise <= 0`.

Player behavior:

```text
currentPoise = 0
recoveringFromBreak = true
regenDelay = NumbedDelayBonus
immunity = fStaggerImmunitySeconds
```

NPC behavior:

```text
currentPoise = maximumPoise
recoveringFromBreak = false
regenDelay = fRegenDelaySeconds + NumbedDelayBonus
immunity = creatureImmunity ? fCreatureStaggerImmunitySeconds : fStaggerImmunitySeconds
```

The break magnitude returned to the stagger gate is:

```text
clamp(finalDamage / maximumPoise,
       minimumBreakMagnitude,
       1.0)
```

Minimum break magnitude:

- Boss: `1.0`
- Every other nonboss target: `0.25`

Stagger immunity defaults:

| Actor kind | INI key | Default |
|---|---|---:|
| Player / regular NPC | `fStaggerImmunitySeconds` | `1.0` |
| Creature | `fCreatureStaggerImmunitySeconds` | `2.0` |

Creature immunity also applies to Boss, Heavy, Medium, Light, and other recognized creature tiers; standard NPCs do not use it.

## 11. Recovery and Regen Delay

When a hit is applied, the regen delay is reset to:

```text
regenDelay = fRegenDelaySeconds + numbedDelayBonus
```

Default `fRegenDelaySeconds` is `3.0`.

For recovery each frame/update interval:

```text
immunity = max(0, immunity - delta)
effectiveDelta = max(0, delta - oldImmunity)

regenDelay = max(0, regenDelay - effectiveDelta)
regenTime = max(0, effectiveDelta - oldRegenDelay)
```

Base recovery rate:

```text
regenRate =
    fx.sapped
    ? 0
    : fRegenRatePerSecond
      * creatureBossRegenMultiplier
      * hemorrhagingRegenRateMultiplier
```

Default `fRegenRatePerSecond` is `8.0`.

Boss creatures have `fBossCreatureRegenMultiplier=1.5`; other creatures use `1.0`.

Recovery is capped by:

```text
ceiling = maximumPoise * effectivePoiseRegenCeilingRatio
```

The ceiling never lowers poise already above it:

```text
if currentPoise < ceiling:
    currentPoise = min(ceiling, currentPoise + recoveredPoise)
```

## 12. Post-Break Surge Recovery

Players that broke posture enter `recoveringFromBreak`.

The surge zone is the lower half of maximum poise:

```text
surgeLimit = maximumPoise * 0.5
surgeRate = normalRegenRate * 1.65
```

Rules:

- The surge applies only while current poise is below `50%` maximum.
- Surge recovery uses the 1.65x multiplier until the surge ceiling or 50% limit is reached.
- Surge recovery cannot cross a lower active Enfeebled ceiling.
- Once current poise reaches the surge limit, normal recovery resumes.

If no debuff changes the normal ceiling, a broken target recovers to the full maximum.

## 13. Physical Poise Status Debuffs

The master toggle is:

```text
bEnablePoiseDebuffs=true
```

When false:

- No hostile debuff active-effect scan occurs.
- All debuff effects become neutral.
- Poise debuff UI/icons are hidden.

Each debuff category is boolean/nonstacking. Cross-type debuffs may coexist.

### Sapped

Trigger:

- A hostile active effect with archetype `Absorb`
- For a player, an absorb hit can hold a temporary pulse for `2.0` seconds even if the lasting effect disappears

Effect:

```text
regenRate = 0
```

INI:

| Key | Default |
|---|---:|
| `bEnableSappedDebuff` | `true` |

Self-cast absorb effects do not trigger Sapped.

### Enfeebled

Trigger:

- Hostile poison-resist active effect
- Active effect with the `MagicAlchHarmful` keyword

Effect:

```text
ceilingRatio =
    max(
        fMinGlobalRegenCeiling,
        highCeiling
        - (highCeiling - lowCeiling)
          * clamp(poisonMagnitude / fEnfeebledMagnitudeForMin, 0, 1)
    )
```

Defaults:

| Key | Default |
|---|---:|
| `bEnableEnfeebledDebuff` | `true` |
| `fEnfeebledRegenCeilingMin` | `0.70` |
| `fEnfeebledRegenCeilingMax` | `0.85` |
| `fEnfeebledMagnitudeForMin` | `10.0` |
| `fMinGlobalRegenCeiling` | `0.60` |

The loader swaps Min and Max if configured in the wrong order. The strongest poison magnitude wins; poison debuffs do not add.

If current poise is below the ceiling, it recovers toward it. If current poise is already above the ceiling, it is not reduced.

### Numbed

Trigger:

- Hostile lingering Frost effect
- Resist variable is `ResistFrost`
- Effect primary/health damage flag is set
- Duration is greater than zero

Effect:

```text
regenDelay =
    fRegenDelaySeconds
    + fNumbedRegenDelayBonus
```

Default:

| Key | Default |
|---|---:|
| `bEnableNumbedDebuff` | `true` |
| `fNumbedRegenDelayBonus` | `1.0` |

### Hemorrhaging

Trigger:

- Hostile bleed effect
- Editor ID, effect name, or spell name contains a configured case-insensitive keyword
- Vanilla Skyrim `PerkBleedingDamage` form is recognized when present

Effect:

```text
regenRate *= fHemorrhagingRegenRateMultiplier
```

Defaults:

| Key | Default |
|---|---:|
| `bEnableHemorrhagingDebuff` | `true` |
| `fHemorrhagingRegenRateMultiplier` | `0.65` |
| `sHemorrhagingEffectKeywords` | `bleed,hemorrhag` |

Vanilla bleed handling has an explicit `Skyrim.esm` form exception for `PerkBleedingDamage` local form `0x000C367A`. It can count even when the victim is its own caster, but only as Hemorrhaging.

### Scorched

Trigger:

- Hostile lingering Fire effect
- Resist variable is `ResistFire`
- Effect primary/health damage flag is set
- Duration is greater than zero

Effect:

```text
incomingPhysicalPoiseDamage *= fScorchedMeleeDamageMultiplier
```

Defaults:

| Key | Default |
|---|---:|
| `bEnableScorchedDebuff` | `true` |
| `fScorchedMeleeDamageMultiplier` | `1.20` |

Scorched applies to melee, bash, and creature hits, but not projectile hits.

## 14. Hit Classification Rules

A hit is ignored before poise application if:

- It has `kPredictDamage`
- It is an explosion
- It has no valid source and is not an NPC/creature fallback
- A valid projectile lacks an ammo source or weapon source
- The weapon is a staff
- The target or attacker relationship is invalid

Classification priority:

1. Projectile source
2. Creature/non-NPC attacker
3. Bash flag
4. Equippable weapon melee
5. Creature fallback only if skill is `kNone`

For blocked hits:

- A hit flagged blocked is checked from left and right equipped objects.
- A shield wins and is marked as shield-blocked.
- A held non-staff weapon marks the hit as weapon-blocked.

## 15. Armor and Light-Rating Scanning

The worn-inventory scan:

- Includes worn armor only.
- Clamps positive armor weight to `1000` per piece.
- Clamps total armor weight to `10000`.
- Clamps positive shield weight to `1000` per piece.
- Clamps total shield weight to `10000`.
- Heavy armor sets the Heavy flag.
- Light armor sets the Light flag and contributes base armor rating.
- Shields set neither armor class and contribute no light armor rating.

If both Heavy and Light armor are equipped:

- Flesh efficiency uses Heavy priority.
- Light armor rating still comes from worn light pieces.
- Incoming magic armor scaling, if relevant, also uses Heavy priority.
