# Phase 1: Foundation & Build Configuration — Exact Changes Required

**Estimated Time:** 2–3 hours  
**Goal:** Integrate SKSE Menu Framework headers and build infrastructure; verify clean compilation

---

## 📝 FILES TO CREATE

### 1. `src/SKSEMenuFramework.h` (Copy from Official Repo)

**Source:** https://raw.githubusercontent.com/QTR-Modding/SKSE-Menu-Framework-3/master/resources/SKSEMenuFramework.h

**Action:** Download this file and save to `src/SKSEMenuFramework.h`

**Purpose:** Official single-header API for SKSE Menu Framework. Contains:
- Dynamic module loading via `GetMenuFrameworkModule()`
- Soft dependency check via `SKSEMenuFramework::IsInstalled()`
- All function pointer type definitions
- ImGui integration types

---

### 2. `src/menu_framework.h` (New Wrapper Header)

**Create:** `src/menu_framework.h`

**Content:**
```cpp
#pragma once

#ifdef ARPGHUD_ENABLE_MENU
#include "SKSEMenuFramework.h"
#else
// Stub for when menu is disabled
#endif

namespace MenuFramework
{
	bool IsAvailable();
	bool Initialize();
	void Shutdown();
	
	// Will be expanded in Phase 2-4
	// void RenderMenu();
	// void OnSettingChanged(const std::string& key, float value);
}
```

**Purpose:** Abstraction layer for menu framework. Allows:
- Conditional compilation (ARPGHUD_ENABLE_MENU flag)
- Centralized initialization logic
- Clear namespace for menu code

---

### 3. `src/menu_framework.cpp` (New Implementation - Stub)

**Create:** `src/menu_framework.cpp`

**Content:**
```cpp
#include "menu_framework.h"

#ifdef ARPGHUD_ENABLE_MENU

namespace MenuFramework
{
	static SKSEMenuFramework::Model::WindowInterface* menuWindow = nullptr;

	bool IsAvailable()
	{
		return SKSEMenuFramework::IsInstalled();
	}

	bool Initialize()
	{
		if (!IsAvailable()) {
			logger::info("SKSE Menu Framework not installed; in-game menu disabled.");
			return false;
		}

		logger::info("SKSE Menu Framework is available; initializing mod menu...");
		
		// TODO: Phase 2 - Register menu window
		// Stub for now
		
		logger::info("Menu framework initialization stub complete.");
		return true;
	}

	void Shutdown()
	{
		menuWindow = nullptr;
	}
}

#else

namespace MenuFramework
{
	bool IsAvailable() { return false; }
	bool Initialize() { return true; }  // No-op when disabled
	void Shutdown() {}
}

#endif
```

**Purpose:** Stub implementation that will be expanded in later phases. Currently:
- Checks if framework is installed
- Logs framework availability
- Gracefully no-ops if disabled or framework absent

---

## ✏️ FILES TO MODIFY

### 1. `xmake.lua` (Add Menu Configuration Flag)

**Location:** After `set_project()` and before `set_languages()`

**Change:** Add menu build flag
```lua
-- set minimum xmake version
set_xmakever("3.0.0")

includes("lib/commonlibsse-ng")

local PLUGIN_NAME = "arpghud"
local PLUGIN_VERSION = "1.2.0"

local VIEW_NAME = "arpghud"

set_project(PLUGIN_NAME)
set_version(PLUGIN_VERSION)
set_license("GPL-3.0")

set_languages("c++23")
set_warnings("allextra")
add_defines("UNICODE", "_UNICODE")

-- Menu Framework Integration (enabled by default)
add_defines("ARPGHUD_ENABLE_MENU=1")  -- ← ADD THIS LINE

set_policy("package.requires_lock", true)

add_rules("mode.release")
```

**Purpose:** Enable conditional compilation of menu code

---

### 2. `src/pch.h` (Conditional Include)

**Location:** After existing includes, before namespace declarations

**Current:**
```cpp
#pragma once

#pragma warning(push)
#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>

#include <fstream>

#ifdef NDEBUG
#	include <spdlog/sinks/basic_file_sink.h>
#else
#	include <spdlog/sinks/msvc_sink.h>
#endif
#pragma warning(pop)
```

**Change To:**
```cpp
#pragma once

#pragma warning(push)
#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>

#include <fstream>

#ifdef NDEBUG
#	include <spdlog/sinks/basic_file_sink.h>
#else
#	include <spdlog/sinks/msvc_sink.h>
#endif

// Menu Framework (optional; soft dependency)
#ifdef ARPGHUD_ENABLE_MENU
#	include <filesystem>  // Required by SKSEMenuFramework.h for IsInstalled()
#endif

#pragma warning(pop)
```

**Purpose:** Include necessary headers for menu framework when enabled. `<filesystem>` is needed by the official header's `IsInstalled()` function.

---

### 3. `src/main.cpp` (Add Menu Initialization Call)

