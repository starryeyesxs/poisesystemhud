# Poise System and Minimalist HUD

An SKSE poise system and PrismaUI minimalist HUD for Skyrim Special Edition. The HUD
displays Health, Magicka, Stamina, player poise, weather, and a sneak light indicator.

**Work in progress:** Our poise mission is to introduce true engine-level poise
mechanics to Skyrim with zero dependencies on other poise or combat mods. The
HUD/plugin still requires the tools listed below, and balancing and compatibility
testing are ongoing. See [POISE_GUIDE.txt](./POISE_GUIDE.txt) for a player-friendly
explanation and a list of all INI settings.

**AI disclosure:** This mod was built with AI assistance.

## Requirements

- Skyrim SE/AE, SKSE64, Address Library, and PrismaUI
- Optional: SKSE Menu Framework, for the in-game settings menu

Players do not need a compiler or any build tools. Those are only for
[building from source](#building-from-source-developers).

## Installation (Players)

1. Install the requirements above: SKSE64 and PrismaUI (with Address Library).
2. Download the release archive, `poisesystemhud_1.3.1.zip`.
3. Install it as a mod with Mod Organizer 2 or Vortex (for example, **Install a new mod from an archive**
   in Mod Organizer 2), then enable it.

The archive contains `SKSE/Plugins/poisesystemhud.dll` with its matching PDB and `poisesystemhud.ini`,
`PrismaUI/views/poisesystemhud/`, `fomod/info.xml`, `README.md`, and `POISE_GUIDE.txt`.

The HUD is hidden in menus and while loading. In gameplay, **F10** reloads the view.
For live edits, change the installed copy of the view files, not just the source files.
The plugin writes `poisesystemhud.log` to the SKSE log directory.

Edit the installed `SKSE/Plugins/poisesystemhud.ini` under `[HUD]`: `iSideOffsetLeft` and
`iBottomOffset` position the minimalist cluster. Press **F10** after editing to
reload the view and apply settings. The reload key is fixed to F10; older `iReloadKey`
settings are ignored.
Missing or invalid values retain their previous settings and are logged.

`bEnableWeatherWidget` controls the location/date/weather widget;
`bEnableSneakLightGem` controls the sneak indicator. Set either to `false` to disable it.
Press F10 to apply changes.
The minimalist bars use combat, weapon-drawn, depleted-stat, and sprint state for
visibility. Stamina drain from movement mods also reveals the stamina bar without
requiring special movement-action detection. The prototype's projected heal-over-time layer is not shown because the game
bridge does not provide a reliable future healing target; actual healing still updates
the main bar and damage trail. Weather uses the current weather record's flags; unknown or unavailable
weather is shown as unknown, and interior cells display Indoors. Unnamed exterior
locations fall back to the worldspace name or Wilderness. The displayed calendar
uses the Fourth Era.

The retained dynamic visibility settings now apply to the Minimalist bars:
when all enabled resources are full and the player is idle, the bars fade to
`fIdleOpacity` after `fFadeDelaySeconds`. `bAlwaysOn=true` or `bDynamicHUD=false`
keeps enabled bars visible. The weather widget and sneak light gem are not faded.
Previously these controls only affected the removed layout.

The light gem reads CommonLibSSE-NG's player high-process `lightLevel` field while
sneaking, clamped to 0–100. The field's correspondence to Skyrim's GetLightLevel
condition and its behavior across game versions require in-game verification; an
unavailable or non-finite value hides the gem rather than displaying a false reading.

### Built-in player poise

`bEnableBuiltInPoise=true` in `[Poise]` enables standalone player poise. Equipped light and
heavy armor adds its worn weight times `fArmorWeightMultiplier` (default 1.0) to the configured base.
Player shield weight instead uses `fShieldWeightPoiseMultiplier` (default 0.0, range -1.0 to 2.0).
With `bApplyShieldWeightToNPCs=true` (default), non-player shield weight uses
`fArmorWeightMultiplier`; set it to `false` to use the shield multiplier for them too.
Negative shield multipliers reduce maximum poise, which is floored at 10 after summon scaling.
The settings are available in the menu and apply on F10 reload. Eligible physical hits
deplete poise; a break refills it, starts a grace period, and requests a directional vanilla
stagger. Poise regenerates after the configured delay. Grace protects against eligible physical
and magic poise damage; vanilla magic reactions, scripted reactions, knockdowns, and other engine
reactions are not necessarily suppressed.

`bEnableNPCPoise=true` also enables poise for loaded, living actors with the
`ActorTypeNPC` keyword. The registry starts an actor at full poise on its first eligible hit,
refreshes armor during updates, and discards stale state. Disabling the built-in system disables
both player and NPC poise; F10 applies both switches. Unblocked physical hits use
`fUnblockedPhysicalMultiplier=2.5`; ward spells alone do not count as physical blocking.

Elemental spell impacts damage poise with elemental and magic resistance scaling; concentration
hits are pooled and capped. Confirmed ward interception uses an estimated spell-record magnitude
and `fWardMagicDamageMultiplier=0.35`. Actively blocking scales a hit's magic poise damage by
`fShieldBlockedMagicMultiplier=0.4` with an equipped shield or `fWeaponBlockedMagicMultiplier=0.7` with a weapon alone;
ward interception takes precedence, so the multipliers do not stack. Neither armor weight nor armor class adds a magic damage penalty.
Elemental effects that arrive without a spell projectile, such as Lightning Cloak and other cloaks, are
pooled like concentration spells, and explosions count once per spell and target alongside a direct hit.
Casters need no keyword, so Dragon Priests, Dragons and Hagravens damage the player's poise. Spells cast by
anyone other than the player are scaled by `fNPCMagicPoiseMultiplier` (0.6); with the ward multiplier that
leaves a ward absorbing roughly 79% of an NPC spell's poise damage.
No extra magicka is drained by this feature. The physical-hit hook can preserve a
pre-existing executable call destination after checking the surrounding instructions, but
ABI and gameplay compatibility with Valhalla and BFCO remain unverified. Conflicting update
hooks or unrecognized call-site patches are rejected.

`bSuppressVanillaStaggers=true` (default, in `[Poise]`) drops vanilla staggers on poise-managed
actors, namely the player, NPCs with `bEnableNPCPoise`, and creatures with `bEnableCreaturePoise`, so only
poise breaks stagger them. It covers
hit, bash and unclassified-impact staggers that reach the game's stagger routine, plus the ward-break
reaction, which the game sends as a `WardBreak` animation event, and the `staggerStart` event that a timed
bash sends straight to the animation graph without using the stagger routine. Because that last gate is by
animation event, a `staggerStart` that another mod sends directly to a managed actor is dropped as well.
`bAllowTimedBashStaggers=true` lets the stagger from a timed bash (`HitData` flag `kTimedBash`) through, for
Valhalla Combat or vanilla's stamina-gated interrupt; it defaults to `false`. Shouts are never suppressed: every
`kStagger` effect, including Unrelenting Force and Disarm, keeps its vanilla stagger. Untracked creatures
(ambient critters, flying dragons), other
animation events such as `recoil*`, and staggers that other mods trigger through the stagger routine's
own entry points elsewhere are untouched. The routine gate is installed only if its code bytes and
the shout whitelist hooks validate; the log line beginning `Vanilla stagger suppression:` reports the
result. F10 applies changes to this setting.

### Creature poise

`bEnableCreaturePoise=true` (default) gives creatures a poise pool sized by archetype. Each creature's race
is matched by editor ID (with `ActorTypeDragon`/`ActorTypeGiant` keywords as fallback) when the game data loads,
and the race table takes priority over `ActorTypeNPC`, so Draugr and Falmer are Medium and Dragon Priests are Boss.
The tier multiplier scales `fBasePoise` only; worn armor and spell bonuses still add on top.

| Tier | Setting | Default | Races |
|---|---|---:|---|
| Light | `fLightCreaturePoiseMultiplier` | 0.75 | Wolves, Ice Wolves, Skeevers, Mudcrabs, Foxes, Horkers |
| Medium | `fMediumCreaturePoiseMultiplier` | 1.7 | Draugr, Ice Wolves, Skeletons, Falmer, Chaurus, Sabre Cats, Frostbite Spiders, Ice Wraiths, Spriggans |
| Heavy | `fHeavyCreaturePoiseMultiplier` | 3.75 | Bears, Trolls, Hagravens, Gargoyles, Dwarven Spheres, Chaurus Hunters, Werewolves |
| Boss | `fBossCreaturePoiseMultiplier` | 6.0 | Dragons, Giants, Mammoths, Dwarven Centurions, Lurkers, Dragon Priests |

Ice Wolves share the wolf race, so they are promoted from Light to Medium by display name.
Boss creatures regenerate at `fBossCreatureRegenMultiplier` (1.5) times the normal rate, and a boss poise
break always staggers at full strength so large creatures play their native stagger. After any creature-tier
poise break the actor is immune to another break for `fCreatureStaggerImmunitySeconds` (2.0), so light-attack
spam cannot stunlock it. Rabbits, chickens,
slaughterfish and insects are never tracked, other creatures use an unmodified `fBasePoise`, and flying
creatures keep vanilla behavior until they land. Every attack by a non-`ActorTypeNPC` attacker is classified as a
creature hit and multiplied by `fCreatureMultiplier` (default 0.8) times the attacker's tier damage multiplier
(`fLightCreatureDamageMultiplier` 0.6, `fMediumCreatureDamageMultiplier` 1.2, `fHeavyCreatureDamageMultiplier` 1.375,
`fBossCreatureDamageMultiplier` 2.25; unlisted creatures 1.0) regardless of the race's unarmed weapon.
Commanded actors such as summons have their maximum poise multiplied by `fSummonPoiseMultiplier` (1.5).
Creature armor, flesh-spell and summon refreshes, and removal of untrackable creature entries, run from the
player's update because the per-actor update hook does not tick creatures.
With `bEnableCreaturePoise=false`, creatures keep vanilla behavior and Draugr, Falmer and Dragon Priests are
treated as ordinary NPCs. F10 applies changes to these settings.

### Blocking

Physical hits taken while blocking are scaled from the unblocked hit: `fShieldBlockedPhysicalMultiplier` (0.25)
when any shield is equipped and `fWeaponBlockedPhysicalMultiplier` (0.45) when blocking with a weapon alone, so a
shield lets 25% of an unblocked hit through and a weapon 45%. The unblocked multiplier always applies first, so a
blocked hit's poise damage is `(base + bonus) × type × power × fUnblockedPhysicalMultiplier × block`. This replaces
the old single `fBlockedDamageMultiplier`, which is no longer read. Because the old value scaled the base hit instead
(an effective 16% of an unblocked hit), blocking now absorbs less than before.

### Target poise bar

`bShowTargetPoiseBar=true` in `[HUD]` (or **Show Target Poise Bar** in SKSE Menu Framework) shows an upper-right
bar reading `<Name> [<Tier>]  <current> / <maximum>` for the most recently hit tracked NPC or creature. It
fades after 4 seconds without a hit, and the fill dims while the target is immune to another break. It is off
by default and does not follow the crosshair.

### Live poise telemetry (developer overlay)

A click-through upper-left panel for verifying poise live. It shows the player's poise, flesh and Light Armor
bonuses, regen rate, regen-delay and immunity timers and ceiling; the equipped right-hand weapon benchmark
(`1H Light` / `Power` unblocked poise damage, including a labeled estimate of the physical-damage bonus from the
weapon's base damage); the last-hit target with a light-hits-to-break estimate (kept for 15 seconds, or until it
dies, unloads or another target is hit); and the last 5 poise hits involving you, newest first, as
`[OUT|IN] Attacker -> Target (Type): final [Base + RawDmg [CAP] * multipliers]` with a `[STAGGER!]` tag on breaks.
The debuff pills show the active poise debuffs (see below).

It is off by default and costs nothing while off. Three controls share one state:

- `bShowPoiseTelemetry=true` in `[HUD]`, applied with F10.
- **Show Live Poise Telemetry (Dev)** in SKSE Menu Framework, under "Widget Visibility & Display".
- The console command `showpoisedev` toggles it in game and prints `Poise Telemetry Overlay: ON` or `OFF`. It
  is not saved to the INI, so F10 reloads the INI value. The command reuses an unused built-in console command
  slot (`ClearScreenBlood`), so that vanilla command is unavailable while the plugin is loaded.

### Poise status debuffs

Hostile damage-over-time and drain effects slow posture recovery instead of dealing direct poise damage. Each
debuff changes one value, same-type effects never stack, and different debuffs coexist. Only effects flagged hostile
count, and an effect the actor cast on itself is ignored (except a detrimental bleed, which combat-hit perks apply with
the victim as caster). Both the player and tracked NPCs and creatures are affected.

| Debuff | Trigger | Effect |
| --- | --- | --- |
| Sapped | hostile absorb effect (Vampiric Drain, Absorb Health and similar) | poise recovery stops |
| Enfeebled | hostile poison (poison resist, or the `MagicAlchHarmful` keyword) | recovery is capped at a ceiling of maximum poise |
| Numbed | lingering hostile frost health damage | adds `fNumbedRegenDelayBonus` seconds to the regen delay set by a hit |
| Hemorrhaging | hostile effect whose editor ID, name or spell name contains a `sHemorrhagingEffectKeywords` entry, or the vanilla `Skyrim.esm` bleed effect `PerkBleedingDamage` (`0x000C367A`, applied by the War Axe and Battleaxe perks) | recovery rate is multiplied by `fHemorrhagingRegenRateMultiplier` |
| Scorched | lingering hostile fire health damage | incoming non-projectile physical hits deal `fScorchedMeleeDamageMultiplier` times the poise damage |

Enfeebled scales with the strongest poison's magnitude per second: 0 gives `fEnfeebledRegenCeilingMax` and
`fEnfeebledMagnitudeForMin` or more gives `fEnfeebledRegenCeilingMin`. The ceiling never drops below
`fMinGlobalRegenCeiling`, only limits regeneration (poise already above it is not reduced and a poise break still
refills fully), and does not change maximum poise. The boss regeneration multiplier still applies. Burst and
concentration spell impacts keep their direct poise damage; the lingering ticks and absorb effects never deal any.
Vanilla Skyrim's War Axe and Battleaxe perk bleed (`PerkBleedingDamage`, `0x000C367A` in `Skyrim.esm`) is matched by
form ID, since it is flagged detrimental rather than hostile and has no editor ID at runtime; bleed mods are matched by
keyword, which is comma-separated and case-insensitive. Bleed effects are accepted when flagged hostile or detrimental,
and a missing caster is fine; a detrimental bleed also counts when the victim is its own caster, as the combat-hit perks
apply it. Other effects count as hostile when their record or parent spell is flagged hostile.

A hostile absorb effect (Vampiric Drain, an Absorb Health/Stamina/Magicka weapon enchantment) also keeps Sapped on for
about 2 seconds from the hit, because weapon enchantments leave no lasting effect for the 0.5 second scan to find. A
player absorb hit selects the target for the telemetry card. The pulse is gated by the same Sapped toggles, and it also
lights the violet Health bar for the reactive bar effects.

`bEnablePoiseDebuffs=false` in `[Poise Debuffs]` (or **Enable Poise Status Debuffs** at the top of the **Poise
Status Debuffs** menu section) skips all effect scanning and neutralizes every debuff; each debuff also has its own
`bEnable...Debuff` switch. With `bShowPoiseDebuffIcons=true` in `[HUD]` (or **Show Poise Debuff Icons**), a strip of
24 px icons beneath the player poise bar shows the active debuffs, a tick on the bar marks the Enfeebled ceiling, and
the poise bar stays visible while a debuff is active. The icons work without the telemetry overlay. Changes apply
with F10.

### Reactive resource bar effects

With `bShowReactiveBarEffects=true` in `[HUD]` (default, or **Show Reactive Resource Bar Effects** under Widget
Visibility & Display), hostile effects on the player tint the Minimalist HUD's Health, Magicka and Stamina bars:
poison (toxic green) and Sapped (violet) tint the Health fill, a lingering burn adds an ember border and glow, a
bleed pulses crimson, a Shock hit crackles electric blue on both Health and Magicka for 1.5 seconds, and a lingering
frost effect chills the Stamina bar. They reuse the poise debuff effect scan, work with `bEnablePoiseDebuffs=false`,
and need no new HUD elements; turning the setting off clears every effect and stops the scan for the player.
Changes apply with F10.

For development, `xmake build arpghud_magic_diagnostic` creates a separate DLL/PDB overlay in
`dist/poisesystemhud_1.3.1-magic-diagnostic`, leaving the normal package untouched. Installing that DLL
opts into experimental, logging-only hooks on the 1.7.104 magic-effect call and beam, flame, and
missile projectile impact, `HandleHits`, and `ProcessImpacts` vtables and projectile spell- and ward-collision call sites. Each validates the expected original call or vtable target
before installation and skips a conflicting patch. The log records form IDs, projectile type,
collision layer, effect magnitude, and effect acceptance; it does not apply magic poise damage,
ward mitigation, or magicka drain. The latest diagnostic also logs projectile handles, the
enclosing sampled impact, collision-handler, impact-processing and spell-collision projectile handles for effect calls, ward power before and after the impact handler,
and its returned pointer. A zero enclosing impact handle can also mean the impact was not sampled; the
returned pointer by itself does not prove a successful ward block. Each hook logs at most 200 samples per game process; restart
between test scenarios if the limit is reached. Collision layer 29 is Skyrim's ward layer, but
its appearance alone does not establish a defender or prove a successful ward block.
The original diagnostic build discarded projectile impact return values and should not be used.
The latest build reports `ward collision outcomes` at startup and logs collision layer, ward power,
engine return value and output flag from the ward-check path, plus elemental effect resistance data.
It also logs game executable
stack RVAs for up to eight accepted Flames and eight accepted Chain Lightning effects per run. Its gameplay compatibility
still requires in-game verification. On 1.7.104, the last impact argument is a byte-sized flag;
older diagnostic logs display undefined high bits for `arg7` and should not be interpreted as a
32-bit value.

The built-in hit and player-update hook signatures are currently validated only for Skyrim
**1.7.104.0**. If the runtime is different, an incompatible hook is already modified, or Chocolate Poise is
loaded, the built-in mechanic is disabled and vanilla behavior is preserved. The log reports
hook validation and installation. Other combat or animation overhauls may also conflict; disable
built-in poise if using another player poise overhaul. Runtime and in-game compatibility must be
tested with the installed mod list.

`bShowPoiseBar=true` in `[HUD]` shows the existing yellow bar when built-in poise is available,
or when the optional bridge supplies a positive maximum. It controls display only; setting it
to `false` does not disable gameplay. Built-in poise takes display priority. When it is disabled
or unavailable, the bridge is used; with neither, the bar remains hidden. This mod does not take
TrueHUD special-resource ownership or modify its enemy bars.

For testing, set `bShowPoiseNumbers=true` in `[HUD]` or enable **Show Poise Numbers**
in SKSE Menu Framework to display current and maximum poise to one decimal place.
The numbers follow the poise bar's visibility and fade; they are off by default.
`bShowTargetPoiseBar` is documented under "Target poise bar".
Press F10 after editing the INI.

The following `[Poise]` settings apply on F10 without reinstalling hooks. Missing or invalid
values retain their previous value. Disabling or re-enabling the mechanic resets transient
poise; loading a save starts at full poise.

| Setting | Default | Range | Meaning |
|---|---:|---:|---|
| `bEnableBuiltInPoise` | `true` | boolean | Enable built-in gameplay |
| `bEnableNPCPoise` | `true` | boolean | Enable tracked NPC poise |
| `bAllowTimedBashStaggers` | `false` | boolean | Let the vanilla stagger from a timed bash through |
| `bEnableCreaturePoise` | `true` | boolean | Enable creature poise pools and creature stagger suppression |
| `fLightCreaturePoiseMultiplier` | 0.75 | 0.1–100 | Light creature `fBasePoise` scale |
| `fMediumCreaturePoiseMultiplier` | 1.7 | 0.1–100 | Medium creature `fBasePoise` scale |
| `fHeavyCreaturePoiseMultiplier` | 3.75 | 0.1–100 | Heavy creature `fBasePoise` scale |
| `fBossCreaturePoiseMultiplier` | 6.0 | 0.1–100 | Boss creature `fBasePoise` scale |
| `fBossCreatureRegenMultiplier` | 1.5 | 0–100 | Boss creature poise regeneration scale |
| `fLightCreatureDamageMultiplier` | 0.6 | 0–10 | Light creature outgoing poise damage scale |
| `fMediumCreatureDamageMultiplier` | 1.2 | 0–10 | Medium creature outgoing poise damage scale |
| `fHeavyCreatureDamageMultiplier` | 1.375 | 0–10 | Heavy creature outgoing poise damage scale |
| `fBossCreatureDamageMultiplier` | 2.25 | 0–10 | Boss creature outgoing poise damage scale |
| `fCreatureStaggerImmunitySeconds` | 2 | 0–60 | Post-break protection for creature tiers |
| `fSummonPoiseMultiplier` | 1.5 | 0.1–100 | Maximum poise scale for commanded actors |
| `fNPCMagicPoiseMultiplier` | 0.6 | 0–10 | Magic poise scale for spells not cast by the player |
| `fBasePoise` | 40 | 1–100000 | Base maximum poise before bonuses and scaling |
| `fArmorWeightMultiplier` | 1 | 0–100 | Maximum poise per worn armor-weight unit |
| `fShieldWeightPoiseMultiplier` | 0 | -1–2 | Shield weight multiplier for the player, and for non-player actors when the toggle below is false |
| `bApplyShieldWeightToNPCs` | `true` | boolean | Use `fArmorWeightMultiplier` for non-player shield weight instead |
| `fBaseHitPoiseDamage` | 8 | 0–10000 | Base poise damage per hit |
| `fWeaponWeightDamageMultiplier` | 0.5 | 0–100 | Additional damage per weapon-weight unit |
| `fTwoHandedMultiplier` | 0.85 | 0–10 | Two-handed melee multiplier; lower than 1 because weapon weight and the physical-damage bonus already scale with two-handers |
| `fShieldBashMultiplier` | 1 | 0–10 | Bash multiplier when a shield is equipped |
| `fWeaponBashMultiplier` | 0.35 | 0–10 | Bash multiplier for a weapon bash without a shield |
| `fCreatureMultiplier` | 0.8 | 0–10 | Multiplier for every attack by a non-NPC creature |
| `fProjectileMultiplier` | 0.8 | 0–10 | Physical projectile multiplier |
| `fPowerAttackMultiplier` | 1.5 | 0–10 | Power-attack multiplier |
| `fShieldBlockedPhysicalMultiplier` | 0.25 | 0–1 | Share of an unblocked hit's poise damage taken while blocking with a shield |
| `fWeaponBlockedPhysicalMultiplier` | 0.45 | 0–1 | Share of an unblocked hit's poise damage taken while blocking with a weapon only |
| `fUnblockedPhysicalMultiplier` | 2.5 | 0–10 | Multiplier applied to every physical hit before any block share |
| `fRegenDelaySeconds` | 3 | 0–600 | Delay following poise damage |
| `fRegenRatePerSecond` | 8 | 0–10000 | Poise points restored per gameplay second |
| `fStaggerImmunitySeconds` | 1 | 0–60 | Post-break protection duration |
| `fFleshSpellPoiseMultiplier` | 0.45 | 0–100 | Maximum poise per active Alteration armor-rating point |
| `fDragonhidePoiseBonus` | 60 | 0–100000 | Maximum poise bonus for Dragonhide |
| `fPhysicalDamagePoiseMultiplier` | 0.05 | 0–0.25 | Share of the hit's physical damage added to base poise damage |
| `fPhysicalDamagePoiseCap` | 6 | 0–25 | Hard cap on that physical-damage bonus |
| `fLightArmorRatingPoiseMultiplier` | 0.15 | 0–0.5 | Maximum poise per base armor-rating point of worn Light Armor (shields excluded) |
| `fLightArmorRatingPoiseCap` | 15 | 0–50 | Hard cap on the Light Armor rating bonus |
| `fLightArmorFleshMultiplier` | 0.75 | 0–1 | Flesh spell and Dragonhide poise scale while wearing Light Armor |
| `fHeavyArmorFleshMultiplier` | 0.45 | 0–1 | Flesh spell and Dragonhide poise scale while wearing Heavy Armor; takes priority over Light, 0 disables |
| `fMagicBurstPoiseMultiplier` | 2 | 0–100 | Burst spell magnitude multiplier |
| `fMagicStaggerPoiseMultiplier` | 10 | 0–100 | Reserved; no stagger contribution is currently available |
| `fConcentrationPoiseMultiplier` | 1 | 0–100 | Concentration spell magnitude multiplier |
| `fConcentrationIntervalSeconds` | 1 | 0.01–60 | Concentration pool interval |
| `fConcentrationMaxPoisePerInterval` | 15 | 0–10000 | Maximum pooled damage per interval |
| `bConcentrationSpellsDealPoise` (`[Magic Poise]`) | false | bool | When false, channeled concentration spells (Flames, Frostbite, Sparks) deal no direct poise damage, leaving it to impact spells; their Scorched and Numbed debuffs still apply |
| `fWardMagicDamageMultiplier` | 0.35 | 0–1 | Confirmed ward interception multiplier |
| `fShieldBlockedMagicMultiplier` | 0.4 | 0–1 | Magic poise multiplier while blocking with a shield |
| `fWeaponBlockedMagicMultiplier` | 0.7 | 0–1 | Magic poise multiplier while blocking with a weapon |

`[Poise Debuffs]` keys (see "Poise status debuffs"):

| Key | Default | Range | Meaning |
| --- | --- | --- | --- |
| `bEnablePoiseDebuffs` | true | bool | Master switch; false skips effect scanning entirely |
| `bEnableSappedDebuff`, `bEnableEnfeebledDebuff`, `bEnableNumbedDebuff`, `bEnableHemorrhagingDebuff`, `bEnableScorchedDebuff` | true | bool | Per-debuff switches |
| `fEnfeebledRegenCeilingMin` | 0.70 | 0–1 | Recovery ceiling at full poison strength |
| `fEnfeebledRegenCeilingMax` | 0.85 | 0–1 | Recovery ceiling for the weakest poison |
| `fEnfeebledMagnitudeForMin` | 10 | 0.1–1000 | Poison magnitude per second that reaches the minimum ceiling |
| `fMinGlobalRegenCeiling` | 0.60 | 0–1 | No debuff lowers the recovery ceiling below this |
| `fNumbedRegenDelayBonus` | 1.0 | 0–10 | Extra regen-delay seconds while Numbed |
| `fHemorrhagingRegenRateMultiplier` | 0.65 | 0–1 | Regen rate multiplier while Hemorrhaging |
| `sHemorrhagingEffectKeywords` | bleed,hemorrhag | text | Comma-separated bleed keywords matched against effect IDs and names |
| `fScorchedMeleeDamageMultiplier` | 1.20 | 0.5–3 | Incoming non-projectile physical poise damage multiplier while Scorched |

Hit damage starts as `fBaseHitPoiseDamage + weaponWeight * fWeaponWeightDamageMultiplier + min(fPhysicalDamagePoiseCap, physicalDamage * fPhysicalDamagePoiseMultiplier)`.
The relevant attack-category, power-attack, and blocked multipliers are applied afterward.
Missing weapon weight uses zero. Values are initial balance defaults, not in-game tuning claims.

### Poise provider bridge

The public [TrueHUD API](https://github.com/ersh1/TrueHUD/blob/master/src/TrueHUDAPI.h)
accepts special-resource callbacks but does not expose them for another plugin to read.
[Chocolate Poise](https://github.com/doodlum/skyrim-poise/blob/main/src/UI/PoiseAVHUD.cpp)
stores its Poise values in its own manager. Unmodified Chocolate Poise cannot supply this bar:
it needs an opt-in provider or companion bridge with legitimate access to those values.
[Valhalla Combat](https://github.com/D7ry/valhallaCombat/blob/master/src/bin/stunHandler.cpp)
does not apply its stun damage to the player. Installing TrueHUD, Chocolate Poise, or Valhalla
alone does not connect this bar.

Provider developers can include [PoiseBridgeAPI.h](src/PoiseBridgeAPI.h), dynamically look up
`poisesystemhud.dll` and its `ARPGHUD_RegisterPoiseProvider` and `ARPGHUD_UnregisterPoiseProvider`
exports (for example at SKSE `kPostLoad`), and use the exact function pointer types and
`kVersion` from that header. The registration returns false for a version mismatch, null
callback, or an already-registered different provider. Only one provider is accepted; no
plugin takes or replaces TrueHUD's provider.

The `noexcept` callback receives the player and output pointers for current and maximum values.
Return `true` after writing both finite values, or `false` when unavailable. A non-positive
maximum hides the bar; the UI clamps the displayed percentage. The callback runs on the SKSE
UI task used by this HUD, approximately every 100 ms while gameplay is ready and the setting
is enabled. Providers must synchronize access to their own state, avoid blocking, and not
re-enter either bridge registration function from their callback. Unregister the same callback
before its code or state can be unloaded; unregistration waits for an in-flight call to finish.
Use `GetModuleHandleW`/`GetProcAddress` and check for null so neither side needs a hard DLL
dependency. A provider must not assume a `1 / 1` sentinel is a valid player poise value.

For example, after all SKSE plugins have loaded, a provider can connect without linking to
this DLL:

```cpp
#include "PoiseBridgeAPI.h"
#include <Windows.h>

// Implement this using the provider's own synchronized player-poise state.
bool ReadPlayerPoise(RE::Actor* player, float* current, float* maximum) noexcept;

bool ConnectPoiseBar()
{
    const auto module = GetModuleHandleW(L"poisesystemhud.dll");
    if (!module) {
        return false;
    }
    const auto registerProvider = reinterpret_cast<ARPGHUD_POISE_API::RegisterProvider>(
        GetProcAddress(module, ARPGHUD_POISE_API::kRegisterExport));
    const auto unregisterProvider = reinterpret_cast<ARPGHUD_POISE_API::UnregisterProvider>(
        GetProcAddress(module, ARPGHUD_POISE_API::kUnregisterExport));
    return registerProvider && unregisterProvider &&
        registerProvider(ARPGHUD_POISE_API::kVersion, ReadPlayerPoise);
}
```

For unloadable providers, retain the unregister function and call it with `ReadPlayerPoise`
before unloading. The callback must return false during loads or whenever the provider cannot
produce a valid player value.

The plugin reapplies the vanilla Health, Magicka, and Stamina clip visibility, alpha, and
position every 100 ms while the custom view is ready. This also hides their penalty-meter
children. Other HUD replacements may use different Scaleform clip names.

## Building from Source (Developers)

Building is only needed to modify the mod; players should use the release archive above.

Requirements: xmake 3.0 or later and a C++23 compiler (Visual Studio with the MSVC toolchain on Windows).
Clone the repository with its submodules so `lib/commonlibsse-ng` is present, then run:

```
xmake -y
```

The default target builds `poisesystemhud.dll` and assembles the Mod Organizer 2-ready package in
`dist/poisesystemhud_1.3.1/`, containing `SKSE/Plugins/poisesystemhud.dll`, its matching PDB and
`poisesystemhud.ini`, `PrismaUI/views/poisesystemhud/`, `fomod/info.xml`, `README.md`, and `POISE_GUIDE.txt`.
Install that folder, or an archive of its contents, as a mod. Archive the folder's contents rather than the
folder itself so `SKSE/` and `PrismaUI/` sit at the archive root.

The diagnostic variants are opt-in targets (`arpghud_magic_diagnostic` and `arpghud_stagger_diagnostic`) that
write to their own `dist/poisesystemhud_1.3.1-<variant>/` folders; build one with, for example,
`xmake build arpghud_magic_diagnostic`.

Tests: run the JavaScript suite with `node --test tests`. The C++ tests in `tests/*.test.cpp` are standalone
programs that each compile and run on their own.
