# Prisma UI SKSE Plugin - Project Setup Complete ✅

> Historical template setup: the current plugin is `poisesystemhud`. See [README.md](README.md)
> for its current build output, view path, and install instructions.

## Overview
The Prisma UI C++ SKSE plugin template has been successfully cloned and configured for Skyrim SE (CommonLibSSE-NG) development. The project is ready to integrate your custom HUD HTML/CSS/JS files.

## Project Structure

```
poetstylehudforskyrim/
├── src/                          # C++ Plugin Source Code
│   ├── main.cpp                  # Plugin entry point & PrismaUI initialization
│   ├── pch.h                     # Precompiled header (RE/Skyrim, SKSE, REL)
│   ├── PrismaUI_API.h            # PrismaUI API interface declarations
│   └── keyhandler/
│       └── keyhandler.cpp        # Input handling (F3 toggle example)
│
├── view/                         # HTML/CSS/JavaScript UI Files
│   └── index.html                # Template UI (to be customized)
│
├── lib/
│   └── commonlibsse-ng/          # CommonLibSSE-NG dependency (submodule)
│       ├── include/              # Skyrim & SKSE headers
│       ├── src/                  # CommonLibSSE-NG source
│       └── extern/
│           └── openvr/           # OpenVR support (VR plugin compatibility)
│
├── xmake.lua                     # XMake build configuration
├── xmake-requires.lock           # Locked dependency versions
├── build/                        # Build output directory (generated)
├── .xmake/                       # XMake cache & config (generated)
└── README.md                     # Official template documentation
```

## Key Configuration Details

### Build System: XMake
- **Version Required**: 3.0.0+ (Installed: v3.1.2+HEAD.5d21e9daf) ✅
- **Build Output**: `build/windows/x64/release/`
- **Plugin Name**: PrismaUI-Example-Plugin
- **Plugin Version**: 1.5.0
- **Language**: C++23
- **Compiler**: MSVC 2026 (Visual Studio Community 2026) ✅

### Dependencies
1. **CommonLibSSE-NG** (via submodule)
   - Status: ✅ Initialized and checked out (commit: c5424463...)
   - Provides Skyrim/SKSE headers and base classes
   - Branch: `ng` (Next Generation)

2. **OpenVR** (nested submodule)
   - Status: ✅ Initialized
   - Optional VR plugin support

### Plugin Configuration (xmake.lua)
```lua
set_project("PrismaUI-Example-Plugin")
set_version("1.5.0")
set_languages("c++23")
add_defines("UNICODE", "_UNICODE")
add_rules("mode.release")
set_policy("package.requires_lock", true)
```

### Build Distribution
After build, creates plugin distribution at:
```
dist/PrismaUI-Example-Plugin_1.5.0/
├── SKSE/plugins/
│   ├── PrismaUI-Example-Plugin.dll
│   └── PrismaUI-Example-Plugin.pdb
└── PrismaUI/views/PrismaUI-Example-UI/
    └── index.html
```

## Next Steps: Integrating Your Custom HUD

### 1. Customize the HTML/CSS/JavaScript UI
Replace [view/index.html](view/index.html) with your custom HUD files:
- HTML structure for your HUD elements
- CSS styling (make sure to use relative positioning for in-game overlay)
- JavaScript functions callable from C++ via `PrismaUI->Invoke(view, "functionName(args)")`

### 2. Extend the C++ Plugin
Modify [src/main.cpp](src/main.cpp) to:
- Handle your custom JavaScript listeners with `PrismaUI->RegisterJSListener()`
- Implement game event handlers (actor updates, combat state, etc.)
- Call your JS functions to update the HUD display

Example pattern:
```cpp
// Listen for data from JS
PrismaUI->RegisterJSListener(view, "yourCustomHandler", [](const char* data) {
    // Handle data from your HUD
});

// Send data to JS
PrismaUI->Invoke(view, "updateHUDElement('health', 100)");
```

### 3. Update Plugin Metadata
Edit [xmake.lua](xmake.lua) to customize:
- `PLUGIN_NAME` - Your plugin's name
- `PLUGIN_VERSION` - Version number
- `VIEW_NAME` - Your view's folder name
- Author and description in build rules

