# HUD Bar and Light Diamond Reconnaissance

## Rendering files

The player HUD bars are implemented entirely in:

- `view/index.html` - markup and shared inline SVG definitions.
- `view/minimal.css` - themes, sizing, bevels, fills, and effects.
- `view/minimal.js` - bar construction, themes, scaling, and dynamic light handling.

No external PNG/SVG assets are used for these bars. All visible geometry is inline SVG or CSS.

## Health, Magicka, Stamina, and Poise structure

The four row containers are declared in `view/index.html`:

```html
<div class="min-row hidden" id="min-health" data-stat="health"></div>
<div class="min-row hidden" id="min-magicka" data-stat="magicka"></div>
<div class="min-row hidden" id="min-stamina" data-stat="stamina"></div>
<div class="min-row min-thin hidden" id="min-poise" data-stat="poise"></div>
```

`buildMinimalBars()` in `view/minimal.js` dynamically creates the same structure for every row:

```js
row.innerHTML =
    `<svg class="min-cap"><use href="#${cap}-left"></use></svg>` +
    `<div class="min-track"><div class="min-layer min-trail min-${key}"></div>` +
    `<div class="min-layer min-fill min-${key}"></div></div>` +
    `<svg class="min-cap"><use href="#${cap}-right"></use></svg>` +
    daedricFrameMarkup();
```

Shared bar fill palettes are in `view/minimal.css`:

```css
#minimal-layout .min-health  { --c1: #c43a3a; --c2: #7a1414; }
#minimal-layout .min-magicka { --c1: #3d6fd0; --c2: #16336f; }
#minimal-layout .min-stamina { --c1: #3fae5c; --c2: #1a5a2c; }
#minimal-layout .min-poise   { --c1: #e0b43a; --c2: #8a6410; }
```

Every bar uses `.min-row`, `.min-track`, `.min-layer`, `.min-fill`, and `.min-trail`.

## Minimalist theme spike caps

The spike symbols are inline SVG `<symbol>` elements in `view/index.html`:

```html
<symbol id="min-cap-left" viewBox="0 0 22 16">...</symbol>
<symbol id="min-cap-right" viewBox="0 0 22 16">...</symbol>
<symbol id="min-cap-thin-left" viewBox="0 0 14 10">...</symbol>
<symbol id="min-cap-thin-right" viewBox="0 0 14 10">...</symbol>
```

Relevant symbol definitions:

```html
<symbol id="min-cap-left" viewBox="0 0 22 16">
  <path fill="rgba(10,11,13,.98)" d="M22 1 H10 L2 8 L10 15 H22 Z"/>
  <path fill="none" stroke="#0f1013" stroke-width="2"
        stroke-linejoin="miter" d="M22 1 H10 L2 8 L10 15 H22"/>
  <path fill="#3c4047" d="M8.5 8 L11 5.5 L13.5 8 L11 10.5 Z"/>
</symbol>
<symbol id="min-cap-right" viewBox="0 0 22 16">
  <path fill="rgba(10,11,13,.98)" d="M0 1 H12 L20 8 L12 15 H0 Z"/>
  <path fill="none" stroke="#0f1013" stroke-width="2"
        stroke-linejoin="miter" d="M0 1 H12 L20 8 L12 15 H0"/>
  <path fill="#3c4047" d="M8.5 8 L11 5.5 L13.5 8 L11 10.5 Z"/>
</symbol>
```

Poise uses the thin 14x10 symbol pair. Health, Magicka, and Stamina use the 22x16 pair.

The row references the left/right symbols with separate SVG `<use>` elements:

```js
`<svg class="min-cap"><use href="#${cap}-left"></use></svg>`
`<svg class="min-cap"><use href="#${cap}-right"></use></svg>`
```

Left and right symbols are separately drawn rather than mirrored by CSS or SVG transforms.

In `view/minimal.css`, cap sizing follows the row height:

