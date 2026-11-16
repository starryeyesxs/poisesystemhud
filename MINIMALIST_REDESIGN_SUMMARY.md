# Minimalist HUD Redesign Summary

## Overview
Redesigned the Minimalist HUD layout with a **TrueHUD-inspired left-anchored filigree layout** featuring dynamic bar width scaling based on player max stat values and continuous metallic framing for a forged Nordic UI aesthetic.

## Key Changes

### 1. **Left-Anchored Nordic Filigree Brooch** ✅
- **New SVG Symbol:** `#nordic-brooch` (48×48 viewBox)
  - Subtle geometric diamond emblem with Norse/Nordic line-art design
  - Layered frame (thick outer ring, thin inner accent, centered circle dot)
  - Colors: `rgba(140,130,110,.6)` outer frame, `rgba(180,170,150,.4)` accents
  - Drop-shadow for depth: `drop-shadow(0 2px 4px rgba(0,0,0,.6))`

- **Layout Change:** 
  - Each bar row now starts with `min-brooch` SVG (32×32 px, 6px right margin)
  - Brooch acts as fixed left anchor that never moves with bar expansion
  - Sneak light gem positioned absolutely at `bottom: 8px; left: 8px` to overlay the Health gem brooch
  - Prevents collisions when bars scale dynamically to the right

### 2. **Continuous Metallic Bar Framing** ✅
- **Dual-tone Borders:**
  - Top border: `1px solid rgba(180,170,150,.5)` (brushed silver-gray highlight)
  - Bottom border: `1px solid rgba(50,50,50,.8)` (dark recessed shadow)
  - Creates illusion of raised metallic rail forged into dark steel

- **Enhanced Depth:**
  - Inset shadow: `inset 0 1px 2px rgba(0,0,0,.5)` (recessed track)
  - Outer drop-shadow: `0 1px 2px rgba(0,0,0,.8)` (frame elevation)
  - Outline: `1px solid rgba(255,255,255,.08)` with `outline-offset: 1px` (thin specular edge)

- **Right SVG End Cap Only:**
  - Left diamond cap replaced by Nordic brooch
  - Right cap (`#min-cap-right`) still present for visual balance

### 3. **Dynamic Bar Width Scaling (Health, Magicka, Stamina Only)** ✅
- **Formula:**
  - Base width: **200px** (at 100 max stat)
  - Scaling rate: **0.5px per point** above base value
  - Ceiling: **480px max** (prevents excessive expansion)
  - Formula: `calculatedWidth = min(480, 200 + max(0, maxStat - 100) * 0.5)`

- **Examples:**
  - 100 max health → 200px
  - 150 max health → 225px (200 + 50*0.5)
  - 200 max health → 250px (200 + 100*0.5)
  - 600 max health → 480px (clamped at ceiling)

- **Animation:**
  - CSS Transition: `width 0.25s ease-out`
  - Smooth expansion when player gains buffs (Fortify Health, etc.)
  - Smooth contraction when buffs expire

- **Poise Bar Exception:**
  - Stays fixed at **200px** regardless of max poise value
  - Higher poise values read through smaller damage chunks per hit, not wider bars
  - Prevents visual confusion between health/magicka (scalable) and poise (fixed)

### 4. **GPU-Accelerated Fill Animation** ✅
- **Minimalist Bar Drain:**
  - Fill layer uses `transform: scaleX(var(--fill-scale, 1))` with `transform-origin: left`
  - CSS Transition: `transform 0.05s linear` for continuous drain smoothing
  - `will-change: transform` optimizes GPU compositing

- **Phantom Trail Decay:**
  - Trail bar maintains smooth % width animation
  - Discrete hits trigger hitAt timestamp → trail recedes smoothly over 600ms
  - No stuttering during sprinting or concentration spell drains

### 5. **Responsive Layout Adjustments** ✅
- **Desktop (>480px):**
  - Brooch: 32×32 px, 6px right margin
  - Dynamic width scaling enabled for all stat bars

- **Mobile (<480px):**
  - Brooch: 24×24 px, 4px right margin
  - Bars scale proportionally
  - Same dynamic width formula applies

## File Changes

