#pragma once

#include <cstdint>

namespace RE
{
	class Actor;
	class HitData;
}

namespace StaggerDiagnostic
{
	void Install();

	// One physical hit as seen by the built-in poise hit hook; only filled in diagnostic builds.
	struct HitSample
	{
		RE::Actor* target = nullptr;
		RE::Actor* aggressor = nullptr;
		const RE::HitData* data = nullptr;
		float incomingStagger = 0.0F;
		bool eligible = false;
		bool classified = false;
		int hitType = -1;
		bool handled = false;
		bool broke = false;
		float poiseBefore = -1.0F;
		float poiseAfter = -1.0F;
		float poiseMax = -1.0F;
	};

	void OnHit(const HitSample& sample);
	// Logs the raw HitData damage fields before vanilla processing for any hit involving the player.
	void OnDamageSample(const HitSample& sample);
}
