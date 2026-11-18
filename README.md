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

* **Layout & Visibility:** The HUD hides in menus and loading screens, presenting resources (Health, Magicka, Stamina, poise) in a minimalist cluster that can be repositioned via UI settings or `.ini` options (`iSideOffsetLeft`, `iBottomOffset`).
* **Dynamic Fading:** Resource bars fade out when you are idle with full stats, and reappear during combat, weapon/spell drawing, sprinting, or low resources. Weather and sneak indicators remain visible independently.
* **Widgets & Light Gem:** The weather/location widget displays indoor/outdoor data and Fourth Era calendar dates. A sneak light gem appears while crouching, though readings depend on compatibility and hide if unavailable.
* **Poise Display:** The yellow poise bar shows built-in or optional provider data (or hides if neither is active), with an optional toggle for exact numerical values.

### Target poise bar

* **Overview:** **Show Target Poise Bar** (off by default) pins to the upper-right corner to display the name, tier, and poise of the last-hit tracked NPC or creature.
* **Behavior:** It stays fixed in the corner (not tracking the crosshair), dims during post-break immunity windows, and automatically fades away four seconds after your last hit.

## Poise gameplay

### Built-in player poise

* **Base Poise & Armor:** Enabled via `bEnableBuiltInPoise=true`, your maximum poise starts at the base value and scales with equipped light/heavy armor weight (`fArmorWeightMultiplier`, default 1.0). Light Armor also grants bonus poise based on its base armor rating.
* **Shield Weight:** Player shield weight uses `fShieldWeightPoiseMultiplier` (default 0.0, range -1.0 to 2.0), while NPCs use their normal armor multiplier by default (`bApplyShieldWeightToNPCs=true`). Negative shield multipliers reduce maximum poise, though total poise cannot drop below 10 after summon scaling.
* **Flesh Spells & Dragonhide:** Active Alteration armor spells and Dragonhide add bonus poise, scaled by worn armor class (highest when unarmored, reduced in Light Armor, and lowest in Heavy Armor).
* **Combat & Regeneration:** Physical hits deplete poise; reaching zero triggers a directional stagger, refills poise, and starts a brief grace period against further poise damage. Poise regenerates after a short delay. (Scripted reactions, knockdowns, and other vanilla engine reactions are not necessarily suppressed.)
* **NPCs & Unblocked Hits:** `bEnableNPCPoise=true` enables poise for eligible loaded, living NPCs. Unblocked physical hits multiply poise damage by `fUnblockedPhysicalMultiplier=2.5` (ward spells alone do not count as physical blocking).
* **Magic & Wards:** Elemental spell impacts damage poise scaled by elemental and magic resistance. Channeled concentration spells (*Flames*, *Frostbite*, *Sparks*) apply status debuffs without dealing direct poise damage unless **Concentration Spells Deal Poise** is enabled (which pools and caps their damage). Magic wards intercept spell poise damage (`fWardMagicDamageMultiplier=0.35`) with no extra magicka cost, taking precedence over shield (`0.4x`) or weapon (`0.7x`) magic blocking. Non-player spells scale by `fNPCMagicPoiseMultiplier` (0.6).
* **Stagger Suppression:** By default, **Suppress Vanilla Staggers** replaces ordinary hit, bash, and ward-break staggers with poise-break staggers on managed actors. **Allow Timed Bash Staggers** (off by default) lets timed-bash staggers through, and shouts like *Unrelenting Force* always retain their vanilla stagger.


### Creature poise

Enabling `bEnableCreaturePoise` assigns poise pools to creatures based on their archetype tier, where worn armor and spell bonuses stack on top of scaled base values:

* **Tiers & Multipliers:** Creatures are categorized into Light (`0.75x`), Medium (`1.7x`), Heavy (`3.75x`), and Boss (`6.0x`) tiers (e.g., Skeevers and Wolves in Light; Draugr, Falmer, and Sabre Cats in Medium; Trolls and Bears in Heavy; Dragons and Giants in Boss).
* **Mechanics & Combat Rules:** Boss creatures regenerate poise `1.5x` faster and trigger full native staggers upon breaking. To prevent stunlocking, creatures receive a `2.0`-second stagger immunity window after a poise break. Small ambient fauna are untracked, while summoned allies receive `1.5x` max poise.
* **Outgoing Damage & Disabling:** Creature damage scales via base and tier multipliers, ranging from `0.48` for light creatures up to `1.8` for boss tiers. Disabling the system reverts creatures to vanilla behavior.

### Blocking

Blocking mitigates incoming physical damage depending on the gear used, applying an unblocked multiplier first before factoring in the block reduction:

* **Shields:** Equipping a shield reduces incoming physical damage to `25%` (`fShieldBlockedPhysicalMultiplier = 0.25`).
* **Weapons:** Blocking with a weapon alone reduces incoming physical damage to `45%` (`fWeaponBlockedPhysicalMultiplier = 0.45`).

