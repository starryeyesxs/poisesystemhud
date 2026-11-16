#pragma once

#include <cstdint>

namespace RE
{
	class Actor;
}

namespace ARPGHUD_POISE_API
{
	inline constexpr std::uint32_t kVersion = 1;
	inline constexpr const char* kRegisterExport = "ARPGHUD_RegisterPoiseProvider";
	inline constexpr const char* kUnregisterExport = "ARPGHUD_UnregisterPoiseProvider";

	// Return false when no player resource is available. The caller supplies both outputs.
	using ReadCallback = bool (*)(RE::Actor* player, float* current, float* maximum) noexcept;
	using RegisterProvider = bool (*)(std::uint32_t version, ReadCallback callback) noexcept;
	using UnregisterProvider = bool (*)(ReadCallback callback) noexcept;
}
