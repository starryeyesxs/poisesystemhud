#include <cassert>
#include <cmath>
#include <limits>

namespace logger
{
	template <class... Args> void error(const char*, Args&&...) {}
	template <class... Args> void warn(const char*, Args&&...) {}
	template <class... Args> void info(const char*, Args&&...) {}
}
#define DLLEXPORT
#include "../src/poise_bridge.cpp"

namespace
{
	float current = 25.0F;
	float maximum = 100.0F;
	bool active = true;
	int calls = 0;

	bool Read(RE::Actor*, float* outCurrent, float* outMaximum) noexcept
	{
		++calls;
		*outCurrent = current;
		*outMaximum = maximum;
		return active;
	}

	bool Other(RE::Actor*, float*, float*) noexcept { return false; }
}

int main()
{
	auto* actor = reinterpret_cast<RE::Actor*>(1);
	PoiseBridge::Values values;
	assert(!PoiseBridge::Read(actor, values));
	assert(!ARPGHUD_RegisterPoiseProvider(99, Read));
	assert(!ARPGHUD_RegisterPoiseProvider(ARPGHUD_POISE_API::kVersion, nullptr));
	assert(ARPGHUD_RegisterPoiseProvider(ARPGHUD_POISE_API::kVersion, Read));
	assert(!ARPGHUD_RegisterPoiseProvider(ARPGHUD_POISE_API::kVersion, Other));
	assert(!PoiseBridge::Read(nullptr, values));
	assert(calls == 0);
	assert(PoiseBridge::Read(actor, values));
	assert(values.current == 25.0F && values.maximum == 100.0F);
	active = false;
	assert(!PoiseBridge::Read(actor, values));
	assert(values.current == 0.0F && values.maximum == 0.0F);
	active = true;
	maximum = 0;
	assert(!PoiseBridge::Read(actor, values));
	maximum = std::numeric_limits<float>::quiet_NaN();
	assert(!PoiseBridge::Read(actor, values));
	assert(values.current == 0.0F && values.maximum == 0.0F);
	maximum = 100;
	current = std::numeric_limits<float>::infinity();
	assert(!PoiseBridge::Read(actor, values));
	assert(!ARPGHUD_UnregisterPoiseProvider(Other));
	assert(ARPGHUD_UnregisterPoiseProvider(Read));
	assert(!PoiseBridge::Read(actor, values));
	assert(ARPGHUD_RegisterPoiseProvider(ARPGHUD_POISE_API::kVersion, Other));
	assert(ARPGHUD_UnregisterPoiseProvider(Other));
}