**Location:** In `SKSEMessageHandler`, after PrismaUI initialization in `kDataLoaded` case

**Current (around line ~940):**
```cpp
	case SKSE::MessagingInterface::kDataLoaded: {
		LoadSettings();
		BuiltinPoise::Install();
#ifdef ARPGHUD_MAGIC_GAMEPLAY
		MagicGameplay::Install();
#endif
#ifdef ARPGHUD_MAGIC_DIAGNOSTICS
		MagicDiagnostic::Install();
#endif
		logger::info("PrismaUI RequestPluginAPI starting");
		prismaUI = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>();
		logger::info("PrismaUI RequestPluginAPI completed: {}", prismaUI != nullptr);
		if (!prismaUI) {
			logger::critical("PrismaUI API is unavailable. The HUD view cannot be created.");
			return;
		}
		// ... rest of initialization ...
```

**Change To (insert after PrismaUI check):**
```cpp
	case SKSE::MessagingInterface::kDataLoaded: {
		LoadSettings();
		BuiltinPoise::Install();
#ifdef ARPGHUD_MAGIC_GAMEPLAY
		MagicGameplay::Install();
#endif
#ifdef ARPGHUD_MAGIC_DIAGNOSTICS
		MagicDiagnostic::Install();
#endif
		logger::info("PrismaUI RequestPluginAPI starting");
		prismaUI = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>();
		logger::info("PrismaUI RequestPluginAPI completed: {}", prismaUI != nullptr);
		if (!prismaUI) {
			logger::critical("PrismaUI API is unavailable. The HUD view cannot be created.");
			return;
		}

		// Initialize SKSE Menu Framework (soft dependency)
#ifdef ARPGHUD_ENABLE_MENU
		if (!MenuFramework::Initialize()) {
			logger::warn("SKSE Menu Framework initialization failed; continuing without menu.");
		}
#endif

		taskInterface = SKSE::GetTaskInterface();
		// ... rest of initialization ...
```

**Also add include near top of main.cpp:**
```cpp
#include "menu_framework.h"
```

**Purpose:** Call menu framework initialization during plugin startup; gracefully handle failures

---

### 4. `src/main.cpp` (Add Shutdown in Plugin Unload)

**Location:** Add at end of file, after `SKSEPlugin_Load` function

**Add:**
```cpp
extern "C" DLLEXPORT void SKSEPlugin_Unload(const SKSE::UnloadInterface* a_unload)
{
#ifdef ARPGHUD_ENABLE_MENU
	MenuFramework::Shutdown();
#endif
}
```

**Purpose:** Clean up menu framework resources on plugin unload (best practice)

---

## 📋 BUILD VERIFICATION CHECKLIST

After making changes, verify:

1. **Clean rebuild:**
   ```powershell
   xmake clean
   xmake -y
   ```
   - Should complete without errors
   - No undefined reference errors for menu code
   - No header inclusion cycles

2. **Build output inspection:**
   - Check `build/windows/x64/release/arpghud.pdb` size (should be similar to before)
   - No unexpected large size increase (indicates overly large template expansion)

3. **Runtime check (optional, if test environment available):**
   - Plugin loads cleanly
   - Check `arpghud.log` for menu initialization message:
     ```
     SKSE Menu Framework not installed; in-game menu disabled.
     ```
     OR
     ```
     SKSE Menu Framework is available; initializing mod menu...
     Menu framework initialization stub complete.
     ```

---

## 🎯 Success Criteria for Phase 1

✅ All three new files created without errors  
✅ All four existing files modified correctly  
✅ `xmake -y` compiles cleanly (no warnings or errors)  
✅ DLL/PDB generated successfully  
✅ Plugin loads without crashes  
✅ Log shows menu framework availability message  
✅ No regression in existing HUD functionality  

---

## 📝 Summary of Changes

| File | Action | Lines Changed | Purpose |
|------|--------|---------------|---------|
| `src/SKSEMenuFramework.h` | **CREATE** | ~2000 | Official API header |
| `src/menu_framework.h` | **CREATE** | ~20 | Wrapper abstraction |
| `src/menu_framework.cpp` | **CREATE** | ~40 | Initialization stub |
| `xmake.lua` | **MODIFY** | +1 | Add build flag |
| `src/pch.h` | **MODIFY** | +4 | Conditional includes |
| `src/main.cpp` | **MODIFY** | +15 | Call init/shutdown |
| `README.md` | *(deferred)* | — | Phase 5 only |
| `arpghud.ini` | *(unchanged)* | — | No changes needed |

**Total New Code:** ~2060 lines (mostly official header)  
**Total Modified:** ~20 lines of actual implementation

---

## ⏭️ Next Steps After Phase 1

Once Phase 1 is complete:
1. Proceed to Phase 2 (Dynamic Binding & Menu Registration)
2. Implement actual menu window registration
3. Create render callback stub
4. Test menu window appears in SKSE Menu Framework UI (if framework installed)

---
