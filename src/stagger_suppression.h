#pragma once

#include <cstddef>
#include <optional>

namespace RE
{
	class Actor;
}

namespace StaggerSuppression
{
	// Installs the routine gate, the shout-aware StaggerEffect gate, the Disarm whitelist and the ward-break event gate.
		// Must run after BuiltinPoise::Install.
		void Install();

	// Optional logging hook; called for every routine call after the suppression decision.
	using RoutineObserver = void (*)(RE::Actor* target, float magnitude, RE::Actor* aggressor, const void* caller, bool suppressed);
	void SetRoutineObserver(RoutineObserver observer);

	// Index of the IAnimationGraphManagerHolder vtable, read from the live player rather than assumed.
	[[nodiscard]] std::optional<std::size_t> HolderVTableIndex();
}
