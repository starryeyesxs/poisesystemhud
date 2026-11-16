# SKSE Menu Framework Integration — Quick Reference

**Status:** READY FOR PHASE 1 IMPLEMENTATION  
**Total Estimated Time:** 23–31 hours across 5 phases  
**Last Updated:** 2026-11-13

---

## 📋 Key Implementation Details

### Official API
- **Header:** `SKSEMenuFramework.h` (single file; copy to `src/`)
- **Source:** https://github.com/QTR-Modding/SKSE-Menu-Framework-3
- **Soft Dependency:** Built-in via `GetModuleHandle()` + `IsInstalled()` checks

### Build Configuration
- **Add to xmake.lua:** `add_defines("ARPGHUD_ENABLE_MENU=1")`
- **Include in pch.h:** Conditionally include `SKSEMenuFramework.h`
- **Files to Create:** `src/menu_framework.h`, `src/menu_framework.cpp`
- **Files to Modify:** `xmake.lua`, `pch.h`, `main.cpp`

### Menu Structure (5 Sections)
1. **Layout & Positioning:** 6 controls (style, scales, offsets)
2. **Widget Visibility:** 4 toggles (weather, light gem, vitals, poise bar)
3. **Dynamic Visibility:** 4 controls (dynamic, always-on, idle opacity, fade delay)
4. **Graphics Effects:** 4 sliders (glass effects, shine, glow, shadow)
5. **Combat Balance:** 24+ sliders (poise tuning, multipliers, regen)

### Live Update Strategy
| Setting Type | Update Method | Debounce | DOM Reload |
|--------------|---------------|----------|-----------|
| Layout/scale/offset | `updateHudSettings()` JS | None | No |
| Widget toggles | `updateHudSettings()` JS | None | No |
| Visibility settings | `updateHudSettings()` JS | None | No |
| Graphics settings | `updateHudSettings()` JS | None | No |
| **HUD Style (0↔1)** | **DOM reload** | **N/A** | **YES** |
| Poise settings | `BuiltinPoise::Configure()` | None | No |
| **INI write** | **Batched queue** | **500ms** | **N/A** |

### ImGui Slider Ranges (Practical Bounds)
- `fBasePoise`: 10–500 (manual INI: 1–100k)
- `fRegenDelaySeconds`: 0.0–15.0 (manual INI: 0–600)
- `fRegenRatePerSecond`: 1.0–50.0 (manual INI: 0–10k)
- Multipliers: 0.0–5.0 range (manual INI: 0–10)
- All ranges clamp to INI limits; higher values override in config file

### INI Persistence
- **Debouncing:** 500ms batch writes (collect changes, write once)
- **Format Preservation:** Line-by-line parsing; keep comments/structure intact
- **Fallback:** If write fails, log warning; in-memory value persists
- **Resync:** F10 reload re-reads INI; menu values update to match

### README Updates
- **Location:** "Configuration" section (replaces F10-only docs)
- **Add:** SKSE Menu Framework as optional dependency
- **Explain:** Overlay hotkey access (not pause menu; not SkyUI MCM)
- **Include:** Full settings table (HUD, Graphics, Poise sections)
- **Link:** https://github.com/QTR-Modding/SKSE-Menu-Framework-3 (correct repo)

---

## 🔧 Phase Breakdown

### Phase 1: Foundation (2–3 hrs)
- Integrate official `SKSEMenuFramework.h`
- Create wrapper `src/menu_framework.h`
- Stub `src/menu_framework.cpp`
- Update xmake & pch
- ✓ Plugin compiles cleanly (with/without menu)

### Phase 2: Dynamic Binding (3–4 hrs)
- Implement `InitMenuFramework()`
- Query framework module; register menu window
- Graceful fallback if framework absent
- ✓ Menu window registers and renders (if framework present)

### Phase 3: Menu UI (6–8 hrs)
- ImGui layout with 5 collapsible sections
- Getter/setter callbacks per setting
- Value validation & range clamping
- ✓ All menu controls interactive; values display correctly

### Phase 4: Live Persistence (8–10 hrs)
- `ApplySetting()` dispatch logic
- `SendSettingsToHUD()` (PrismaUI invoke)
- `OnHudStyleChanged()` (DOM reload on style toggle)
- Debounced INI write (500ms batch)
- Format-preserving file I/O
- ✓ Slider drag → HUD updates live; INI written (debounced); poise applies immediately

