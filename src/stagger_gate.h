#pragma once

#include <cstddef>

namespace StaggerGate
{
	inline thread_local int allowDepth = 0;

	// Marks staggers triggered by this plugin (or whitelisted effects) so a vanilla-stagger gate can tell them apart.
	class ScopedAllowStagger
	{
	public:
		ScopedAllowStagger() noexcept { ++allowDepth; }
		~ScopedAllowStagger() noexcept { --allowDepth; }
		ScopedAllowStagger(const ScopedAllowStagger&) = delete;
		ScopedAllowStagger& operator=(const ScopedAllowStagger&) = delete;
	};

	[[nodiscard]] inline bool Allowed() noexcept { return allowDepth > 0; }

	inline thread_local int timedBashDepth = 0;

	// Marks the vanilla hit handler call for a timed bash, so users who prefer vanilla's interrupt can let its stagger through.
	class ScopedTimedBash
	{
	public:
		ScopedTimedBash() noexcept { ++timedBashDepth; }
		~ScopedTimedBash() noexcept { --timedBashDepth; }
		ScopedTimedBash(const ScopedTimedBash&) = delete;
		ScopedTimedBash& operator=(const ScopedTimedBash&) = delete;
	};

	[[nodiscard]] inline bool InTimedBash() noexcept { return timedBashDepth > 0; }

	[[nodiscard]] constexpr bool ShouldSuppress(bool enabled, bool managed, bool allowed) noexcept
	{
		return enabled && managed && !allowed;
	}

	// expected must be lower case.
	template <std::size_t N>
	[[nodiscard]] inline bool EqualsIgnoreCase(const char* name, const char (&expected)[N]) noexcept
	{
		if (!name) {
			return false;
		}
		for (std::size_t i = 0; i < N - 1; ++i) {
			const char c = name[i];
			if (c == '\0' || (c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c) != expected[i]) {
				return false;
			}
		}
		return name[N - 1] == '\0';
	}

	// The vanilla ward break reaches the behavior graph as this event rather than through the stagger routine.
	[[nodiscard]] inline bool IsWardBreakEvent(const char* name) noexcept
	{
		return EqualsIgnoreCase(name, "wardbreak");
	}

	// A timed bash makes the engine send this event straight to the graph, bypassing the stagger routine.
	[[nodiscard]] inline bool IsStaggerStartEvent(const char* name) noexcept
	{
		return EqualsIgnoreCase(name, "staggerstart");
	}
}