# Customization Guide: Adding Your Custom HUD

> Historical template guide: plugin names, view paths, and code examples below predate
> `poisesystemhud`. See [README.md](README.md) for the current package, view path, and build steps.

This guide walks you through replacing the template HUD with your custom "Poet Style" HUD design.

## Step 1: Create Your Custom UI

### Replace the HTML/CSS/JavaScript

Your custom HUD files should go in the `view/` directory. The template currently has a basic example.

**Location**: [view/index.html](view/index.html)

### Recommended Structure
```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Your HUD Title</title>
    <link rel="stylesheet" href="styles.css">
</head>
<body>
    <!-- Your HUD elements here -->
    <div id="hud-container">
        <!-- Health bar, stamina, status effects, etc. -->
    </div>
    
    <script src="hud.js"></script>
</body>
</html>
```

### Key CSS Considerations for In-Game Overlay
- Use `position: fixed` or `position: absolute` for overlay positioning
- Avoid `user-select: text` - the template uses `user-select: none`
- Test transparency with `rgba()` colors for blending with game
- Use `z-index` carefully for layering elements
- Consider performance: minimize animations and DOM updates

### Example: Simple Health Bar HUD
```html
<style>
    body {
        margin: 0;
        padding: 0;
        background: transparent;
        font-family: Arial, sans-serif;
    }
    
    .health-bar {
        position: fixed;
        bottom: 50px;
        left: 50px;
        width: 200px;
        height: 30px;
        background: rgba(0, 0, 0, 0.7);
        border: 2px solid #fff;
        border-radius: 4px;
        overflow: hidden;
    }
    
    .health-fill {
        height: 100%;
        background: linear-gradient(90deg, #ff0000, #ff6600);
        width: 100%;
        transition: width 0.2s ease;
    }
</style>

<div class="health-bar">
    <div class="health-fill" id="health"></div>
</div>

<script>
    // This function will be called from your C++ plugin
    window.updateHealth = (current, max) => {
        const percentage = (current / max) * 100;
        document.getElementById('health').style.width = percentage + '%';
    };
</script>
```

## Step 2: Update the C++ Plugin Code

### Modify Plugin Initialization

**Location**: [src/main.cpp](src/main.cpp)

Replace the `SKSEMessageHandler` function to initialize your custom view:

```cpp
static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
    switch (message->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        // Initialize PrismaUI API
        PrismaUI = static_cast<PRISMA_UI_API::IVPrismaUI1*>(
            PRISMA_UI_API::RequestPluginAPI(PRISMA_UI_API::InterfaceVersion::V1)
        );

        // Create your view - IMPORTANT: folder name must match your view directory
        view = PrismaUI->CreateView("PrismaUI-Example-UI/index.html", [](PrismaView a_view) {
            logger::info("Custom HUD DOM is ready");
            
            // Initialize HUD with game data
            PrismaUI->Invoke(a_view, "initializeHUD()");
        });

        // Register listeners for data from your HUD JS
        PrismaUI->RegisterJSListener(view, "playerClickedAbility", [](const char* data) {
            logger::info("Player clicked ability: {}", data);
            // Handle ability activation
        });

        // Set up event handlers (example: listen to game events)
        SetupGameEventListeners();
        
        // Set up key handler
        KeyHandler::RegisterSink();
        KeyHandler* keyHandler = KeyHandler::GetSingleton();
        const uint32_t TOGGLE_HUD_KEY = 0x3D; // F3

        keyHandler->Register(TOGGLE_HUD_KEY, KeyEventType::KEY_DOWN, []() {
            if (!PrismaUI->HasFocus(view)) {
                PrismaUI->Focus(view);
                logger::info("HUD focused");
            } else {
                PrismaUI->Unfocus(view);
                logger::info("HUD unfocused");
            }
        });
        
        break;
    }
}
```

### Add Game Event Handling

Create a new function to handle Skyrim game events:

```cpp
static void SetupGameEventListeners()
{
    // Hook into SKSE events to update HUD
    auto scriptInterface = SKSE::GetScriptInterface();
    
    // Example: Register for OnUpdateGameTime event
    // This would update your HUD display with current game data
    
    logger::info("Game event listeners registered");
}

// Helper function to send data to HUD
static void UpdateHUDDisplay(const std::string& functionName, const std::string& args)
{
    if (PrismaUI && view) {
        std::string invocation = functionName + "(" + args + ")";
        PrismaUI->Invoke(view, invocation.c_str());
    }
}
```

