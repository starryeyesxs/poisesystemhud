# SKSE Menu Framework Integration — Plan Index

**Date:** 2026-11-13  
**Status:** ✅ READY FOR PHASE 1 IMPLEMENTATION  
**Estimated Total Time:** 23–31 hours across 5 phases

---

## 📚 Documentation Files

### 1. **IMPLEMENTATION_PLAN.md** ← Start here for deep dive
**17 KB | Complete Specification**

The comprehensive master plan document. Contains:

- **Section 1:** Current INI & Config Architecture
  - Complete cataloguing of all 41+ settings
  - INI structure and how F10 reload works
  - Live update flow diagram

- **Section 2:** SKSE Menu Framework Integration  
  - Official SKSEMenuFramework.h API overview
  - Soft dependency strategy (dynamic binding)
  - Runtime framework detection

- **Section 3:** Menu Organization & Live Persistence
  - 5-section menu layout (Layout, Widgets, Visibility, Graphics, Poise)
  - ImGui slider practical bounds vs. INI ranges
  - Live update strategy (NO DOM reload except style toggle)
  - Debounced INI writes (500ms batch)
  - Format-preserving INI file updates

- **Section 4:** README Update Plan
  - Correct SKSE Menu Framework repo link
  - Overlay hotkey access instructions
  - Complete settings reference tables
  - Files to modify (README.md, POISE_GUIDE.txt)

- **Section 5:** Implementation Phases (5 phases, 23–31 hours)
  - Phase 1: Foundation & Build Config (2–3 hrs)
  - Phase 2: Dynamic Binding & Registration (3–4 hrs)
  - Phase 3: Menu UI & Settings Binding (6–8 hrs)
  - Phase 4: Live Persistence & INI Writes (8–10 hrs)
  - Phase 5: Testing, Polish & Documentation (4–6 hrs)

- **Sections 6–9:** Key decisions, risk mitigation, build integration, checklist

**Use:** For understanding the full strategy and all details

---

### 2. **IMPLEMENTATION_SUMMARY.md** ← Quick reference guide
**7 KB | Developer Cheat Sheet**

Quick-reference document for implementing. Contains:

- Key Implementation Details table
- Phase Breakdown (visual summary with hours)
- Core Code Patterns (copy-paste ready)
  - Framework availability check
  - Menu window registration
  - HUD setting updates
  - Poise configuration
  - Debounced INI writes
  - DOM reload (style only)
- File Checklist (create/modify/unchanged)
- Verification Checkpoints (after each phase)
- Success Criteria (10-point validation)
- README Preview (markdown template)

**Use:** For quick lookups during development; verification after each phase

---

### 3. **PHASE1_CHANGES.md** ← Implementation steps
**9 KB | Exact Changes Needed**

Step-by-step instructions for Phase 1. Contains:

- **Files to Create (3 files):**
  1. `src/SKSEMenuFramework.h` (download from official repo)
  2. `src/menu_framework.h` (wrapper abstraction)
  3. `src/menu_framework.cpp` (initialization stub)

- **Files to Modify (4 files):**
  1. `xmake.lua` (add ARPGHUD_ENABLE_MENU define)
  2. `src/pch.h` (conditional includes)
  3. `src/main.cpp` (initialize menu framework)
  4. `src/main.cpp` (add plugin unload hook)

- Build Verification Checklist
- Success Criteria
- Summary table of changes

**Use:** For actually making Phase 1 changes; follow step-by-step

---

## 🎯 Plan Overview at a Glance

### Official API Integration
- ✅ Use **SKSEMenuFramework.h** (single header from official repo)
- ✅ Dynamic binding via `GetModuleHandle()` + `GetProcAddress()`
- ✅ Soft dependency built-in (`IsInstalled()` check)
- ✅ No custom ABI wrappers needed

### Smart Update Strategy
| Update Type | Method | DOM Reload | Debounce |
|-------------|--------|-----------|----------|
| Scale/offset | `updateHudSettings()` JS | No | None |
| Toggles | `updateHudSettings()` JS | No | None |
| **HUD Style** | **DOM reload** | **YES** | **N/A** |
| Poise settings | `BuiltinPoise::Configure()` | No | None |
| INI writes | Batched queue | N/A | 500ms |

### Menu Structure (5 Sections)
1. **Layout & Positioning** (6 controls)
2. **Widget Visibility** (4 toggles)
3. **Dynamic Visibility** (4 controls)
4. **Graphics Effects** (4 sliders)
5. **Combat Balance** (24+ sliders)

### Practical Slider Ranges
- Base Poise: 10–500 (manual INI: 1–100k)
- Regen Delay: 0–15 sec (manual INI: 0–600)
- Multipliers: 0–5 range (manual INI: 0–10)
- All clamp to INI limits; manual values override

