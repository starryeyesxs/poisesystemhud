# SKSE Menu Framework Integration Plan (REVISED)
**Implementation Strategy with Official SKSEMenuFramework.h API**  
**Status: READY FOR IMPLEMENTATION**

---

## REVISIONS APPLIED

This revised plan incorporates the following adjustments requested:
1. ✅ Use official `SKSEMenuFramework.h` single-header API (dynamic `GetModuleHandle` / `IsInstalled()`)
2. ✅ Avoid full DOM reloads; push settings directly via `updateHudSettings()` JS function (reload only on `iHudStyle` 0↔1 change)
3. ✅ Practical slider ranges for ImGui (capped to gameplay bounds; higher manual INI values respected)
4. ✅ Fix README with correct SKSE Menu Framework source and overlay hotkey access instructions

---

## 1. CURRENT INI & CONFIG ARCHITECTURE

### 1.1 Settings Overview
All settings are read from `Data/SKSE/Plugins/arpghud.ini` at plugin startup (`LoadSettings()` in main.cpp) and on F10 reload. The plugin uses native Windows `GetPrivateProfileStringW()` for INI parsing.

#### **[HUD] Section** (Layout & Display)
| Setting | Type | Default | Range | Purpose |
|---------|------|---------|-------|---------|
| `iHudStyle` | int | 0 | 0–1 | 0=ARPG globes, 1=minimalist bars |
| `bEnableWeatherWidget` | bool | true | — | Show location/weather widget |
| `bEnableSneakLightGem` | bool | true | — | Show light gem while sneaking (minimalist only) |
| `fGlobeScale` | float | 0.85 | 0.1–2.0 | Size of corner health/magicka globes |
| `fHotbarScale` | float | 0.62 | 0.1–2.0 | Size of center stamina/hotbar bar |
| `iSideOffset` | int | 20 | 0–2000 | Left/right pixel distance from screen edge |
| `iBottomOffset` | int | 16 | 0–2000 | Pixel distance from bottom for globes |
| `iHotbarBottomOffset` | int | 0 | 0–2000 | Pixel distance from bottom for hotbar |
| `bShowVitalNumbers` | bool | false | — | Show health/magicka/stamina percentages |
| `bShowPoiseBar` | bool | true | — | Show poise bar (if available) |

#### **[HUD] Section** (Dynamic Visibility & Opacity)
| Setting | Type | Default | Range | Purpose |
|---------|------|---------|-------|---------|
| `bDynamicHUD` | bool | true | — | Fade HUD when idle, full vitals, unarmed |
| `bAlwaysOn` | bool | false | — | Keep HUD always visible (overrides dynamic) |
| `fIdleOpacity` | float | 0.0 | 0.0–1.0 | Opacity when idle (0=invisible) |
| `fFadeDelaySeconds` | float | 3.0 | 0.0–600.0 | Seconds before fading to idle opacity |

#### **[Graphics] Section** (Visual Effects)
| Setting | Type | Default | Range | Purpose |
|---------|------|---------|-------|---------|
| `bGlassEffects` | bool | true | — | Enable glass reflections, liquid glow, shadows |
| `fGlassShineOpacity` | float | 1.0 | 0.0–2.0 | Specular shine intensity |
| `fMeniscusGlow` | float | 1.0 | 0.0–2.0 | Liquid meniscus glow intensity |
| `fOrbInnerShadow` | float | 1.0 | 0.0–1.5 | Inner rim shadow intensity |

#### **[Poise] Section** (Combat Balance Tuning)
| Setting | Type | Default | Range | Purpose |
|---------|------|---------|-------|---------|
| `bEnableBuiltInPoise` | bool | true | — | Enable player poise system |
| `bEnableNPCPoise` | bool | true | — | Enable NPC poise system |
| `fBasePoise` | float | 40.0 | 1.0–100000.0 | Base player poise value |
| `fArmorWeightMultiplier` | float | 1.0 | 0.0–100.0 | Armor weight's contribution to poise |
| `fBaseHitPoiseDamage` | float | 8.0 | 0.0–10000.0 | Base poise damage from physical hits |
| `fWeaponWeightDamageMultiplier` | float | 0.5 | 0.0–100.0 | Weapon weight multiplier for poise damage |
| `fTwoHandedMultiplier` | float | 1.4 | 0.0–10.0 | Multiplier for two-handed weapons |
| `fBashMultiplier` | float | 1.2 | 0.0–10.0 | Multiplier for shield bashes |
| `fCreatureMultiplier` | float | 1.2 | 0.0–10.0 | Multiplier for creature attacks |
| `fProjectileMultiplier` | float | 0.8 | 0.0–10.0 | Multiplier for projectile/arrow hits |
| `fPowerAttackMultiplier` | float | 1.5 | 0.0–10.0 | Multiplier for power attacks |
| `fBlockedDamageMultiplier` | float | 0.4 | 0.0–1.0 | Multiplier for blocked damage |
| `fUnblockedPhysicalMultiplier` | float | 2.5 | 0.0–10.0 | Multiplier for unblocked physical damage |
| `fRegenDelaySeconds` | float | 3.0 | 0.0–600.0 | Seconds before poise regeneration starts |
| `fRegenRatePerSecond` | float | 8.0 | 0.0–10000.0 | Poise regeneration rate (points/sec) |
| `fStaggerImmunitySeconds` | float | 1.0 | 0.0–60.0 | Immunity duration after stagger break |
| `fFleshSpellPoiseMultiplier` | float | 0.45 | 0.0–100.0 | Damage multiplier for Flesh spells (MAGIC_GAMEPLAY) |
| `fDragonhidePoiseBonus` | float | 60.0 | 0.0–100000.0 | Poise bonus from Dragonhide (MAGIC_GAMEPLAY) |
| `fMagicBurstPoiseMultiplier` | float | 2.0 | 0.0–100.0 | Multiplier for magic burst effects (MAGIC_GAMEPLAY) |
| `fMagicStaggerPoiseMultiplier` | float | 10.0 | 0.0–100.0 | Multiplier for magic stagger effects (MAGIC_GAMEPLAY) |
| `fConcentrationPoiseMultiplier` | float | 1.0 | 0.0–100.0 | Multiplier for concentration damage (MAGIC_GAMEPLAY) |
| `fConcentrationIntervalSeconds` | float | 1.0 | 0.01–60.0 | Interval for concentration damage pooling (MAGIC_GAMEPLAY) |
| `fConcentrationMaxPoisePerInterval` | float | 15.0 | 0.0–10000.0 | Max poise damage per interval (MAGIC_GAMEPLAY) |
| `fWardMagicDamageMultiplier` | float | 0.35 | 0.0–1.0 | Ward spell magic poise damage reduction |
| `fShieldMagicDamageMultiplier` | float | 0.6 | 0.0–1.0 | Shield block magic poise damage reduction |