### 4. Build Your Plugin

**Option A: Using XMake (Recommended)**
```batch
xmake build
```
Output: `dist/YourPluginName_Version/`

**Option B: Using Generated Visual Studio Project**
```batch
xmake project -k vsxmake
```
Then open the generated `.sln` file in Visual Studio 2026

### 5. Install to Skyrim
Copy the distribution folder to your Skyrim SE/AE installation:
```
SkyrimSE/
├── Data/
├── SKSE/plugins/
│   └── PrismaUI-Example-Plugin.dll
└── PrismaUI/views/PrismaUI-Example-UI/
    └── index.html
```

## Build Commands

| Command | Purpose |
|---------|---------|
| `xmake config -vD` | Configure build system (verbose) |
| `xmake build` | Build the plugin in release mode |
| `xmake build --dry-run` | Simulate build without compiling |
| `xmake project -k vsxmake` | Generate Visual Studio 2026 solution |
| `xmake repo --update` | Update package repositories |
| `xmake require --upgrade` | Upgrade dependencies |
| `xmake clean` | Clean build artifacts |

## Environment Variables (Optional)

Set these to redirect build output:
```batch
set XSE_TES5_GAME_PATH=C:\Path\To\Skyrim SE
set XSE_TES5_MODS_PATH=C:\Path\To\MO2\Mods
```

## PrismaUI API Overview

The plugin uses the Prisma UI API (v1) for HUD communication:

```cpp
// Get API (called at kDataLoaded)
PrismaUI = static_cast<PRISMA_UI_API::IVPrismaUI1*>(
    PRISMA_UI_API::RequestPluginAPI(PRISMA_UI_API::InterfaceVersion::V1)
);

// Create/Initialize View
view = PrismaUI->CreateView("FolderName/index.html", [](PrismaView v) {
    // Called when DOM is ready
});

// Send JS function calls from C++
PrismaUI->Invoke(view, "functionName('arg1', arg2)");

// Listen for JS function calls in C++
PrismaUI->RegisterJSListener(view, "handlerName", [](const char* data) {
    // Handle data
});

// Focus/Unfocus the UI
PrismaUI->Focus(view);
PrismaUI->Unfocus(view);
bool hasFocus = PrismaUI->HasFocus(view);
```

## Example: Key Handler Pattern

The template includes a key handler for toggling UI focus (F3):
- Location: [src/keyhandler/keyhandler.cpp](src/keyhandler/keyhandler.cpp)
- Shows how to register keyboard input events
- Demonstrates focus/unfocus operations
- Customize the key code (0x3D = F3) and callbacks for your HUD

## Notes & Tips

1. **Precompiled Headers**: `pch.h` includes common SKSE/Skyrim headers - add custom includes here
2. **Plugin Logging**: Use `logger::info()` from SKSE logging system
3. **C++23 Features**: Modern C++ syntax is fully supported
4. **Distribution Structure**: XMake automatically creates proper folder hierarchy for MO2/direct installation
5. **Build Time**: First build compiles CommonLibSSE-NG (~5-10 min), subsequent builds are faster

## Verification Checklist

- [x] Repository cloned: PrismaUI-SKSE/example-skse-plugin
- [x] Submodules initialized: CommonLibSSE-NG, OpenVR
- [x] XMake installed and functional (v3.1.2+)
- [x] MSVC C++ compiler detected (VS 2026)
- [x] Build configuration verified (dry-run successful)
- [x] Project structure validated
- [x] Ready for custom HUD integration

## Resources

- **Template Repository**: https://github.com/PrismaUI-SKSE/example-skse-plugin
- **Prisma UI Framework**: https://github.com/PrismaUI-SKSE/framework
- **CommonLibSSE-NG**: https://github.com/alandtse/CommonLibVR
- **XMake Documentation**: https://xmake.io/guide/quick-start
- **SKSE Documentation**: https://skse.silverlock.org/

---

**Status**: ✅ Ready for development
**Last Updated**: 2026-11-11
**Setup Time**: ~5 minutes