### Phase 5: Testing & Docs (4–6 hrs)
- Integration tests (with/without framework)
- Edge cases (rapid drag, manual INI edit + F10, style toggle)
- Performance validation (no lag)
- Update README & POISE_GUIDE
- ✓ Full suite passes; documentation complete

---

## ⚙️ Core Code Patterns

### Check Framework Availability
```cpp
if (!SKSEMenuFramework::IsInstalled()) {
    logger::info("SKSE Menu Framework not installed; menu disabled.");
    return;
}
```

### Register Menu Window
```cpp
auto addWindow = GetMenuFrameworkFunction<SKSEMenuFramework::Model::AddWindowFunction>("AddWindow");
if (addWindow) {
    menuWindow = addWindow(OnMenuRender);
}
```

### Update HUD Setting (Live)
```cpp
const auto script = std::format("updateHudSettings({:.6f}, ...)", settings.globeScale);
prismaUI->Invoke(hudView, script.c_str());
```

### Update Poise (Immediate)
```cpp
BuiltinPoise::Configure(poiseSettings);
```

### Debounced INI Write
```cpp
void QueueINIWrite(const std::string& key, float value) {
    pendingWrites[key] = value;
    auto elapsed = duration_cast<milliseconds>(now - lastFlush).count();
    if (elapsed >= 500) FlushINIWrites();
}
```

### DOM Reload (Style Only)
```cpp
if (key == "iHudStyle") {
    prismaUI->Invoke(hudView, "window.location.reload()");
}
```

---

## 📁 File Checklist

### Create (New Files)
- [ ] `src/SKSEMenuFramework.h` (copy from official repo)
- [ ] `src/menu_framework.h` (wrapper)
- [ ] `src/menu_framework.cpp` (implementation; ~500–700 lines)

### Modify (Existing Files)
- [ ] `xmake.lua` (add ARPGHUD_ENABLE_MENU define)
- [ ] `src/pch.h` (conditional include)
- [ ] `src/main.cpp` (call InitMenuFramework in kDataLoaded)
- [ ] `README.md` (Configuration section + tables)
- [ ] `POISE_GUIDE.txt` (cross-reference menu)

### Unchanged
- `arpghud.ini` (no changes; comments already explain settings)

---

## ✅ Verification Checkpoints

After each phase:
- **Phase 1:** `xmake -y` compiles without errors
- **Phase 2:** Plugin loads; menu window visible (if framework present)
- **Phase 3:** All 5 menu sections render; controls interactive
- **Phase 4:** Slider → HUD updates live; INI file written (debounced); poise applies
- **Phase 5:** Full integration tests pass; docs updated

---

## 🎯 Success Criteria

1. ✓ Plugin loads cleanly **without** SKSE Menu Framework (INI + F10 fallback works)
2. ✓ Plugin loads cleanly **with** SKSE Menu Framework (menu registers and renders)
3. ✓ Menu changes apply to HUD **immediately** (no `window.location.reload()` except style)
4. ✓ INI file updated after every menu change (debounced; no spam)
5. ✓ Format preserved in INI (comments, section headers, whitespace intact)
6. ✓ Slider range clamp to practical bounds; manual INI can go higher
7. ✓ README explains menu access via overlay hotkey (not pause menu)
8. ✓ F10 reload syncs menu values if INI manually edited
9. ✓ All poise settings take effect immediately (next hit/regen tick)
10. ✓ Zero performance regression (debouncing prevents disk I/O spam)

---

## 📖 README Section (Preview)

```markdown
## Configuration

### In-Game Menu (Recommended)

If [SKSE Menu Framework](https://github.com/QTR-Modding/SKSE-Menu-Framework-3) is installed:

1. Press the **SKSE Menu Framework overlay hotkey** (default: **Right Ctrl + Right Shift**)
2. Select **ArpgHud** from the mod list
3. Adjust settings; changes apply immediately

### Manual Configuration (INI)

Edit `Data/SKSE/Plugins/arpghud.ini` and press **F10** in-game to reload.

### Settings Reference

[Full table of all settings with type, range, default, description]
```

---

## 🚀 Ready to Begin?

All preparations complete. Starting **Phase 1: Foundation & Build Configuration** now.

Expected deliverable: Plugin compiles cleanly with menu framework integration layer in place.