### 1.2 Configuration Flow & F10 Reload Mechanism

**Startup Flow:**
1. Plugin loads (SKSEPlugin_Load)
2. SKSE messages trigger `SKSEMessageHandler`
3. `kDataLoaded` event fires → calls `LoadSettings()`
4. Settings loaded into global `HudSettings` struct and `BuiltinPoise::Settings`
5. F10 key registered (scan code 0x44) via KeyHandler

**F10 Reload (in-game):**
1. KeyHandler detects F10 press → calls `ReloadHud()`
2. `ReloadHud()` calls `LoadSettings()` (re-reads INI)
3. INI changes are parsed into memory structs
4. `BuiltinPoise::Configure(poiseSettings)` applies changes to combat system
5. PrismaUI `Invoke(hudView, "window.location.reload()")` reloads view DOM
6. On DOM ready (`OnDomReady`), settings propagated via `updateHudSettings()` JavaScript function

**INI Persistence:**
- Settings are **read-only** from INI during game play
- No automatic write-back to INI when settings change in-game
- F10 reload re-reads the INI file (allowing manual edits to take effect)
- Users must manually edit INI file for changes

---

## 2. SKSE MENU FRAMEWORK INTEGRATION & SOFT DEPENDENCY

### 2.1 Current State
- **No SKSE Menu Framework integration currently exists**
- No headers or SDK files in the repo
- Plugin uses custom KeyHandler for F10 binding

### 2.2 Official Single-Header API Approach