### Poise status debuffs

Hostile damage-over-time and drain effects interact with the poise system rather than dealing direct poise damage, affecting both the player and tracked NPCs while ignoring self-casts (aside from combat-hit bleed perks) and failing to stack with identical effects.

* **Sapped (Absorb Effects):** Caused by absorb or drain effects, halting poise recovery for ~2 seconds on weapon hits.
* **Enfeebled (Poisons):** Triggered by hostile poisons, capping poise recovery between configured ceiling limits.
* **Numbed (Lingering Frost):** Caused by lingering frost damage, adding a delay bonus to post-hit poise regeneration.
* **Hemorrhaging (Bleed Effects):** Triggered by axe bleeds and compatible mods, multiplying the poise recovery rate.
* **Scorched (Lingering Fire):** Caused by lingering fire damage, multiplying incoming non-projectile physical poise damage.

Debuffs can be toggled globally or individually in the configuration (`[Poise Debuffs]`), with HUD icon strips and Enfeebled markers manageable via `[HUD]` settings and applied using **F10**.

### Reactive resource bar effects

When `bShowReactiveBarEffects=true` is enabled in `[HUD]`, hostile effects dynamically tint the Minimalist HUD resource bars independently of poise debuff settings:

* **Health Bar:** Turns toxic green from poisons, violet from Sapped, pulses crimson from bleeding, and gains an ember glow from lingering fire.
* **Magicka & Health Bars:** Crackles with electric blue for 1.5 seconds upon shock hits.
* **Stamina Bar:** Chilled by lingering frost effects.

These visual effects clear automatically upon reload (**F10**) or when disabled.

## Compatibility and troubleshooting

* **Version & Frameworks:** Validated for **Skyrim 1.7.104.0** and compatible with major overhauls like Valhalla Combat, BFCO, and TrueHUD. It auto-disables on version mismatches or if Chocolate Poise is present. Disable built-in poise (`bEnableBuiltInPoise=false`) if running a separate poise mod.
* **Diagnostics & Setup:** Settings refresh via **F10** during gameplay. Check `poisesystemhud.log` in your SKSE directory for errors, and use **Show Live Poise Telemetry** or the `showpoisedev` console command for real-time math testing.

## Settings reference

Configure options via the **SKSE Menu Framework** or consult [POISE_GUIDE.txt](./POISE_GUIDE.txt) for detailed INI guidelines.

### HUD and appearance (`[HUD]`)

* **Widgets & Layout:** Pick the HUD theme (`sHudTheme`: `Daedric Iron` by default, or `Minimalist`), toggle the weather/location widget (`bEnableWeatherWidget`), sneak light gem (`bEnableSneakLightGem`), customize screen offsets (`iSideOffsetLeft`, `iBottomOffset`) and the uniform HUD scale (`fHudScale`, 0.70 to 1.50, default 1.00).
* **Poise Displays:** Control player and target poise bars (`bShowPoiseBar`, `bShowTargetPoiseBar`), numerical values (`bShowPoiseNumbers`), debuff icons (`bShowPoiseDebuffIcons`), reactive status tinting (`bShowReactiveBarEffects`), and live telemetry (`bShowPoiseTelemetry`).
* **Visibility & Fading:** Configure dynamic fading (`bDynamicHUD`, `bAlwaysOn`), idle opacity (`fIdleOpacity`), and fade delay (`fFadeDelaySeconds`).

### Minimalist graphics (`[MinimalistGraphics]`)

* **Visual Polish:** Adjust decorative filigree (`bMinBarFiligree`), bar shine (`fMinBarShine`), shadow depth (`fMinBarShadow`), and quarter-mark notches (`bMinBarNotches`).

### Poise settings reference summary

The `[Poise]` configuration covers core gameplay toggles (`bEnableBuiltInPoise`, `bEnableNPCPoise`, `bSuppressVanillaStaggers`), creature multipliers for base poise and damage scaling, armor and shield weight adjustments, hit damage formulas, and regeneration parameters.

* **Core & Creatures:** Includes multipliers and stagger immunities for light, medium, heavy, and boss creatures, plus summon and magic poise adjustments.
* **Weights & Combat:** Configures base hit damage, weapon/shield weight scaling, two-handed bonuses, power-attack multipliers, block mitigation shares, and unblocked physical multipliers.
* **Regeneration & Armor:** Controls poise regeneration delay and rate, flesh spell multipliers (e.g., Dragonhide), physical damage caps, and light/heavy armor rating scaling.

#### `[Magic Poise]`

* `bConcentrationSpellsDealPoise`: Toggles pooled direct poise damage from concentration spells (`false` by default).

#### `[Poise Debuffs]`

* Controls the master switch (`bEnablePoiseDebuffs`) and individual debuff toggles (`Sapped`, `Enfeebled`, `Numbed`, `Hemorrhaging`, `Scorched`), along with their specific recovery ceilings, delay bonuses, rate multipliers, and damage scaling keywords.
