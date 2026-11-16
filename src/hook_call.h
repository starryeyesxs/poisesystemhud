#pragma once

#include <Windows.h>
#include <cstdint>
#include <cstring>

namespace HookCall
{
	inline std::uintptr_t ExecutableTarget(std::uintptr_t site)
	{
		if (*reinterpret_cast<const std::uint8_t*>(site) != 0xE8) {
			return 0;
		}
		std::int32_t displacement = 0;
		std::memcpy(&displacement, reinterpret_cast<const void*>(site + 1), sizeof(displacement));
		const auto target = site + 5 + static_cast<std::int64_t>(displacement);
		MEMORY_BASIC_INFORMATION memory{};
		if (VirtualQuery(reinterpret_cast<const void*>(target), &memory, sizeof(memory)) != sizeof(memory) ||
			memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
			return 0;
		}
		const auto protection = memory.Protect & 0xFF;
		if (protection != PAGE_EXECUTE && protection != PAGE_EXECUTE_READ &&
			protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY) {
			return 0;
		}
		return target;
	}
}