```css
#minimal-layout .min-row {
    --bar-h: 17px;
}

#minimal-layout #min-health {
    --bar-h: 19px;
}

#minimal-layout .min-thin {
    --bar-h: 13px;
}

#minimal-layout .min-cap {
    flex: none;
    display: block;
    width: calc(var(--bar-h) * 1.375);
    height: var(--bar-h);
    filter: drop-shadow(0 0 .6px rgba(185,192,202,.85));
}

#minimal-layout .min-thin .min-cap {
    width: calc(var(--bar-h) * 1.4);
    height: var(--bar-h);
}
```

The ratios preserve the source art proportions:

- Standard: `22 / 16 = 1.375`
- Thin: `14 / 10 = 1.4`

In the Minimalist theme, `.no-filigree` hides `.min-cap`:

```css
#minimal-layout.no-filigree .min-cap {
    display: none;
}
```

## Daedric Iron spike caps

The Daedric theme does not use the Minimalist `<use>` caps. `daedricCapMarkup()` in `view/minimal.js` creates a separate inline SVG for each side:

```js
function daedricCapMarkup(isRight) {
    return [
        `<svg class="dd-cap ${isRight ? 'right' : ''}" viewBox="0 0 26 24" aria-hidden="true">`,
        `<polygon points="1,12 11,7.5 9.5,12 11,16.5"
                  fill="url(#spearBlade)" stroke="#111" stroke-width="0.8"/>`,
        `<path d="M26,2 L15,2 L9.5,12 L15,22 L26,22 L26,18.5
                  L17,18.5 L13.5,12 L17,5.5 L26,5.5 Z"
              fill="url(#ironGrad)" />`,
        `<polyline points="26,2 15,2 9.5,12 15,22 26,22"
                   fill="none" stroke="url(#silverBevel)" stroke-width="1"/>`,
        `<polyline points="26,5.5 17,5.5 13.5,12 17,18.5 26,18.5"
                   fill="none" stroke="url(#silverBevel)" stroke-width="0.9"/>`,
        `<path d="M19,3.8 L22,3.8 M20.5,2.7 L20.5,4.8
                  M19,20.2 L22,20.2 M20.5,19.2 L20.5,21.3"
              stroke="#cfd5e0" stroke-width="0.8" opacity="0.95"/>`,
        `</svg>`
    ].join('');
}
```

The left/right Daedric caps share the same generated markup. The right cap is mirrored horizontally in CSS:

```css
#minimal-layout.theme-daedric .dd-cap.right {
    transform: scaleX(-1);
}
```

Daedric sizing and frame behavior:

```css
#minimal-layout.theme-daedric #min-health,
#minimal-layout.theme-daedric #min-magicka,
#minimal-layout.theme-daedric #min-stamina,
#minimal-layout.theme-daedric #min-poise {
    --bar-h: 28px;
}

#minimal-layout.theme-daedric .min-cap {
    display: none;
}

#minimal-layout.theme-daedric .min-track {
    position: absolute;
    left: 23px;
    right: 23px;
    top: 6px;
    bottom: 6px;
    height: auto;
}

#minimal-layout.theme-daedric .dd-cap {
    width: 30px;
    height: 28px;
    flex: none;
}

#minimal-layout.theme-daedric .dd-mid {
    height: 28px;
    flex: 1;
}

#minimal-layout.theme-daedric .dd-sigil {
    width: 112px;
    height: 28px;
    left: 50%;
    transform: translateX(-50%);
}
```

The middle rail SVG uses `preserveAspectRatio="none"` and `vector-effect="non-scaling-stroke"` for its lines. This keeps stroke width stable while allowing the rail to stretch horizontally.

The Daedric frame is also shared across all four rows. Poise numbers receive a higher stacking level:

```css
#minimal-layout.theme-daedric #min-poise-numbers {
    z-index: 3;
}
```

## Steel-Iron light diamond widget

### Shared/Daedric palette and gradients

The global SVG gradients are defined in `view/index.html`:

