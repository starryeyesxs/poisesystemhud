# Modern Stagger Lock Interaction Analysis

Modern Stagger Lock (MSL) is compatible with this mod's intentional poise-break stagger path. The quick flinch observed before poise broke was a separate `HitData::pushBack` path, not an MSL animation selected through `StaggerStart`.

## Does MSL Replace or Add Animations?

It does both at the behavior-graph level.

- MSL adds dedicated animation files under `Animations\modernstaggerlock\`, including forward/backward Small, Medium, Large, and Largest staggers, plus special, fall, and land animations.
- It installs `modernstaggerlock_behavior.hkx` into the character master behavior as `StaggerBFR` / `PlayerStaggerBehavior`.
- The custom behavior retains references to some vanilla 1H, 2H, and H2H stagger clips as available behavior paths/fallbacks, while routing stagger selection through MSL's variables.

MSL therefore does not merely rename or directly replace every vanilla stagger `.hkx` file. It adds its own animation set and state machine, then changes the behavior routing used by normal stagger events.

Relevant upstream files:

- [`src/Hooks.cpp`](https://github.com/max-su-2019/ModernStaggerLock/blob/3ce822f8a6b5076d2650f69871a5a111a5d0b773/src/Hooks.cpp)
- [`modernstaggerlock_behavior.hkx`](https://github.com/max-su-2019/Stagger_Lock_Behaviour/blob/8d659612eedd3c6582d830c5e034a24d9ec45dca/Character/behaviors/modernstaggerlock_behavior.hkx)
- [`0_master.hkx`](https://github.com/max-su-2019/Stagger_Lock_Behaviour/blob/8d659612eedd3c6582d830c5e034a24d9ec45dca/Character/behaviors/0_master.hkx)

## How MSL Triggers

MSL hooks player and NPC `NotifyAnimationGraph` calls.

When it receives vanilla `StaggerStart`, it:

1. Reads `staggerDirection`.
2. Reads `staggerMagnitude`.
3. Converts magnitude into `msl_staggerLevel`.
4. Converts direction into a forward/backward graph value.
5. Optionally processes a Precision-provided special stagger.
6. Writes MSL graph variables.
7. Lets the attached behavior graph select the matching MSL animation.

Default magnitude thresholds are:

| Level | Threshold |
| --- | ---: |
| Small | `0.0` |
| Medium | `0.5` |
| Large | `0.75` |
| Largest | `1.0` |

MSL also observes these animation events:

- `MSL_SmallStaggerStart`
- `MSL_MediumStaggerStart`
- `MSL_LargeStaggerStart`
- `MSL_LargestStaggerStart`
- `MSL_SpecialStaggerStart`
- `staggerStop`

Additional MSL features include:

- Disabling jumping while staggering.
- Quick recovery driven by configured animation events.
- Character-state tracking.
- Special stagger falling and landing transitions.

## Does Our Poise Break Use MSL Animations?

Yes, when MSL and its behavior files are installed.

The poise-break path is:

1. A hit consumes poise.
2. If poise reaches zero, this mod creates a `ScopedAllowStagger`.
3. This mod calls Skyrim's directional stagger routine with the calculated poise-break magnitude.
4. The poise suppression gate sees the allow guard and does not suppress the call.
5. Skyrim's stagger routine emits a normal `StaggerStart`.
6. MSL intercepts `StaggerStart` and selects its animation using the engine's stagger magnitude and direction.

MSL does not identify or distinguish this mod's poise break. It treats the resulting engine stagger exactly like another valid stagger event.

The intentional call is in [`src/builtin_poise.cpp`](src/builtin_poise.cpp) near the directional stagger invocation.

## Why the Recent Quick Flinch Was Different

The diagnostic log showed:

```text
HitData::stagger  = 1.150 -> 0.000
HitData::pushBack = 1.000 -> 1.000
ValhallaCombat.dll is in the call chain
```

Before the fix, this mod cleared handled-hit `data->stagger` but left `data->pushBack` unchanged. The original Skyrim hit handler therefore received `pushBack = 1.0`, allowing a quick positional recoil or flinch without necessarily entering the full stagger routine.

MSL's repository does not hook `HitData`, `HitData::pushBack`, or the vanilla hit handler. It reacts to `StaggerStart`. That explains why a pushback-only recoil could occur without a corresponding MSL stagger animation.

## Current Release Behavior

The release now provides:

```ini
bEnableHitPushBack=false
```

For handled poise hits:

- `data->stagger` is cleared.
- When pushback is disabled, `data->pushBack` is also cleared before the original hit handler runs.
- Normal pre-break hits therefore do not produce the Valhalla/vanilla push-flinch.

When a genuine poise break occurs afterward:

- The pushback setting does not disable the intentional stagger call.
- `ScopedAllowStagger` permits that call through suppression.
- Skyrim emits `StaggerStart`.
- MSL receives the event and selects its directional stagger animation.

In short, pushback suppression removes the pre-break micro-flinch while preserving MSL animations for intentional poise breaks.
