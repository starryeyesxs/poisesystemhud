#include "../src/stagger_gate.h"

#include <cassert>
#include <thread>

int main()
{
	assert(!StaggerGate::Allowed());
	{
		StaggerGate::ScopedAllowStagger outer;
		assert(StaggerGate::Allowed());
		{
			StaggerGate::ScopedAllowStagger inner;
			assert(StaggerGate::Allowed());
		}
		assert(StaggerGate::Allowed());
		bool otherThreadAllowed = true;
		std::thread([&] { otherThreadAllowed = StaggerGate::Allowed(); }).join();
		assert(!otherThreadAllowed);
	}
	assert(!StaggerGate::Allowed());

	assert(!StaggerGate::InTimedBash());
	{
		StaggerGate::ScopedTimedBash outer;
		assert(StaggerGate::InTimedBash());
		{
			StaggerGate::ScopedTimedBash inner;
			assert(StaggerGate::InTimedBash());
		}
		assert(StaggerGate::InTimedBash());
		bool otherThread = true;
		std::thread([&] { otherThread = StaggerGate::InTimedBash(); }).join();
		assert(!otherThread);
	}
	assert(!StaggerGate::InTimedBash());

	static_assert(StaggerGate::ShouldSuppress(true, true, false));
	static_assert(!StaggerGate::ShouldSuppress(false, true, false));
	static_assert(!StaggerGate::ShouldSuppress(true, false, false));
	static_assert(!StaggerGate::ShouldSuppress(true, true, true));

	assert(StaggerGate::IsWardBreakEvent("WardBreak"));
	assert(StaggerGate::IsWardBreakEvent("wardbreak"));
	assert(StaggerGate::IsWardBreakEvent("WARDBREAK"));
	assert(!StaggerGate::IsWardBreakEvent("wardAbsorb"));
	assert(!StaggerGate::IsWardBreakEvent("WardBreakStart"));
	assert(!StaggerGate::IsWardBreakEvent("WardBrea"));
	assert(!StaggerGate::IsWardBreakEvent(""));
	assert(!StaggerGate::IsWardBreakEvent(nullptr));

	assert(StaggerGate::IsStaggerStartEvent("staggerStart"));
	assert(StaggerGate::IsStaggerStartEvent("StaggerStart"));
	assert(StaggerGate::IsStaggerStartEvent("STAGGERSTART"));
	assert(!StaggerGate::IsStaggerStartEvent("staggerStop"));
	assert(!StaggerGate::IsStaggerStartEvent("staggerStartLarge"));
	assert(!StaggerGate::IsStaggerStartEvent("staggerStar"));
	assert(!StaggerGate::IsStaggerStartEvent("recoilStart"));
	assert(!StaggerGate::IsStaggerStartEvent("wardBreak"));
	assert(!StaggerGate::IsStaggerStartEvent(""));
	assert(!StaggerGate::IsStaggerStartEvent(nullptr));
}
