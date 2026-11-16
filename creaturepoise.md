# Design Specification: Creature Poise System (4-Tier Archetype Buckets)

## Overview
Extend built-in poise tracking and Stage 1 vanilla stagger suppression (`bSuppressVanillaStaggers`) to non-`ActorTypeNPC` creatures using 4 predictable keyword/race archetype tiers, while fixing outgoing creature attack classification so all creature hits properly apply `settings.creatureMultiplier` (`fCreatureMultiplier`).

## Settings & `[Poise]` INI Keys
- `settings.enableCreaturePoise` <- `bEnableCreaturePoise = true` *(Master toggle for creature poise pools & creature stagger suppression)*
- `settings.lightCreaturePoiseMultiplier` <- `fLightCreaturePoiseMultiplier = 0.75` *(Tier 1: ~30 Max Poise at 40 base)*
- `settings.mediumCreaturePoiseMultiplier` <- `fMediumCreaturePoiseMultiplier = 1.35` *(Tier 2: ~54 Max Poise at 40 base)*
- `settings.heavyCreaturePoiseMultiplier` <- `fHeavyCreaturePoiseMultiplier = 2.75` *(Tier 3: ~110 Max Poise at 40 base)*
- `settings.bossCreaturePoiseMultiplier` <- `fBossCreaturePoiseMultiplier = 6.0` *(Tier 4: ~240 Max Poise at 40 base)*
- `settings.bossCreatureRegenMultiplier` <- `fBossCreatureRegenMultiplier = 1.5` *(Boss-tier creatures recover poise 50% faster so players must chain heavy hits rather than kiting indefinitely)*

## 1. Creature Archetype Classification (Keyword & Race Buckets)
Evaluate non-`ActorTypeNPC` living actors into one of 4 tiers (defaulting to `1.0x` `settings.basePoise` for unknown custom mod creatures):

1. **Tier 1 — Light Beasts & Critters (`0.75x` `fBasePoise`):**
   - Wolves, Ice Wolves, Skeevers, Mudcrabs, Foxes, Horkers (or small beasts).
   - *Exclusion:* Tiny non-combat ambient critters (Rabbits, Chickens, Slaughterfish, Insects) can be skipped from active poise tracking.
2. **Tier 2 — Standard Undead & Medium Creatures (`1.35x` `fBasePoise`):**
   - Draugr, Skeletons, Falmer, Chaurus, Sabre Cats, Frostbite Spiders, Ice Wraiths, Spriggans.
3. **Tier 3 — Heavy Brutes & Constructs (`2.75x` `fBasePoise`):**
   - Bears, Trolls, Hagravens, Gargoyles, Dwarven Spheres, Chaurus Hunters, Werewolves.
4. **Tier 4 — Bosses & Colossi (`6.0x` `fBasePoise` + `1.5x` Regen Rate):**
   - Dragons (`ActorTypeDragon`), Giants, Mammoths, Dwarven Centurions, Lurkers, Dragon Priests.

## 2. Outgoing Creature Hit Fix (`fCreatureMultiplier`)
- Ensure all attacks originating from non-`ActorTypeNPC` creatures (including Ice Wolves and variant creature races) properly classify as creature hits and multiply incoming poise damage by `settings.creatureMultiplier` (`fCreatureMultiplier`), rather than falling back to unmultiplied or invalid weapon-weight paths.
- Consider tuning default `fCreatureMultiplier` down from `1.2` to `0.75`–`0.85` so fast multi-hit creatures (like Wolves and Sabre Cats) don't instantly shred player poise in two bites.

## 3. Stagger Suppression & `directionalStagger` Integration
- When `settings.enableCreaturePoise` and `settings.suppressVanillaStaggers` are active, include tracked creatures in `ShouldSuppressStagger` so vanilla light hits do not stunlock heavy creatures (like Trolls or Bears).
- Verify `directionalStagger` magnitude on massive creatures (Dragons, Giants, Centurions) so breaking a Tier 4 creature's poise pool passes sufficient magnitude to trigger their native stagger/ground-crash animation.

## 4. UI & Configuration
- Expose `bEnableCreaturePoise` and the tier multipliers under `[Poise]` in `arpghud.ini` and the **Combat Balance** menu (included in Poise Reset defaults and live F10 reload).