## Step 3: Update Project Configuration

### Rename Your View Folder and Update Config

1. **Rename the view folder** (if desired):
   - Current: `view/` → New: `view/YourHUDName/`
   - Add your HTML/CSS/JS files there

2. **Update [xmake.lua](xmake.lua)**:
   ```lua
   local PLUGIN_NAME = "YourPluginName"
   local PLUGIN_VERSION = "1.0.0"
   local VIEW_NAME = "YourHUDName"  -- Matches your view directory
   
   -- Update author/description
   add_rules("commonlibsse-ng.plugin", {
      name = PLUGIN_NAME,
      author = "Your Name <discord: yourname>",
      description = "Your custom HUD plugin using CommonLibSSE-NG and PrismaUI"
   })
   ```

## Step 4: Build and Test

### Build the Plugin
```bash
cd C:\Users\tward\OneDrive\Desktop\poetstylehudforskyrim
xmake build
```

Output location: `dist/YourPluginName_Version/`

### Installation for Testing

**Option A: Mod Organizer 2 (Recommended)**
1. Create a new mod in MO2
2. Copy contents of `dist/YourPluginName_Version/` to mod folder
3. Activate in MO2
4. Launch Skyrim through MO2

**Option B: Direct Installation**
1. Copy `dist/YourPluginName_Version/SKSE/` to `SkyrimSE/SKSE/`
2. Copy `dist/YourPluginName_Version/PrismaUI/` to `SkyrimSE/PrismaUI/`
3. Launch Skyrim

### Testing & Debugging
- Check plugin load log: `SkyrimSE/My Games/SKSE/skse.log`
- Enable debug logging in your plugin (modify logging in main.cpp)
- Press F3 (or your toggle key) to focus/unfocus HUD
- Open browser dev tools if needed for JS debugging

## Step 5: Advanced Customization

### Listening for Game Events

Update your C++ code to capture game events:

```cpp
// Example: Listen to actor health changes
class PlayerEventListener : public RE::BSTEventSink<RE::TESCombatEvent>
{
public:
    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESCombatEvent* event,
        RE::BSTEventSource<RE::TESCombatEvent>* dispatcher
    ) override {
        if (event && event->actor == RE::PlayerCharacter::GetSingleton()) {
            // Player combat state changed
            UpdateHUDDisplay("updateCombatState", "'active'");
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
```

### Sending Complex Data to HUD

```cpp
// Send structured data via JSON string
std::string playerData = R"(
{
    "health": 100,
    "stamina": 75,
    "magicka": 50,
    "level": 10,
    "experience": 1500
}
)";

PrismaUI->Invoke(view, ("updatePlayerStats(" + playerData + ")").c_str());
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| HUD doesn't show in-game | Check view folder name matches `CREATE_VIEW()` path |
| JS functions not callable | Ensure functions are global (`window.functionName`) |
| Plugin doesn't load | Check SKSE log, verify DLL in correct SKSE/plugins folder |
| HUD elements misaligned | Use `fixed` positioning, test with different resolutions |
| Performance issues | Minimize animations, batch DOM updates, use `requestAnimationFrame()` |

## File Checklist

Before building, verify:
- [ ] [src/main.cpp](src/main.cpp) - Updated with your event handlers
- [ ] [view/index.html](view/index.html) - Your custom HTML/CSS/JS
- [ ] [xmake.lua](xmake.lua) - Plugin name and view path updated
- [ ] [src/pch.h](src/pch.h) - Any additional headers needed
- [ ] [src/keyhandler/keyhandler.cpp](src/keyhandler/keyhandler.cpp) - Custom key bindings if needed

## Quick Reference: Common Operations

### Update HUD from C++
```cpp
PrismaUI->Invoke(view, "updateHealthBar(100)");
```

### Listen to HUD JS Events
```cpp
PrismaUI->RegisterJSListener(view, "eventName", [](const char* data) {
    logger::info("Event data: {}", data);
});
```

### Toggle HUD Visibility
```cpp
PrismaUI->Focus(view);   // Show and focus
PrismaUI->Unfocus(view); // Hide and unfocus
```

### Log Messages
```cpp
logger::info("Message here");
logger::warn("Warning message");
logger::critical("Critical message");
```

---

**Next**: Check [PROJECT_SETUP.md](PROJECT_SETUP.md) for build commands and installation instructions.
