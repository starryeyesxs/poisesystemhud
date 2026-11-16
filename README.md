# Poise System and Minimalist HUD

An SKSE poise system and PrismaUI minimalist HUD for Skyrim Special Edition. The HUD displays Health, Magicka, Stamina, player poise, weather, and a sneak light indicator.

**Work in progress:** Our poise mission is to introduce true engine-level poise mechanics to Skyrim with zero dependencies on other poise or combat mods. The HUD/plugin still requires the tools listed below, and balancing and compatibility testing are ongoing. See [POISE_GUIDE.txt](./POISE_GUIDE.txt) for a player-friendly explanation and poise settings.

**AI disclosure:** This mod was built with AI assistance.

## Credits

- [Prisma UI](https://nexusmods.com) by StarkMP provides the framework for the HUD.

## Requirements

- Skyrim SE/AE, SKSE64, Address Library, and PrismaUI
- Optional: SKSE Menu Framework, for the in-game settings menu

Built-in poise is currently validated only for Skyrim **1.7.104.0**; see [Compatibility and troubleshooting](#compatibility-and-troubleshooting) for details.

## Installation

1. Install SKSE64, Address Library, and PrismaUI for your game version. Install SKSE Menu Framework too if you want to change settings in game.
2. Download the release archive, `poisesystemhud_1.3.1.zip`.
3. Install it as a mod with Mod Organizer 2 or Vortex (for example, **Install a new mod from an archive** in Mod Organizer 2), then enable it.

## Changing settings

With **SKSE Menu Framework** installed, open its menu and select **Poise System and Minimalist HUD → Settings**. Almost every HUD option and poise gameplay setting—including armor and weapon multipliers, block and magic mitigation, creature tier scaling, and status debuff rules—can be adjusted right in the menu. Close the menu and press **F10** during gameplay to apply your changes and refresh the HUD (many visual adjustments update immediately).

Without the menu framework, open the installed `SKSE/Plugins/poisesystemhud.ini` in your mod manager, save your edits, and press **F10** during gameplay to live-reload them without restarting. Missing or invalid values safely keep their previous settings and are logged. **F10** is the dedicated reload key.

## HUD

The HUD is hidden in menus and while loading. Health, Magicka, Stamina, and player poise appear in the minimalist cluster. Use **Left Offset** and **Bottom Offset** in the settings menu (or `iSideOffsetLeft` and `iBottomOffset` in `[HUD]`) to reposition it. The weather/location widget and sneak light gem can also be turned off in the menu.

By default, the resource bars fade after the player is idle with full resources. Combat, drawing a weapon or spell, depleted resources, and sprinting can reveal them; stamina drain from movement mods also reveals the stamina bar. Adjust the idle opacity and fade delay, or keep the bars visible using **Always On** or by disabling dynamic HUD. The weather widget and light gem do not fade with the bars.

The weather widget shows **Indoors** inside and **Unknown** if weather is unavailable; unnamed exterior locations use the worldspace name or Wilderness. The calendar uses the Fourth Era. The light gem appears while sneaking, but its readings still need verification across game versions and it hides when a reading is unavailable.

The yellow poise bar can be hidden without disabling gameplay. It displays the built-in system when available, or a compatible optional provider when built-in poise is disabled or unavailable. Without either, the bar stays hidden. You can also enable **Show Poise Numbers** for exact current and maximum values.

### Target poise bar

**Show Target Poise Bar** displays the name, tier, and poise of the most recently hit tracked NPC or creature in the upper-right corner. It is off by default, fades after four seconds without a hit, and dims while the target is immune to another poise break. It remains stationary in the corner rather than following the crosshair.

## Poise gameplay

### Built-in player poise

`bEnableBuiltInPoise=true` in `[Poise]` enables standalone player poise. Equipped light and heavy armor adds its worn weight times `fArmorWeightMultiplier` (default 1.0) to the base poise pool. Player shield weight instead uses `fShieldWeightPoiseMultiplier` (default 0.0, range -1.0 to 2.0). With `bApplyShieldWeightToNPCs=true` (default), non-player shield weight uses `fArmorWeightMultiplier`; set it to `false` to use the shield multiplier for them too. Negative shield multipliers reduce maximum poise, which cannot drop below 10 after summon scaling. Light Armor also adds poise based on its armor rating. Flesh spells and Dragonhide add poise too, with a larger bonus when unarmored than when wearing Light or Heavy Armor. All of these settings are available in the menu and apply on F10 reload. Eligible physical hits deplete poise; a break refills it, starts a grace period, and triggers a directional stagger. Poise regenerates after the configured delay. Grace protects against eligible physical and magic poise damage; vanilla magic reactions, scripted reactions, knockdowns, and other engine reactions are not necessarily suppressed.

`bEnableNPCPoise=true` also enables poise for eligible loaded, living NPCs, who start at full poise. Disabling the built-in system disables player, NPC, and creature poise; F10 applies these switches. Unblocked physical hits use `fUnblockedPhysicalMultiplier=2.5`; ward spells alone do not count as physical blocking.

Elemental spell impacts damage poise with elemental and magic resistance scaling. Channeled concentration spells such as Flames, Frostbite, and Sparks do not deal direct poise damage by default, though their status debuffs still apply. Enable **Concentration Spells Deal Poise** to allow pooled, capped damage from them. Repeated cloak effects are also pooled and capped. Confirmed ward interception uses an estimated spell-record magnitude and `fWardMagicDamageMultiplier=0.35`. Actively blocking scales a hit's magic poise damage by `fShieldBlockedMagicMultiplier=0.4` with an equipped shield or `fWeaponBlockedMagicMultiplier=0.7` with a weapon alone; ward interception takes precedence, so the multipliers do not stack. Neither armor weight nor armor class adds a magic damage penalty. Explosions count once per spell and target alongside a direct hit. Casters need no keyword, so Dragon Priests, Dragons, and Hagravens damage the player's poise. Spells cast by anyone other than the player are scaled by `fNPCMagicPoiseMultiplier` (0.6); together with the ward multiplier, that leaves 21% of the original spell poise damage. No extra magicka is drained by this feature. Not every spell or impact path is covered.

By default, **Suppress Vanilla Staggers** suppresses ordinary hit, bash, and ward-break staggers on actors managed by the poise system, so poise breaks cause their stagger instead. **Allow Timed Bash Staggers** lets timed-bash staggers through and is off by default. Shouts such as Unrelenting Force and Disarm keep their vanilla stagger. Other scripted or mod-added reactions may still occur; untracked creatures and flying creatures retain vanilla behavior. Suppression depends on compatibility checks; see [Compatibility and troubleshooting](#compatibility-and-troubleshooting).

### Creature poise

`bEnableCreaturePoise=true` (default) gives creatures a poise pool sized by archetype. Draugr and Falmer are Medium and Dragon Priests are Boss. The tier multiplier scales `fBasePoise` only; worn armor and spell bonuses still add on top.

| Tier | Setting | Default | Races |
|---|---|---:|---|
| Light | `fLightCreaturePoiseMultiplier` | 0.75 | Wolves, Skeevers, Mudcrabs, Foxes, Horkers |
| Medium | `fMediumCreaturePoiseMultiplier` | 1.7 | Draugr, Ice Wolves, Skeletons, Falmer, Chaurus, Sabre Cats, Frostbite Spiders, Ice Wraiths, Spriggans |
| Heavy | `fHeavyCreaturePoiseMultiplier` | 3.75 | Bears, Trolls, Hagravens, Gargoyles, Dwarven Spheres, Chaurus Hunters, Werewolves |
| Boss | `fBossCreaturePoiseMultiplier` | 6.0 | Dragons, Giants, Mammoths, Dwarven Centurions, Lurkers, Dragon Priests |

Ice Wolves share the wolf race, so they are promoted from Light to Medium by display name. Boss creatures regenerate at `fBossCreatureRegenMultiplier` (1.5) times the normal rate, and a boss poise break always staggers at full strength so large creatures play their native stagger. After any creature-tier poise break, the actor is immune to another break for `fCreatureStaggerImmunitySeconds` (2.0), so light-attack spam cannot stunlock it. Rabbits, chickens, slaughterfish, and insects are never tracked, other creatures use an unmodified `fBasePoise`, and flying creatures keep vanilla behavior until they land. Every attack by a creature is classified as a creature hit and multiplied by `fCreatureMultiplier` (default 0.8) times the attacker's tier damage multiplier (`fLightCreatureDamageMultiplier` 0.6, `fMediumCreatureDamageMultiplier` 1.2, `fHeavyCreatureDamageMultiplier` 1.375, `fBossCreatureDamageMultiplier` 2.25; unlisted creatures 1.0) regardless of the race's unarmed weapon. Commanded actors such as summons have their maximum poise multiplied by `fSummonPoiseMultiplier` (1.5). With `bEnableCreaturePoise=false`, creatures keep vanilla behavior and Draugr, Falmer, and Dragon Priests are treated as ordinary NPCs. F10 applies changes to these settings.

### Blocking

Physical hits taken while blocking are scaled from the unblocked hit: `fShieldBlockedPhysicalMultiplier` (0.25) when any shield is equipped and `fWeaponBlockedPhysicalMultiplier` (0.45) when blocking with a weapon alone, so a shield lets 25% of an unblocked hit through and a weapon 45%. The unblocked multiplier always applies first, so a blocked hit's poise damage is `(base + bonus) × type × power × fUnblockedPhysicalMultiplier × block`.

### Poise status debuffs

Hostile damage-over-time and drain effects affect poise rather than dealing direct poise damage. Same-type effects do not stack, but different debuffs can coexist. Both the player and tracked NPCs and creatures are affected. Self-cast effects are ignored, except for bleeds applied by combat-hit perks.

Debuff	Trigger	Effect
Sapped	hostile absorb effect (Vampiric Drain, Absorb Health and similar)	poise recovery stops
Enfeebled	hostile poison	recovery is capped at a ceiling of maximum poise
Numbed	lingering hostile frost health damage	adds fNumbedRegenDelayBonus seconds to the regen delay set by a hit
Hemorrhaging	bleed effects, including vanilla War Axe and Battleaxe perk bleeds and supported mod-added bleeds	recovery rate is multiplied by fHemorrhagingRegenRateMultiplier
Scorched	lingering hostile fire health damage	incoming non-projectile physical hits deal fScorchedMeleeDamageMultiplier times the poise damage

Enfeebled scales with the strongest poison's magnitude per second: 0 gives fEnfeebledRegenCeilingMax and fEnfeebledMagnitudeForMin or more gives fEnfeebledRegenCeilingMin. The ceiling never drops below fMinGlobalRegenCeiling, only limits regeneration (poise already above it is not reduced and a poise break still refills fully), and does not change maximum poise. The boss regeneration multiplier still applies. Burst spell impacts and enabled concentration spells can deal direct poise damage; lingering damage-over-time ticks and absorb effects do not. Mod-added bleeds are identified by the comma-separated, case-insensitive sHemorrhagingEffectKeywords setting.
A hostile absorb effect (Vampiric Drain, an Absorb Health/Stamina/Magicka weapon enchantment) also keeps Sapped on for about 2 seconds from the hit. It can also turn the Health bar violet when reactive bar effects are enabled.
bEnablePoiseDebuffs=false in [Poise Debuffs] (or Enable Poise Status Debuffs at the top of the Poise Status Debuffs menu section) disables every poise debuff; each debuff also has its own bEnable...Debuff switch. With bShowPoiseDebuffIcons=true in [HUD] (or Show Poise Debuff Icons), a strip of icons beneath the player poise bar shows the active debuffs, a tick on the bar marks the Enfeebled ceiling, and the poise bar stays visible while a debuff is active. Changes apply with F10.

Reactive resource bar effects

With bShowReactiveBarEffects=true in [HUD] (default, or Show Reactive Resource Bar Effects under Widget Visibility & Display), hostile effects on the player tint the Minimalist HUD's Health, Magicka, and Stamina bars: poison (toxic green) and Sapped (violet) tint the Health fill, a lingering burn adds an ember border and glow, a bleed pulses crimson, a Shock hit crackles electric blue on both Health and Magicka for 1.5 seconds, and a lingering frost effect chills the Stamina bar. They reuse the poise debuff effect scan, work with bEnablePoiseDebuffs=false, and need no new HUD elements; turning the setting off clears the reactive bar visuals. Changes apply with F10.

Compatibility and troubleshooting

The built-in poise system is currently validated for Skyrim 1.7.104.0 and automatically disables itself if it detects an unsupported game version, a conflicting hook, or Chocolate Poise. Its primary goal is to work smoothly alongside major combat and UI setups—and in extensive testing alongside Valhalla Combat, BFCO, TrueHUD, and other combat mods, it runs with zero issues. While it is balanced primarily with modern combat frameworks in mind, it remains fully playable in vanilla setups, and because virtually every multiplier and threshold is customizable, it can be easily tuned to fit almost any load order.
Disable built-in poise (bEnableBuiltInPoise=false) if you prefer to use a different poise overhaul alongside the minimalist HUD. Installing TrueHUD, Chocolate Poise, or Valhalla Combat does not link their internal stun or poise meters to this HUD, and this mod does not overwrite TrueHUD's special resource bars or enemy widgets.
If the HUD or poise system is not working, verify that all requirements are installed, the plugin is enabled, and you are running a supported game version. Remember that settings only reload during active gameplay (F10), and any invalid INI values or hook/compatibility issues are logged to poisesystemhud.log inside your SKSE log directory. If you want to inspect live poise numbers, timers, and recent hit calculations while tuning your balance, you can enable Show Live Poise Telemetry in the menu or type showpoisedev in the console.

Settings reference

The menu is the easiest way to adjust settings. For a full list of poise INI settings and a gameplay explanation, see POISE_GUIDE.txt. The following tables give the defaults and accepted INI ranges.

HUD and appearance


Setting ([HUD])	Default	Range	Meaning
bEnableWeatherWidget	true	boolean	Show location, date, and weather
bEnableSneakLightGem	true	boolean	Show the light gem while sneaking
iSideOffsetLeft	20	-500–2000	Cluster distance from the left edge, in pixels
iBottomOffset	16	-200–2000	Cluster distance from the bottom edge, in pixels
bShowPoiseBar	true	boolean	Show the player poise bar when poise is available
bShowPoiseNumbers	false	boolean	Show current and maximum player poise
bShowTargetPoiseBar	false	boolean	Show the most recently hit target's poise
bShowPoiseDebuffIcons	true	boolean	Show active player poise debuffs and the recovery ceiling
bShowReactiveBarEffects	true	boolean	Show status effects on resource bars
bDynamicHUD	true	boolean	Fade resource bars when idle
bAlwaysOn	false	boolean	Keep enabled bars visible at full opacity
fIdleOpacity	0	0–1	Resource bar opacity when idle
fFadeDelaySeconds	3	0–600	Seconds before idle bars fade
bShowPoiseTelemetry	false	boolean	Show the upper-left poise telemetry overlay for testing balance


Setting ([MinimalistGraphics])	Default	Range	Meaning
bMinBarFiligree	true	boolean	Show decorative bar end caps and trim
fMinBarShine	1	0–2	Bar shine intensity
fMinBarShadow	1	0–2	Bar shadow depth
bMinBarNotches	false	boolean	Show quarter marks on bar tracks


Poise settings

The following [Poise] settings apply on F10. Missing or invalid values retain their previous value. Disabling or re-enabling the mechanic resets transient poise; loading a save starts at full poise.

Setting	Default	Range	Meaning
bEnableBuiltInPoise	true	boolean	Enable built-in gameplay
bEnableNPCPoise	true	boolean	Enable tracked NPC poise
bSuppressVanillaStaggers	true	boolean	Suppress supported vanilla staggers on poise-managed actors
bAllowTimedBashStaggers	false	boolean	Let the vanilla stagger from a timed bash through
bEnableCreaturePoise	true	boolean	Enable creature poise pools and creature stagger suppression
fLightCreaturePoiseMultiplier	0.75	0.1–100	Light creature fBasePoise scale
fMediumCreaturePoiseMultiplier	1.7	0.1–100	Medium creature fBasePoise scale
fHeavyCreaturePoiseMultiplier	3.75	0.1–100	Heavy creature fBasePoise scale
fBossCreaturePoiseMultiplier	6.0	0.1–100	Boss creature fBasePoise scale
fBossCreatureRegenMultiplier	1.5	0–100	Boss creature poise regeneration scale
fLightCreatureDamageMultiplier	0.6	0–10	Light creature outgoing poise damage scale
fMediumCreatureDamageMultiplier	1.2	0–10	Medium creature outgoing poise damage scale
fHeavyCreatureDamageMultiplier	1.375	0–10	Heavy creature outgoing poise damage scale
fBossCreatureDamageMultiplier	2.25	0–10	Boss creature outgoing poise damage scale
fCreatureStaggerImmunitySeconds	2	0–60	Post-break protection for creature tiers
fSummonPoiseMultiplier	1.5	0.1–100	Maximum poise scale for commanded actors
fNPCMagicPoiseMultiplier	0.6	0–10	Magic poise scale for spells not cast by the player
fBasePoise	40	1–100000	Base maximum poise before bonuses and scaling
fArmorWeightMultiplier	1	0–100	Maximum poise per worn armor-weight unit
fShieldWeightPoiseMultiplier	0	-1–2	Shield weight multiplier for the player, and for non-player actors when the toggle below is false
bApplyShieldWeightToNPCs	true	boolean	Use fArmorWeightMultiplier for non-player shield weight instead
fBaseHitPoiseDamage	8	0–10000	Base poise damage per hit
fWeaponWeightDamageMultiplier	0.5	0–100	Additional damage per weapon-weight unit
fTwoHandedMultiplier	0.85	0–10	Two-handed melee multiplier; lower than 1 because weapon weight and the physical-damage bonus already scale with two-handers
fShieldBashMultiplier	1	0–10	Bash multiplier when a shield is equipped
fWeaponBashMultiplier	0.35	0–10	Bash multiplier for a weapon bash without a shield
fCreatureMultiplier	0.8	0–10	Multiplier for every attack by a creature
fProjectileMultiplier	0.8	0–10	Physical projectile multiplier
fPowerAttackMultiplier	1.5	0–10	Power-attack multiplier
fShieldBlockedPhysicalMultiplier	0.25	0–1	Share of an unblocked hit's poise damage taken while blocking with a shield
fWeaponBlockedPhysicalMultiplier	0.45	0–1	Share of an unblocked hit's poise damage taken while blocking with a weapon only
fUnblockedPhysicalMultiplier	2.5	0–10	Multiplier applied to every physical hit before any block share
fRegenDelaySeconds	3	0–600	Delay following poise damage
fRegenRatePerSecond	8	0–10000	Poise points restored per gameplay second
fStaggerImmunitySeconds	1	0–60	Post-break protection duration
fFleshSpellPoiseMultiplier	0.45	0–100	Maximum poise per active Alteration armor-rating point
fDragonhidePoiseBonus	60	0–100000	Maximum poise bonus for Dragonhide
fPhysicalDamagePoiseMultiplier	0.05	0–0.25	Share of the hit's physical damage added to base poise damage
fPhysicalDamagePoiseCap	6	0–25	Hard cap on that physical-damage bonus
fLightArmorRatingPoiseMultiplier	0.15	0–0.5	Maximum poise per base armor-rating point of worn Light Armor (shields excluded)
fLightArmorRatingPoiseCap	15	0–50	Hard cap on the Light Armor rating bonus
fLightArmorFleshMultiplier	0.75	0–1	Flesh spell and Dragonhide poise scale while wearing Light Armor
fHeavyArmorFleshMultiplier	0.45	0–1	Flesh spell and Dragonhide poise scale while wearing Heavy Armor; takes priority over Light, 0 disables
fMagicBurstPoiseMultiplier	2	0–100	Burst spell magnitude multiplier
fMagicStaggerPoiseMultiplier	10	0–100	Reserved; no stagger contribution is currently available
fConcentrationPoiseMultiplier	1	0–100	Concentration spell magnitude multiplier
fConcentrationIntervalSeconds	1	0.01–60	Concentration pool interval
fConcentrationMaxPoisePerInterval	15	0–10000	Maximum pooled damage per interval
fWardMagicDamageMultiplier	0.35	0–1	Confirmed ward interception multiplier
fShieldBlockedMagicMultiplier	0.4	0–1	Magic poise multiplier while blocking with a shield
fWeaponBlockedMagicMultiplier	0.7	0–1	Magic poise multiplier while blocking with a weapon

[Magic Poise]:

Setting	Default	Range	Meaning
bConcentrationSpellsDealPoise	false	boolean	Enable pooled direct poise damage from concentration spells; their status debuffs apply either way

[Poise Debuffs]:

Key	Default	Range	Meaning
bEnablePoiseDebuffs	true	boolean	Master switch for all poise debuffs
bEnableSappedDebuff, bEnableEnfeebledDebuff, bEnableNumbedDebuff, bEnableHemorrhagingDebuff, bEnableScorchedDebuff	true	boolean	Per-debuff switches
fEnfeebledRegenCeilingMin	0.70	0–1	Recovery ceiling at full poison strength
fEnfeebledRegenCeilingMax	0.85	0–1	Recovery ceiling for the weakest poison
fEnfeebledMagnitudeForMin	10	0.1–1000	Poison magnitude per second that reaches the minimum ceiling
fMinGlobalRegenCeiling	0.60	0–1	No debuff lowers the recovery ceiling below this
fNumbedRegenDelayBonus	1.0	0–10	Extra regen-delay seconds while Numbed
fHemorrhagingRegenRateMultiplier	0.65	0–1	Regen rate multiplier while Hemorrhaging
sHemorrhagingEffectKeywords	bleed,hemorrhag	text	Comma-separated bleed keywords matched against effect IDs and names
fScorchedMeleeDamageMultiplier	1.20	0.5–3	Incoming non-projectile physical poise damage multiplier while Scorched

Hit damage starts as fBaseHitPoiseDamage + weaponWeight * fWeaponWeightDamageMultiplier + min(fPhysicalDamagePoiseCap, physicalDamage * fPhysicalDamagePoiseMultiplier). The relevant attack-category, power-attack, and blocked multipliers are applied afterward. Missing weapon weight uses zero.