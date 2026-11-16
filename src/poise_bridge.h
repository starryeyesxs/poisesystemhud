#pragma once

#include "PoiseBridgeAPI.h"

namespace PoiseBridge
{
	struct Values
	{
		float current = 0.0F;
		float maximum = 0.0F;
	};

	[[nodiscard]] bool Read(RE::Actor* player, Values& values);
}