### INI Persistence
- **Debouncing:** 500ms batch writes (prevent disk spam)
- **Format:** Line-by-line parsing (preserve comments/structure)
- **Fallback:** If write fails, log warning; in-memory value persists
- **Resync:** F10 reload re-reads INI; menu values update

---

## 📊 5-Phase Implementation Roadmap

```
Phase 1  Foundation & Build Config          [ 2–3 hrs  ]  ← START HERE
├─ Download SKSEMenuFramework.h
├─ Create menu_framework.h wrapper
├─ Create menu_framework.cpp stub
├─ Update xmake.lua
├─ Update pch.h
└─ Update main.cpp

Phase 2  Dynamic Binding & Registration     [ 3–4 hrs  ]
├─ Implement InitMenuFramework()
├─ Query framework via GetModuleHandle()
├─ Register menu window
└─ Graceful fallback if framework absent

Phase 3  Menu UI & Settings Binding         [ 6–8 hrs  ]
├─ ImGui layout (5 sections)
├─ Callbacks and validation
├─ Real-time feedback
└─ All controls interactive

Phase 4  Live Persistence & INI Writes      [ 8–10 hrs ]
├─ ApplySetting() dispatch
├─ PrismaUI.Invoke() for HUD updates
├─ DOM reload (style only)
├─ Debounced INI writes
└─ Error handling & fallback

Phase 5  Testing, Polish & Documentation    [ 4–6 hrs  ]
├─ Integration tests (with/without framework)
├─ Edge case validation
├─ Performance validation
├─ Update README.md
└─ Final verification

                                  TOTAL: 23–31 hours
```

---

## ✅ Success Criteria

1. ✓ Plugin loads cleanly **without** SKSE Menu Framework (INI + F10 fallback works)
2. ✓ Plugin loads cleanly **with** SKSE Menu Framework (menu registers)
3. ✓ Menu changes apply to HUD **immediately** (no DOM reload except style)
4. ✓ INI file updated after every menu change (debounced)
5. ✓ Format preserved in INI (comments, section headers, whitespace intact)
6. ✓ Slider ranges capped to practical bounds
7. ✓ README explains overlay hotkey menu access (not pause menu)
8. ✓ F10 reload syncs menu values if INI manually edited
9. ✓ All poise settings take effect immediately
10. ✓ Zero performance regression (debouncing prevents disk spam)

---

## 🚀 Getting Started

### For Implementation Details:
Read **IMPLEMENTATION_PLAN.md** (sections 1–5 cover architecture, strategy, and examples)

### For Quick Reference:
Use **IMPLEMENTATION_SUMMARY.md** (tables, code patterns, checklists)

### For Phase 1 Execution:
Follow **PHASE1_CHANGES.md** (exact step-by-step changes)

### Build and Verify:
```powershell
# After making Phase 1 changes:
xmake clean
xmake -y

# Expected: Clean compilation, no errors
# Check log: Menu framework availability message
```

---

## 📋 Key Adjustments Applied

Your four corrections have been fully integrated:

1. ✅ **Official API:** Now uses `SKSEMenuFramework.h` directly (no custom wrapper)
2. ✅ **No DOM Reloads:** Only DOM reloads when `iHudStyle` changes 0↔1; all other changes via live `updateHudSettings()`
3. ✅ **Practical Ranges:** ImGui sliders capped (10–500 for poise, 0–15 for delays, 0–5 for mults); manual INI allows higher
4. ✅ **README Updates:** Correct SKSE Menu Framework GitHub link + overlay hotkey instructions (not pause menu)

---

## 📝 Files Already Created

```
C:\games\poetstylehudforskyrim\
├── IMPLEMENTATION_PLAN.md              (17 KB) ← Main plan document
├── IMPLEMENTATION_SUMMARY.md           (7 KB)  ← Quick reference
├── PHASE1_CHANGES.md                  (9 KB)  ← Phase 1 steps
└── PLAN_INDEX.md                      (this file)
```

---

## Next Steps

**You are ready to begin Phase 1.**

1. Read **PHASE1_CHANGES.md** for exact implementation steps
2. Start with Step 1: Download `SKSEMenuFramework.h` from official repo
3. Follow through Step 7: Verify clean compilation
4. Proceed to Phase 2 when Phase 1 is complete

---

## Questions?

Refer to the appropriate document:
- **"What is the overall strategy?"** → IMPLEMENTATION_PLAN.md (Sections 1–4)
- **"What code patterns should I use?"** → IMPLEMENTATION_SUMMARY.md (Core Code Patterns)
- **"What exactly do I need to change?"** → PHASE1_CHANGES.md (Files to Create/Modify)
- **"How do I verify Phase 1 is done?"** → PHASE1_CHANGES.md (Success Criteria)

---

**Status:** ✅ ALL PLANNING COMPLETE — READY TO IMPLEMENT PHASE 1
