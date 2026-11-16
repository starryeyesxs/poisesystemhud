#pragma once

#include "builtin_poise_state.h"
#include "builtin_poise_telemetry.h"
#include "poise_bridge.h"

#include <string>

namespace BuiltinPoise
{
	void Install();
	void Configure(const Settings& settings);
	void BeginGame();
	void EndGame();
	[[nodiscard]] bool Read(PoiseBridge::Values& values);
	// Live dev-overlay data. Recording is skipped entirely while telemetry is disabled.
	void SetTelemetryEnabled(bool enabled);
	[[nodiscard]] bool TelemetryEnabled();
	// Player poise, the last-hit target (kept for 15 seconds while telemetry is on), the equipped weapon benchmark
	// and the recent hit log.
	[[nodiscard]] bool ReadTelemetry(TelemetrySnapshot& snapshot);
	// Rescans the last-hit target's active effects so the telemetry card shows live debuffs rather than the values
	// from the hit or the last 0.5 second refresh. Call on the game/UI task before ReadTelemetry; no-op while telemetry
	// is off.
	void RefreshTelemetryTarget();
	// Lightweight player debuff state for the Minimalist HUD icons; false when poise or debuffs are off.
	[[nodiscard]] bool ReadPlayerDebuffs(PlayerDebuffView& view);
	// Reactive Health/Magicka/Stamina bar effects. While enabled, the player's effect list is scanned even when the
	// poise debuffs are off; a hostile shock hit on the player counts for 1.5 seconds.
	void SetReactiveBarsEnabled(bool enabled);
	void NoteShockHit();
	[[nodiscard]] bool ReadReactiveBars(ReactiveBarView& view);
	// The most recently hit tracked NPC or creature; false once it has been idle for 4 seconds or is untracked.
	struct TargetValues
	{
		std::string name;
		std::string tier;
		float current = 0.0F;
		float maximum = 0.0F;
		bool immune = false;
	};
	[[nodiscard]] bool ReadTarget(TargetValues& values);
	[[nodiscard]] bool IsInstalled();
	// Lock-free; true when vanilla staggers on this actor should be dropped (poise-managed, not whitelisted).
	[[nodiscard]] bool ShouldSuppressStagger(RE::Actor* actor);
	// "race=<editor ID> tier=<tier>" for diagnostics; takes no locks.
	[[nodiscard]] std::string DescribeActorClass(RE::Actor* actor);
#ifdef ARPGHUD_MAGIC_GAMEPLAY
	// A ward that already mitigated the hit takes precedence over blocking, so the two never stack.
	// The optional breakdown describes how the damage was derived, for the telemetry hit log.
	void ApplyMagicDamage(RE::Actor* target, RE::Actor* attacker, float damage, bool wardMitigated = false,
		const HitBreakdown* breakdown = nullptr);
	// A player spell touched this NPC or creature but deals no direct poise damage (channeled spells): still track it,
	// scan its debuffs and select it as the last target so Flames and Frostbite apply Scorched and Numbed.
	void NoteMagicTarget(RE::Actor* target, RE::Actor* attacker, bool absorb = false);
	[[nodiscard]] std::uint64_t Generation();
	[[nodiscard]] bool IsGenerationActive(std::uint64_t generation);
	[[nodiscard]] Settings GetSettings();
#endif
}