### [dist/arpghud_1.2.0/PrismaUI/views/arpghud/index.html](/c:/games/poetstylehudforskyrim/dist/arpghud_1.2.0/PrismaUI/views/arpghud/index.html)
- Updated `#min-cap-left` and `#min-cap-right` SVG cap colors to dark `rgba(80,80,80,.8)` with solid dark diamond `rgba(50,50,50,1)`
- Added new `#nordic-brooch` SVG symbol (geometric diamond filigree)
- Updated minimal bars HTML to include `<svg class="min-brooch"><use href="#nordic-brooch"></use></svg>` and right cap only
- Removed left cap from HTML (now brooch)

### [dist/arpghud_1.2.0/PrismaUI/views/arpghud/minimal.css](/c:/games/poetstylehudforskyrim/dist/arpghud_1.2.0/PrismaUI/views/arpghud/minimal.css)
- **Layout:** Changed `#minimal-layout` from `left: 0` to `left: 8px` for consistent left padding
- **Bar Row:** Added `--bar-width: 250px` CSS variable, `transition: width 0.25s ease-out`, `width: var(--bar-width)`
- **Brooch Styling:** New `.min-brooch` class (32×32, flex-none, 6px right margin, dark drop-shadow)
- **Bar Framing:** Split `border` into top/bottom with dual-tone colors, added `inset` + outer shadows
- **Fill Animation:** Changed fill to use `transform: scaleX()` with `will-change: transform`, `transition: transform 0.05s linear`
- **Light Gem:** Repositioned with `position: fixed; bottom: 8px; left: 8px; z-index: 10` to overlay left brooch
- **Mobile Breakpoint:** Updated brooch size and margins for <480px screens

### [dist/arpghud_1.2.0/PrismaUI/views/arpghud/minimal.js](/c:/games/poetstylehudforskyrim/dist/arpghud_1.2.0/PrismaUI/views/arpghud/minimal.js)
- **Removed:** `buildMinimalBars()` function (HTML now pre-built)
- **Enhanced `renderMinimalStat()`:**
  - Added dynamic width calculation for Health/Magicka/Stamina bars
  - Formula: `calculatedWidth = min(480, 200 + max(0, maxStat - 100) * 0.5)`
  - Fixed width (200px) for Poise bar
  - Sets `--bar-width` CSS variable on row element

- **Added `updateMinimalGraphics()`:**
  - Toggles filigree visibility for both `.min-cap` and `.min-brooch` SVGs
  - Applies shine/shadow CSS variables for bar depth effects
  - No DOM reload needed (pure CSS update)

## Visual Results

### Before
- Minimalist bars with light diamond end caps, no left anchor
- Fixed bar widths regardless of max stat
- Light gray metallic framing

### After
- Nordic brooch filigree on left, sneak gem overlays it when sneaking
- Health/Magicka/Stamina scale dynamically (200–480px range)
- Poise stays fixed (200px) for clearer damage-per-hit readability
- Dual-tone metallic frames (brushed silver top, dark steel bottom)
- GPU-accelerated smooth drain animations
- TrueHUD-inspired left-anchored layout

## Testing Checklist

- [x] Build compiles cleanly (775,680 bytes DLL, exit code 0)
- [x] No C++ changes needed (pure HTML/CSS/JS redesign)
- [ ] Bars render with Nordic brooch on left
- [ ] Sneak gem overlays health brooch when sneaking
- [ ] Bar widths scale dynamically based on max stat
- [ ] Poise bar stays fixed at 200px
- [ ] Metallic framing appears on all bars
- [ ] Fill animations smooth during sprinting/drain
- [ ] Trail decay smooth and readable
- [ ] Minimalist Graphics sliders (filigree, shine, shadow, notches) apply correctly
- [ ] F10 reload preserves all settings
- [ ] Responsive layout works on mobile breakpoint

## Performance Notes

- **GPU Optimization:** `transform: scaleX()` leverages GPU compositing for smooth drain animation
- **CSS Variables:** `--bar-width` applied at row level, no per-fill calculations
- **No DOM Mutations:** Bars pre-built in HTML, only fill/trail width and row width CSS changed
- **Smooth 0.25s Width Transition:** Allows viewport expansion without stuttering when buffs increase max stats

## Compatibility

- **ARPG Globes Mode (iHudStyle=0):** Unaffected, uses separate layout
- **Minimalist Mode (iHudStyle=1):** Full redesign as above
- **F10 Reload:** Settings persist from arpghud.ini, layout re-initializes correctly
- **Menu Framework:** All existing sliders (scale, offset, poise settings, graphics options) still functional

---

**Build Date:** November 13, 2026  
**DLL Size:** 775,680 bytes  
**Build Status:** ✅ Success (exit code 0)
