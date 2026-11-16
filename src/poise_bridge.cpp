#include "poise_bridge.h"

#include <cmath>
#include <mutex>

namespace
{
	std::mutex providerMutex;
	ARPGHUD_POISE_API::ReadCallback provider = nullptr;
	bool invalidValuesLogged = false;
}

extern "C" DLLEXPORT bool ARPGHUD_RegisterPoiseProvider(
	std::uint32_t version, ARPGHUD_POISE_API::ReadCallback callback) noexcept
{
	if (version != ARPGHUD_POISE_API::kVersion || !callback) {
		logger::error("Rejected poise provider: unsupported API version or null callback.");
		return false;
	}
	std::lock_guard lock(providerMutex);
	if (provider && provider != callback) {
		logger::warn("Rejected poise provider: another provider is already registered.");
		return false;
	}
	provider = callback;
	invalidValuesLogged = false;
	logger::info("Poise provider registered.");
	return true;
}

extern "C" DLLEXPORT bool ARPGHUD_UnregisterPoiseProvider(
	ARPGHUD_POISE_API::ReadCallback callback) noexcept
{
	std::lock_guard lock(providerMutex);
	if (!callback || provider != callback) {
		logger::warn("Rejected poise provider unregistration: callback does not own the registration.");
		return false;
	}
	provider = nullptr;
	invalidValuesLogged = false;
	logger::info("Poise provider unregistered.");
	return true;
}

bool PoiseBridge::Read(RE::Actor* player, Values& values)
{
	values = {};
	if (!player) {
		return false;
	}
	std::lock_guard lock(providerMutex);
	if (!provider || !provider(player, &values.current, &values.maximum)) {
		values = {};
		return false;
	}
	if (!std::isfinite(values.current) || !std::isfinite(values.maximum)) {
		if (!invalidValuesLogged) {
			logger::warn("Poise provider returned non-finite values; hiding bar.");
			invalidValuesLogged = true;
		}
		values = {};
		return false;
	}
	invalidValuesLogged = false;
	return values.maximum > 0.0F;
}