```html
<linearGradient id="ironGrad" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0%" stop-color="#47494f" />
    <stop offset="45%" stop-color="#27282c" />
    <stop offset="100%" stop-color="#17181b" />
</linearGradient>

<linearGradient id="silverBevel" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0%" stop-color="#c9cdd6" />
    <stop offset="50%" stop-color="#7c828d" />
    <stop offset="100%" stop-color="#aeb4bf" />
</linearGradient>

<linearGradient id="spearBlade" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0%" stop-color="#e3e6ed" />
    <stop offset="48%" stop-color="#8b909c" />
    <stop offset="52%" stop-color="#3c3f47" />
    <stop offset="100%" stop-color="#686d78" />
</linearGradient>
```

The Daedric emblem crest is inline SVG in `view/index.html`:

```html
<path fill-rule="evenodd"
      d="M34,8 L60,34 L34,60 L8,34 Z M34,17 L51,34 L34,51 L17,34 Z"
      fill="url(#ironGrad)"
      stroke="url(#silverBevel)"
      stroke-width="1.1"/>
```

The crest has:

- Four spearhead/corner ornaments using `url(#spearBlade)`.
- Dark `#111` borders.
- Bright `#cfd5e0` etched rune strokes.
- An outer 78x78 Daedric frame.

The Daedric light diamond itself:

```html
<div id="daedric-light-gem"
     class="light-gem-core"
     role="img"
     aria-label="Light level unavailable"
     hidden></div>
```

Its exact CSS palette and bevel-like styling are in `view/minimal.css`:

```css
#minimal-layout.theme-daedric .light-gem-core {
    display: block;
    position: absolute;
    top: 50%;
    left: 50%;
    width: 23px;
    height: 23px;
    transform: translate(-50%, -50%) rotate(45deg);
    background: radial-gradient(
        circle,
        #f7e4a3 0%,
        #c89d3c 65%,
        #5c4314 100%
    );
    box-shadow: 0 0 12px rgba(240, 198, 92, 0.75);
    border: 1px solid #fff2c6;
    transition: all 0.2s ease;
}
```

The Daedric diamond is not a rivet-bearing widget in this markup. Rivet-like details are present as small rune/mark paths on the crest, caps, and sigils.

`renderLightGem()` in `view/minimal.js` dynamically changes:

- Core opacity: `0.25 + (lightLevel / 100) * 0.75`
- Core glow blur: `0px` to `16px`
- Core glow alpha: `0.2` to `0.95`

### Minimalist light diamond

The Minimalist widget is inline SVG in `view/index.html`:

```html
<svg viewBox="0 0 48 48" aria-hidden="true">
    <defs>
        <clipPath id="light-gem-clip">
            <path d="M24 7 38 24 24 41 10 24Z"/>
        </clipPath>
    </defs>
    <path class="gem-frame" d="M24 3 42 24 24 45 6 24Z"/>
    <path class="gem-core" d="M24 7 38 24 24 41 10 24Z"/>
    <rect id="light-gem-fill" class="gem-fill"
          x="10" y="41" width="28" height="0"
          clip-path="url(#light-gem-clip)"/>
    <path class="gem-edge" d="M24 7 38 24 24 41 10 24Z"/>
</svg>
```

Its CSS:

```css
#minimal-layout #light-gem {
    position: absolute;
    top: 11px;
    left: 11px;
    width: 44px;
    height: 44px;
    filter: drop-shadow(
        0 0 5px rgba(255,215,132,var(--gem-glow,0))
    );
}

#minimal-layout .gem-frame {
    fill: rgba(8,10,12,.75);
    stroke: rgba(200,205,210,.4);
    stroke-width: 1;
}

#minimal-layout .gem-core {
    fill: #343637;
}

#minimal-layout .gem-fill {
    fill: var(--gem-color, #6d5940);
    transition: fill .2s ease, y .2s ease, height .2s ease;
}

#minimal-layout .gem-edge {
    fill: none;
    stroke: rgba(220,220,210,.55);
    stroke-width: 1;
}
```

The fill color interpolates through these stops:

```text
0:   rgb(52, 54, 55)
25:  rgb(72, 65, 57)
60:  rgb(216, 155, 64)
100: rgb(255, 244, 197)
```

The fill height is `34 * lightLevel / 100`, positioned from the bottom by setting `y = 41 - height`.