**Source:** [QTR-Modding/SKSE-Menu-Framework-3](https://github.com/QTR-Modding/SKSE-Menu-Framework-3)  
**Header:** `SKSEMenuFramework.h` (provided in repo or downloaded at build time)

#### Key API Features (Built-in Soft Dependency)
```cpp
// Dynamic module loading (no static linking required)
inline HMODULE GetMenuFrameworkModule() {
    static HMODULE menuFramework = nullptr;
    if (!menuFramework) {
        menuFramework = GetModuleHandleW(L"SKSEMenuFramework");
    }
    return menuFramework;
}

// Check if framework is installed
inline bool IsInstalled() {
    return std::filesystem::exists("Data/SKSE/Plugins/SKSEMenuFramework.dll");
}

// Query functions at runtime
template <class T>
T GetMenuFrameworkFunction(LPCSTR name) {
    auto module = GetMenuFrameworkModule();
    if (!module) return nullptr;
    return reinterpret_cast<T>(GetProcAddress(module, name));
}
```

**Advantages:**
- Single header file; no external SDK dependencies
- Framework presence detected at runtime via `GetModuleHandle` + `GetProcAddress`
- Plugin loads cleanly even if framework absent
- No conditional compilation needed for soft dependency

#### 2.3 Integration Architecture

**At Plugin Load (`kDataLoaded`):**
1. Check `SKSEMenuFramework::IsInstalled()` 
2. If true: Call framework API functions to register menu window
3. If false: Log info message; skip menu registration; continue normally
4. Plugin operates identically with or without framework

**Menu Registration Pattern:**
```cpp
// Pseudo-code (detailed in Phase 2)
if (SKSEMenuFramework::IsInstalled()) {
    // Register window with custom render callback
    auto menuWindow = SKSEMenuFramework::AddWindow(OnMenuRender);
    // Window rendering happens in callback; no special setup needed
}
```

**Function Pointer Types (from official API):**
- `AddWindowFunction`: Register a menu window with render callback
- `AddSectionItemFunction`: Add menu item to framework's main menu
- Other utilities: texture loading, input events, HUD elements

---

## 3. MENU ORGANIZATION & LIVE PERSISTENCE

### 3.1 Proposed Menu Structure

The mod menu will be organized into **5 logical sections**, mirroring INI structure:

#### **Section 1: Layout & Positioning**
| Control | Type | Range | Default | Note |
|---------|------|-------|---------|------|
| **HUD Style** | Enum | ARPG (0) / Minimalist (1) | 0 | DOM reload on change |
| **Globe Scale** | Slider | 0.1–2.0 | 0.85 | Live update via JS |
| **Hotbar Scale** | Slider | 0.1–2.0 | 0.62 | Live update via JS |
| **Side Offset** | Slider | 0–2000 px | 20 | Live update via JS |
| **Bottom Offset** | Slider | 0–2000 px | 16 | Live update via JS |
| **Hotbar Bottom Offset** | Slider | 0–2000 px | 0 | Live update via JS |

#### **Section 2: Widget Visibility & Display**
| Control | Type | Default | Note |
|---------|------|---------|------|
| **Weather Widget** | Toggle | true | Live update via JS |
| **Sneak Light Gem** | Toggle | true | Live update via JS |
| **Show Vital Numbers** | Toggle | false | Live update via JS |
| **Show Poise Bar** | Toggle | true | Live update via JS |

#### **Section 3: Dynamic Visibility & Opacity**
| Control | Type | Range | Default | Note |
|---------|------|-------|---------|------|
| **Enable Dynamic HUD** | Toggle | — | true | Live update via JS |
| **Always On** | Toggle | — | false | Live update via JS (overrides dynamic) |
| **Idle Opacity** | Slider | 0.0–1.0 | 0.0 | Live update via JS |
| **Fade Delay** | Slider | 0.0–10.0 sec | 3.0 | **Capped for gameplay** (manual INI can go higher) |

#### **Section 4: Visual Effects (Graphics)**
| Control | Type | Range | Default | Note |
|---------|------|-------|---------|------|
| **Glass Effects** | Toggle | — | true | Live update via JS |
| **Glass Shine Opacity** | Slider | 0.0–2.0 | 1.0 | Live update via JS |
| **Meniscus Glow** | Slider | 0.0–2.0 | 1.0 | Live update via JS |
| **Orb Inner Shadow** | Slider | 0.0–1.5 | 1.0 | Live update via JS |

#### **Section 5: Combat Balance (Poise Tuning)**
| Control | Type | ImGui Range | Default | Note |
|---------|------|-------------|---------|------|
| **Enable Player Poise** | Toggle | — | true | Updates poise system immediately |
| **Enable NPC Poise** | Toggle | — | true | Updates poise system immediately |
| **Base Poise** | Slider | 10.0–500.0 | 40.0 | Practical bounds (manual INI: 1–100k) |
| **Armor Weight Mult** | Slider | 0.0–5.0 | 1.0 | Practical bounds (manual INI: 0–100) |
| **Base Hit Damage** | Slider | 0.0–100.0 | 8.0 | Practical bounds (manual INI: 0–10k) |
| **Weapon Weight Mult** | Slider | 0.0–5.0 | 0.5 | Practical bounds (manual INI: 0–100) |
| **Two-Handed Mult** | Slider | 0.5–5.0 | 1.4 | Practical bounds (manual INI: 0–10) |
| **Bash Mult** | Slider | 0.5–5.0 | 1.2 | Practical bounds |
| **Creature Mult** | Slider | 0.5–5.0 | 1.2 | Practical bounds |
| **Projectile Mult** | Slider | 0.1–2.0 | 0.8 | Practical bounds |
| **Power Attack Mult** | Slider | 0.5–5.0 | 1.5 | Practical bounds |
| **Blocked Damage Mult** | Slider | 0.0–1.0 | 0.4 | Practical bounds |
| **Unblocked Physical Mult** | Slider | 1.0–10.0 | 2.5 | Practical bounds |
| **Regen Delay** | Slider | 0.0–15.0 sec | 3.0 | Practical bounds (manual INI: 0–600) |
| **Regen Rate** | Slider | 1.0–50.0 pts/sec | 8.0 | Practical bounds (manual INI: 0–10k) |
| **Stagger Immunity** | Slider | 0.0–5.0 sec | 1.0 | Practical bounds (manual INI: 0–60) |
| **Flesh Spell Mult** | Slider | 0.0–2.0 | 0.45 | (MAGIC_GAMEPLAY only) |
| **Dragonhide Bonus** | Slider | 0.0–200.0 | 60.0 | (MAGIC_GAMEPLAY only; practical bounds) |
| **Magic Burst Mult** | Slider | 0.5–10.0 | 2.0 | (MAGIC_GAMEPLAY only) |
| **Magic Stagger Mult** | Slider | 1.0–50.0 | 10.0 | (MAGIC_GAMEPLAY only; practical bounds) |
| **Concentration Mult** | Slider | 0.0–10.0 | 1.0 | (MAGIC_GAMEPLAY only) |
| **Concentration Interval** | Slider | 0.01–5.0 sec | 1.0 | (MAGIC_GAMEPLAY only; practical bounds) |
| **Concentration Max** | Slider | 1.0–100.0 | 15.0 | (MAGIC_GAMEPLAY only; practical bounds) |
| **Ward Magic Mult** | Slider | 0.0–1.0 | 0.35 | Live update |
| **Shield Magic Mult** | Slider | 0.0–1.0 | 0.6 | Live update |

### 3.2 Live Updates WITHOUT Full DOM Reload

**Principle:** Avoid `window.location.reload()` except when absolutely necessary.

#### Update Paths:

**Standard Settings (No DOM Reload):**
- Scales, offsets, opacities, toggles, multipliers
- Send directly via: `updateHudSettings(globeScale, hotbarScale, sideOffset, ...)`
- JavaScript updates styles/properties in-place
- No page refresh required

**Style Change (DOM Reload Required):**
- When `iHudStyle` toggles between 0 (ARPG globes) ↔ 1 (minimalist bars)
- Call: `window.location.reload()` to rebuild DOM for new layout
- This is the **only** case that needs full reload

**Poise Changes (Immediate System Update):**
- When any poise setting changes
- Call: `BuiltinPoise::Configure(poiseSettings)` immediately
- No in-game visual change until next hit/regen tick
- Takes effect instantly for new gameplay events

#### Update Flow:
```cpp
// Menu slider/toggle changes in ImGui
void OnSettingChanged(const char* key, float newValue) {
    
    // 1. Update in-memory struct
    ApplySettingInMemory(key, newValue);
    
    // 2. Debounced INI write (see 3.3)
    QueueINIWrite(key, newValue);
    
    // 3. Apply to systems
    if (isPoiseSetting(key)) {
        BuiltinPoise::Configure(poiseSettings);
    } else if (key == "iHudStyle") {
        // Style changed; reload DOM
        prismaUI->Invoke(hudView, "window.location.reload()");
    } else {
        // Standard HUD setting; live update only
        SendSettingsToHUD();
    }
}
```

### 3.3 INI Persistence with Debouncing

**Problem:** Slider drag generates 60+ events/sec; writing INI on each would spam disk I/O.

**Solution:** Debounced writes
- Collect all pending changes in a queue
- Write to INI **once per 500ms** (configurable)
- If INI write fails, log warning; in-memory value persists
- On next game reload, INI is re-read

**Implementation:**
```cpp
std::unordered_map<std::string, float> pendingWrites;
std::chrono::steady_clock::time_point lastINIWrite;

void QueueINIWrite(const char* key, float value) {
    pendingWrites[key] = value;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastINIWrite).count();
    
    if (elapsed >= 500) {  // Debounce threshold
        FlushINIWrites();
        lastINIWrite = now;
    }
}

void FlushINIWrites() {
    // Write all pending changes to INI (format-preserving)
    for (const auto& [key, value] : pendingWrites) {
        WriteSettingToINI(key, value);
    }
    pendingWrites.clear();
}
```

**INI Format Preservation:**
- Regex line-by-line parsing to find `key=oldValue` lines
- Replace only the value portion
- Preserve section headers, comments, blank lines, whitespace
- Example:
  ```ini
  ; Glass shine intensity. Range: 0.0 to 2.0.
  fGlassShineOpacity=1.0
  ```
  Becomes:
  ```ini
  ; Glass shine intensity. Range: 0.0 to 2.0.
  fGlassShineOpacity=1.5
  ```

**Fallback on Failure:**
- If file write fails (permissions, locked, etc.): log warning, keep in-memory value
- User can manually edit INI later or use F10 to force re-read
- Gameplay is not disrupted

### 3.4 Manual INI Edits + Resync

**Scenario:** User edits `arpghud.ini` manually while game running.

**Behavior:**
- Menu sliders remain at current values (in-memory snapshot)
- No automatic re-read to avoid conflicts during gameplay
- User presses F10 → `ReloadHud()` re-reads INI
- Menu values update to match re-loaded settings
- OR: Add "Reload from File" button in menu (optional quality-of-life)

---

## 4. README UPDATE PLAN

### 4.1 New Documentation Sections

**Location in README:** Add new subsection under "Requirements" and expand "Build and install" section.

#### **New: "Configuration" Section** (after "Build and install")
Replace the old F10-only section with:

```markdown
### Configuration

The mod provides two configuration methods:

#### In-Game Menu (Recommended)

If [SKSE Menu Framework](https://github.com/QTR-Modding/SKSE-Menu-Framework-3) is installed, 
access the mod menu during gameplay:

1. Press the **SKSE Menu Framework overlay hotkey** (default: **Right Ctrl + Right Shift**)  
   *(Check your SKSE Menu Framework config for the hotkey; it is customizable)*
2. Navigate to **ArpgHud** in the mod list
3. Adjust layout, visibility, graphics, and poise settings; changes apply immediately and persist to `arpghud.ini`

**Note:** SKSE Menu Framework is optional. If not installed, use manual configuration below.

#### Manual Configuration (INI)

Edit `SKSE/Plugins/arpghud.ini` using a text editor:

1. Locate your Skyrim installation directory (where `SkyrimSE.exe` is installed)
2. Open `Data/SKSE/Plugins/arpghud.ini` in Notepad or your preferred editor
3. Modify settings in the `[HUD]`, `[Graphics]`, and `[Poise]` sections (see table below)
4. Save the file
5. Press **F10** in-game to reload and apply changes

**Example:**
```ini
[HUD]
iHudStyle=0                   ; 0 = ARPG globes, 1 = minimalist bars
fGlobeScale=0.85              ; Range: 0.1 to 2.0
fHotbarScale=0.62             ; Range: 0.1 to 2.0
iSideOffset=20                ; Pixel distance from left/right edge
```

#### Settings Reference

The following table lists all available settings. For in-game menu access, see the menu sections above. 
For manual INI edits, modify the values and press **F10** to reload.

**[HUD] - Layout & Positioning**
| Setting | Type | Default | Range | Description |
|---------|------|---------|-------|-------------|
| `iHudStyle` | int | 0 | 0–1 | 0 = ARPG globes and hotbar; 1 = minimalist bars |
| `fGlobeScale` | float | 0.85 | 0.1–2.0 | Size of bottom-left Health and bottom-right Magicka globes |
| `fHotbarScale` | float | 0.62 | 0.1–2.0 | Size of bottom-center Stamina and hotkey bar |
| `iSideOffset` | int | 20 | 0–2000 | Distance (pixels) from left/right screen edge to globes |
| `iBottomOffset` | int | 16 | 0–2000 | Distance (pixels) from bottom of screen to globes |
| `iHotbarBottomOffset` | int | 0 | 0–2000 | Distance (pixels) from bottom of screen to hotbar |

**[HUD] - Widget Visibility & Display**
| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `bEnableWeatherWidget` | bool | true | Show location, date, and weather widget |
| `bEnableSneakLightGem` | bool | true | Show light detection gem while sneaking (minimalist layout only) |
| `bShowVitalNumbers` | bool | false | Display Health, Magicka, and Stamina percentages inside globes/bars |
| `bShowPoiseBar` | bool | true | Show player poise bar (if built-in poise enabled or external provider active) |

**[HUD] - Dynamic Visibility & Opacity**
| Setting | Type | Default | Range | Description |
|---------|------|---------|-------|-------------|
| `bDynamicHUD` | bool | true | — | Fade HUD when idle with full health/magicka/stamina and no weapon/spell drawn |
| `bAlwaysOn` | bool | false | — | Keep HUD permanently visible at full opacity (overrides `bDynamicHUD`) |
| `fIdleOpacity` | float | 0.0 | 0.0–1.0 | HUD opacity while idle (0.0 = invisible, 1.0 = full opacity) |
| `fFadeDelaySeconds` | float | 3.0 | 0.0–600.0 | Seconds to wait after HUD becomes idle before fading |

**[Graphics] - Visual Effects**
| Setting | Type | Default | Range | Description |
|---------|------|---------|-------|-------------|
| `bGlassEffects` | bool | true | — | Enable glass reflections, liquid glow, and inner rim shadows |
| `fGlassShineOpacity` | float | 1.0 | 0.0–2.0 | Intensity of specular glass shine |
| `fMeniscusGlow` | float | 1.0 | 0.0–2.0 | Intensity of liquid meniscus and core glow |
| `fOrbInnerShadow` | float | 1.0 | 0.0–1.5 | Intensity of orb inner rim shadow |

**[Poise] - Combat Balance Tuning**
| Setting | Type | Default | Range | Description |
|---------|------|---------|-------|-------------|
| `bEnableBuiltInPoise` | bool | true | — | Enable the built-in player poise system (disable if using another poise mod) |
| `bEnableNPCPoise` | bool | true | — | Enable built-in poise for loaded NPCs |
| `fBasePoise` | float | 40.0 | 1.0–100000.0 | Base poise value for the player |
| `fArmorWeightMultiplier` | float | 1.0 | 0.0–100.0 | Contribution of worn armor weight to maximum poise |
| `fBaseHitPoiseDamage` | float | 8.0 | 0.0–10000.0 | Base poise damage inflicted by physical hits |
| `fWeaponWeightDamageMultiplier` | float | 0.5 | 0.0–100.0 | Weapon weight contribution to poise damage |
| `fTwoHandedMultiplier` | float | 1.4 | 0.0–10.0 | Poise damage multiplier for two-handed weapons |
| `fBashMultiplier` | float | 1.2 | 0.0–10.0 | Poise damage multiplier for shield bashes |
| `fCreatureMultiplier` | float | 1.2 | 0.0–10.0 | Poise damage multiplier for creature attacks |
| `fProjectileMultiplier` | float | 0.8 | 0.0–10.0 | Poise damage multiplier for projectiles and arrows |
| `fPowerAttackMultiplier` | float | 1.5 | 0.0–10.0 | Poise damage multiplier for power attacks |
| `fBlockedDamageMultiplier` | float | 0.4 | 0.0–1.0 | Poise damage reduction when blocking with shield |
| `fUnblockedPhysicalMultiplier` | float | 2.5 | 0.0–10.0 | Poise damage multiplier for unblocked physical hits |
| `fRegenDelaySeconds` | float | 3.0 | 0.0–600.0 | Delay (seconds) before poise begins regenerating after a break |
| `fRegenRatePerSecond` | float | 8.0 | 0.0–10000.0 | Poise regeneration rate (points per second) |
| `fStaggerImmunitySeconds` | float | 1.0 | 0.0–60.0 | Duration of stagger immunity after poise break |
| `fWardMagicDamageMultiplier` | float | 0.35 | 0.0–1.0 | Poise damage reduction from spells blocked by ward magic |
| `fShieldMagicDamageMultiplier` | float | 0.6 | 0.0–1.0 | Poise damage reduction from spells blocked by active shield |

**[Poise] - Magic Gameplay Settings** *(only with experimental `ARPGHUD_MAGIC_GAMEPLAY` build)*
| Setting | Type | Default | Range | Description |
|---------|------|---------|-------|-------------|
| `fFleshSpellPoiseMultiplier` | float | 0.45 | 0.0–100.0 | Poise damage multiplier for Flesh spell effects |
| `fDragonhidePoiseBonus` | float | 60.0 | 0.0–100000.0 | Bonus poise granted by Dragonhide spell |
| `fMagicBurstPoiseMultiplier` | float | 2.0 | 0.0–100.0 | Poise damage multiplier for magic burst effects |
| `fMagicStaggerPoiseMultiplier` | float | 10.0 | 0.0–100.0 | Poise damage multiplier for magic stagger effects |
| `fConcentrationPoiseMultiplier` | float | 1.0 | 0.0–100.0 | Poise damage multiplier for concentration hits |
| `fConcentrationIntervalSeconds` | float | 1.0 | 0.01–60.0 | Interval (seconds) for pooling concentration poise damage |
| `fConcentrationMaxPoisePerInterval` | float | 15.0 | 0.0–10000.0 | Maximum poise damage per concentration interval |
```

### 4.2 Installation Instructions Update

**Update the main "Requirements" section to include:**

```markdown
## Requirements

- **Skyrim SE/AE** (tested on 1.7.104)
- **SKSE64** (latest version)
- **Address Library** (latest version)
- **PrismaUI** (required; provides HUD rendering framework)
- **SKSE Menu Framework** (optional; recommended for in-game configuration menu)
  - [Download from GitHub](https://github.com/QTR-Modding/SKSE-Menu-Framework-3)
  - If not installed, configure via `arpghud.ini` + **F10** reload (see Configuration section)
- **xmake 3.0** or later and **C++23 compiler** (to build from source)
```

### 4.3 Files Affected in README
- **Main README.md:** 
  - Update "Requirements" section to list SKSE Menu Framework as optional
  - Replace F10-only configuration section with new "Configuration" section (in-game menu vs. manual INI)
  - Add comprehensive settings table
  - Add correct link to SKSE Menu Framework repo
  
- **POISE_GUIDE.txt:** 
  - Add note at top: "See **Configuration** section in main README for in-game menu access via SKSE Menu Framework"
  - No major changes needed (already documents INI poise settings)

- **arpghud.ini:** 
  - No changes; inline comments already document all settings

---

## 5. IMPLEMENTATION PHASES (5-PHASE ROADMAP)

### Phase 1: Foundation & Build Configuration *(2-3 hours)*

**Deliverables:**
- Download and integrate official `SKSEMenuFramework.h` into src/
- Create `src/menu_framework.h` abstraction layer (thin wrapper over official API)
- Add optional build flag to xmake.lua (`ARPGHUD_ENABLE_MENU` enabled by default)
- Create placeholder `src/menu_framework.cpp` with initialization stub

**Files to Create:**
- `src/SKSEMenuFramework.h` (copy from official repo)
- `src/menu_framework.h` (wrapper with soft dependency checks)
- `src/menu_framework.cpp` (initialization logic)

**Files to Modify:**
- `xmake.lua` (add define; ensure clean builds)
- `src/pch.h` (conditionally include menu framework header)
- `src/main.cpp` (add hook in `SKSEMessageHandler::kDataLoaded`)

**Build Test:** ✓ Compiles cleanly with and without menu code

---

### Phase 2: Dynamic Binding & Menu Registration *(3-4 hours)*

**Deliverables:**
- Implement `InitMenuFramework()` function
- Query SKSE Menu Framework module at runtime
- Create render callback for menu window
- Register menu window during `kDataLoaded` event
- Graceful fallback if framework absent

**Key Functions:**
```cpp
// In src/menu_framework.cpp
bool InitMenuFramework() {
    if (!SKSEMenuFramework::IsInstalled()) {
        logger::info("SKSE Menu Framework not installed; in-game menu disabled.");
        return false;
    }
    
    auto renderFunc = SKSEMenuFramework::GetMenuFrameworkFunction<
        SKSEMenuFramework::Model::AddWindowFunction
    >("AddWindow");
    
    if (!renderFunc) {
        logger::warn("Failed to bind SKSE Menu Framework AddWindow function.");
        return false;
    }
    
    menuWindow = renderFunc(OnMenuRender);
    return menuWindow != nullptr;
}

void OnMenuRender() {
    // Menu UI rendering (detailed in Phase 3)
    ImGui::Text("ArpgHud Configuration");
}
```

**Integration Point:**
```cpp
// In SKSEMessageHandler (main.cpp)
case SKSE::MessagingInterface::kDataLoaded:
    LoadSettings();
    BuiltinPoise::Install();
    // ... existing initialization ...
    
    // NEW: Register menu (after PrismaUI setup)
    InitMenuFramework();  // Soft dependency; no-op if framework absent
    
    logger::info("HUD initialization completed.");
    break;
```

**Build Test:** ✓ Plugin loads with menu window registered (if framework present)

---

### Phase 3: Menu UI & Settings Binding *(6-8 hours)*

**Deliverables:**
- ImGui menu layout with 5 collapsible sections
- Getter/setter callbacks for each setting
- Value validation (type conversion, range clamping)
- Real-time slider feedback (value display)

**Menu Structure in Code:**
```cpp
void OnMenuRender() {
    if (!ImGui::Begin("ArpgHud", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }
    
    if (ImGui::CollapsingHeader("Layout & Positioning", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderLayoutSection();
    }
    if (ImGui::CollapsingHeader("Widget Visibility", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderWidgetSection();
    }
    if (ImGui::CollapsingHeader("Dynamic Visibility", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderVisibilitySection();
    }
    if (ImGui::CollapsingHeader("Graphics Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderGraphicsSection();
    }
    if (ImGui::CollapsingHeader("Combat Balance", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderPoiseSection();
    }
    
    ImGui::End();
}
```

**Each Section Example (Layout):**
```cpp
void RenderLayoutSection() {
    // HUD Style enum
    int style = settings.hudStyle;
    const char* styles[] = {"ARPG Globes", "Minimalist Bars"};
    if (ImGui::Combo("HUD Style", &style, styles, 2)) {
        settings.hudStyle = style;
        OnHudStyleChanged(style);
    }
    
    // Globe Scale slider
    float globeScale = settings.globeScale;
    if (ImGui::SliderFloat("Globe Scale##display", &globeScale, 0.1f, 2.0f, "%.2f")) {
        OnSettingChanged("fGlobeScale", globeScale);
    }
    
    // Offsets with clamping
    int sideOffset = settings.sideOffset;
    if (ImGui::SliderInt("Side Offset", &sideOffset, 0, 2000)) {
        OnSettingChanged("iSideOffset", static_cast<float>(sideOffset));
    }
}
```

**Value Validation Pattern:**
```cpp
void OnSettingChanged(const std::string& key, float newValue) {
    // Validate against known ranges
    if (key == "fGlobeScale") {
        newValue = std::clamp(newValue, 0.1f, 2.0f);
    } else if (key == "iSideOffset") {
        newValue = std::clamp(newValue, 0.0f, 2000.0f);
    }
    // ... store validated value
}
```

**Build Test:** ✓ Menu displays all settings; sliders are interactive

---

### Phase 4: Live Persistence (HUD Updates + INI Writes) *(8-10 hours)*

**Deliverables:**
- Send setting changes to PrismaUI via `Invoke(hudView, "updateHudSettings(...)")`
- Apply poise changes via `BuiltinPoise::Configure()`
- Handle `iHudStyle` change with full DOM reload (only case)
- Debounced INI writes (batch + delay)
- Format-preserving INI file updates
- Error handling and fallback logic

**Update Function:**
```cpp
void ApplySetting(const std::string& key, float value) {
    // 1. Update in-memory struct
    if (key == "fGlobeScale") {
        settings.globeScale = value;
    } else if (key == "iHudStyle") {
        settings.hudStyle = static_cast<int>(value);
    }
    // ... more settings ...
    
    // 2. Queue INI write (debounced; see below)
    QueueINIWrite(key, value);
    
    // 3. Apply to systems
    if (key == "iHudStyle") {
        // Style changed: full DOM reload
        if (prismaUI && hudView != 0) {
            prismaUI->Invoke(hudView, "window.location.reload()");
        }
    } else if (IsPoiseSetting(key)) {
        // Poise setting: update system
        BuiltinPoise::Configure(poiseSettings);
    } else {
        // Standard HUD setting: live update
        SendSettingsToHUD();
    }
}

void SendSettingsToHUD() {
    if (!prismaUI || !hudView || settings.hudStyle == 1) {
        // Minimalist style doesn't use these settings
        return;
    }
    
    const auto script = std::format(
        "updateHudSettings({:.6f}, {:.6f}, {}, {}, {}, {}, {}, {:.6f}, {:.6f}, {}, {}, {:.6f}, {:.6f}, {:.6f})",
        settings.globeScale, settings.hotbarScale, settings.sideOffset, settings.bottomOffset,
        settings.showVitalNumbers, settings.alwaysOn, settings.dynamicHud, settings.idleOpacity,
        settings.fadeDelaySeconds, settings.hotbarBottomOffset, settings.glassEffects,
        settings.glassShineOpacity, settings.meniscusGlow, settings.orbInnerShadow);
    
    prismaUI->Invoke(hudView, script.c_str());
}
```

**Debounced INI Writes:**
```cpp
std::unordered_map<std::string, float> pendingINIWrites;
std::chrono::steady_clock::time_point lastINIFlush;
constexpr int DEBOUNCE_MS = 500;

void QueueINIWrite(const std::string& key, float value) {
    pendingINIWrites[key] = value;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastINIFlush).count();
    
    if (elapsed >= DEBOUNCE_MS) {
        FlushINIWrites();
        lastINIFlush = now;
    }
}

void FlushINIWrites() {
    for (const auto& [key, value] : pendingINIWrites) {
        if (!WriteSettingToINI(key, value)) {
            logger::warn("Failed to write {} = {} to arpghud.ini", key, value);
        }
    }
    pendingINIWrites.clear();
}

bool WriteSettingToINI(const std::string& key, float value) {
    // Get INI path (same as LoadSettings())
    // Read entire file
    // Find line matching key=oldValue (case-sensitive)
    // Replace value portion only
    // Write back to file
    // Return success
}
```

**Build Test:** ✓ Menu changes apply to HUD; INI updates are debounced

---

### Phase 5: Testing, Polish & Documentation *(4-6 hours)*

**Deliverables:**
- Full integration testing (with and without SKSE Menu Framework)
- Edge case handling (simultaneous INI edit + menu drag)
- Performance validation (debounce timing, no lag)
- Updated README.md with correct instructions and tables
- Logging/diagnostics for troubleshooting

**Testing Scenarios:**
1. ✓ Plugin loads without SKSE Menu Framework (menu gracefully disabled)
2. ✓ Plugin loads with SKSE Menu Framework (menu registers and renders)
3. ✓ Adjust slider in menu → HUD updates immediately
4. ✓ Adjust slider in menu → INI written (check file after 500ms)
5. ✓ Toggle `iHudStyle` 0→1 → DOM reloads; 1→0 → DOM reloads
6. ✓ Edit INI manually → F10 reload → menu values sync
7. ✓ Rapid slider drag → INI write is debounced (one write per 500ms)
8. ✓ INI write fails (e.g., read-only) → warning logged; gameplay unaffected
9. ✓ Change poise settings → take effect on next hit
10. ✓ Menu closes and reopens → values persist correctly

**Documentation Updates:**
- Revise README.md (Configuration section with menu access + INI tables)
- Update POISE_GUIDE.txt (cross-reference menu sections)
- Add code comments to `src/menu_framework.cpp`
- Add troubleshooting section to README

**Final Build Test:** ✓ Full integration test suite passes

---

## 6. KEY DECISIONS & CONSTRAINTS

| Aspect | Decision | Rationale |
|--------|----------|-----------|
| **Framework Binding** | Official `SKSEMenuFramework.h` + dynamic `GetModuleHandle` | Single header; built-in soft dependency handling; no static linking |
| **Build Configuration** | Menu enabled by default; ARPGHUD_ENABLE_MENU can be toggled | Users opt-in to menu feature; fallback always works |
| **DOM Reload** | Only on `iHudStyle` 0↔1 change; other settings use live push | Minimize visual disruption; preserve gameplay flow |
| **Slider Ranges** | ImGui sliders capped to practical bounds; manual INI allows higher | UX clarity; power users can override |
| **INI Debouncing** | 500ms batch writes; async timer | Prevent disk I/O spam during rapid slider drags |
| **Persistence Model** | INI write-back on every change (debounced) | Changes survive game restart; menu and INI stay in sync |
| **Fallback on I/O Failure** | Log warning; keep in-memory value | Gameplay not disrupted; user edits INI manually later if needed |
| **Manual INI + F10** | Reloads INI and syncs menu values | Coexist peacefully; F10 is the "resync" button |
| **Poise Updates** | Immediate via `BuiltinPoise::Configure()` | Take effect on next eligible hit/regen tick |

---

## 7. RISK MITIGATION

| Risk | Mitigation |
|------|-----------|
| **Menu Framework unavailable** | `SKSEMenuFramework::IsInstalled()` + graceful no-op; full INI fallback via F10 |
| **INI write race condition** | Debounced writes (500ms); timestamp validation on read |
| **Menu value out of sync with INI** | F10 reload re-reads INI and updates menu; optional "Reload from File" button |
| **PrismaUI invocation fails** | Log error; don't break gameplay; next F10 retry |
| **Slider drag performance** | Debounce INI writes; batching; async tasks |
| **Comment/format preservation** | Line-by-line parsing; avoid multiline regex |
| **Rapid style changes** | Queue reload; deduplicate consecutive reloads |
| **Menu not rendering** | Fallback: use INI + F10; log framework function binding errors |

---

## 8. BUILD INTEGRATION

### xmake.lua Changes

**Add optional menu compilation flag:**
```lua
-- Default: enabled (can be toggled to false during build)
add_defines("ARPGHUD_ENABLE_MENU=1")

-- Menu code conditionally compiled based on this flag
```

**No new package dependencies needed** (SKSEMenuFramework.h is a single header; include locally)

### File Structure
```
src/
├── SKSEMenuFramework.h          (official header; copy from repo)
├── menu_framework.h             (wrapper / abstraction)
├── menu_framework.cpp           (implementation)
├── main.cpp                     (modified: call InitMenuFramework())
├── pch.h                        (modified: conditional include)
└── ... (other files unchanged)
```

### Compilation Behavior
- **With ARPGHUD_ENABLE_MENU=1 (default):** Menu code compiled; all five sections available
- **With ARPGHUD_ENABLE_MENU=0:** Menu code excluded; DLL smaller; no menu overhead
- **Runtime:** Plugin detects `SKSEMenuFramework.dll`; gracefully skips menu if absent

---

## 9. IMPLEMENTATION CHECKLIST

### Phase 1: Foundation
- [ ] Download official `SKSEMenuFramework.h` from repo
- [ ] Create `src/menu_framework.h` (thin wrapper)
- [ ] Create `src/menu_framework.cpp` (stub implementation)
- [ ] Update `xmake.lua` (add ARPGHUD_ENABLE_MENU define)
- [ ] Update `src/pch.h` (conditional include guard)
- [ ] Update `src/main.cpp` (add InitMenuFramework() call)
- [ ] Verify builds cleanly (with and without menu)

### Phase 2: Dynamic Binding
- [ ] Implement `InitMenuFramework()` (IsInstalled check, GetModuleHandle binding)
- [ ] Create `OnMenuRender()` callback stub
- [ ] Register menu window via framework API
- [ ] Log framework availability at startup
- [ ] Verify plugin loads with/without framework

### Phase 3: Menu UI
- [ ] Implement `RenderLayoutSection()` (6 controls)
- [ ] Implement `RenderWidgetSection()` (4 controls)
- [ ] Implement `RenderVisibilitySection()` (4 controls)
- [ ] Implement `RenderGraphicsSection()` (4 controls)
- [ ] Implement `RenderPoiseSection()` (24+ controls)
- [ ] Add value validation callbacks
- [ ] Verify all sliders/toggles render correctly

### Phase 4: Live Persistence
- [ ] Implement `ApplySetting()` (in-memory update dispatch)
- [ ] Implement `SendSettingsToHUD()` (PrismaUI invoke)
- [ ] Implement `OnHudStyleChanged()` (DOM reload logic)
- [ ] Implement `QueueINIWrite()` + `FlushINIWrites()` (debouncing)
- [ ] Implement `WriteSettingToINI()` (format-preserving file I/O)
- [ ] Test debouncing (slider drag → verify single INI write per 500ms)
- [ ] Test style toggle (verify DOM reload only on 0↔1 change)

### Phase 5: Testing & Documentation
- [ ] Test with SKSE Menu Framework installed
- [ ] Test without SKSE Menu Framework (fallback to INI)
- [ ] Test edge cases (rapid slider drag, INI edit + F10, style toggle)
- [ ] Performance validation (no lag, smooth interaction)
- [ ] Update README.md (Configuration section + tables)
- [ ] Update POISE_GUIDE.txt (cross-reference)
- [ ] Final integration test suite

---

## SUMMARY

This **revised plan** leverages the official `SKSEMenuFramework.h` single-header API with built-in soft dependency handling. Key improvements:

✅ **No custom ABI:** Use official framework function pointers  
✅ **No DOM thrashing:** Live updates without `window.location.reload()` (except style change)  
✅ **Practical slider bounds:** UX-friendly ranges in ImGui; manual INI for power users  
✅ **Correct README:** Reference official GitHub repo; explain overlay hotkey access  
✅ **Debounced persistence:** 500ms batch INI writes prevent disk spam  
✅ **5 implementation phases:** ~23-31 total hours; clear milestones and deliverables  

**Ready to proceed with Phase 1 (Foundation & Build Configuration).**


