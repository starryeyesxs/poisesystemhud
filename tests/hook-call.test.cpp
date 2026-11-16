#include "../src/hook_call.h"

#include <cassert>
#include <cstdint>
#include <cstring>

int main()
{
	auto* memory = static_cast<std::uint8_t*>(
		VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
	assert(memory);
	auto* site = memory + 32;
	site[0] = 0xE8;
	auto setTarget = [&](std::uint8_t* target) {
		const auto delta = static_cast<std::int32_t>(target - (site + 5));
		std::memcpy(site + 1, &delta, sizeof(delta));
	};
	setTarget(memory + 128);
	assert(HookCall::ExecutableTarget(reinterpret_cast<std::uintptr_t>(site)) ==
		reinterpret_cast<std::uintptr_t>(memory + 128));
	setTarget(memory + 8);
	assert(HookCall::ExecutableTarget(reinterpret_cast<std::uintptr_t>(site)) ==
		reinterpret_cast<std::uintptr_t>(memory + 8));
	site[0] = 0xE9;
	assert(HookCall::ExecutableTarget(reinterpret_cast<std::uintptr_t>(site)) == 0);
	site[0] = 0xE8;
	DWORD oldProtection = 0;
	assert(VirtualProtect(memory, 4096, PAGE_READWRITE, &oldProtection));
	assert(HookCall::ExecutableTarget(reinterpret_cast<std::uintptr_t>(site)) == 0);
	assert(VirtualFree(memory, 0, MEM_RELEASE));
}
