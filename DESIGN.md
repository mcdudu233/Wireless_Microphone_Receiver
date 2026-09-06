# Receiver 160x80 LVGL Design System

This document is the visual contract for every Receiver screen. Read it before
changing `src/ui/`, `include/ui/`, or generated UI resources.

## 1. Atmosphere & Identity

The receiver is a compact professional-audio control surface: quiet, precise,
and readable at a glance. The signature is a pale neutral background, white
functional surfaces, and one blue focus accent. Information density is high,
but each screen must have one obvious focus target and no decorative clutter.

## 2. Color

| Role | Token | Value | Usage |
|------|-------|-------|-------|
| Screen | `surface-screen` | `#F4F5F7` | Full-screen background |
| Surface | `surface-control` | `#FFFFFF` | Lists, cards, controls, dialogs |
| Text | `text-primary` | `#1F2937` | Titles, values, primary labels |
| Muted text | `text-secondary` | `#626367` | Metadata and secondary status |
| Body muted | `text-body-muted` | `#6B7280` | Dialog copy and compact status |
| Quiet text | `text-tertiary` | `#9CA3AF` | Loading and low-priority hints |
| Menu screen | `surface-menu` | `#F5F7FA` | LVGL menu root background |
| Divider | `border-subtle` | `#E5E7EB` | Tracks and restrained separators |
| Border | `border-default` | `#D1D5DB` | Input outlines |
| Focus | `accent-primary` | `#2D6BDB` | Focus, selected state, progress |
| Focus surface | `accent-surface` | `#DBEAFE` | Focused rows and compact inputs |
| Pressed | `accent-pressed` | `#1D4ED8` | Pressed primary controls |
| Success | `status-success` | `#059669` | Connected and completed states |
| Warning surface | `status-warning-bg` | `#FFFBEB` | Warning selection state |
| Success surface | `status-success-bg` | `#ECFDF5` | Connected selection state |

- Accent is functional, never decorative. One screen uses at most one accent
  family apart from semantic success/warning/error feedback.
- Add a color here before using it in UI source. LVGL palette colors are allowed
  only for semantic meter gradients, warnings, and errors.

## 3. Typography

### Current embedded assets

| Role | Asset | Source family proven by generator metadata | LVGL line height | Usage |
|------|-------|--------------------------------------------|------------------|-------|
| Body | `lv_font_harmonyos_12` | SimHei (`simhei.ttf`) | 13 px | Rows, values, status, dialogs, dynamic text |
| Heading | `lv_font_harmonyos_14` | SimHei (`simhei.ttf`) | 16 px | Short page/dialog headings only |
| Display | `lv_font_harmonyos_16` | HarmonyOS Sans SC Regular | 19 px | Five-character boot title only |

The 12 px and 14 px symbols are historically misnamed: they are not HarmonyOS
fonts. Do not describe the current font set as uniformly HarmonyOS. A future
font regeneration may replace them only from an approved HarmonyOS Sans SC
source file while preserving every current Chinese glyph and LVGL symbol
fallback. Generated font files must not be hand-edited.

- The three required sizes are 12, 14, and 16 px; do not add another size.
- Set the 12 px body font explicitly on each screen or shared component. Do not
  rely on a font inherited from a previously displayed page.
- Use 14 px only for short headings. Use 16 px only for the boot title because
  its embedded Chinese subset is narrower than the 12/14 px subsets.
- Keep all text on one line unless a dialog message explicitly permits two
  lines. Dynamic labels need a fixed width and `LV_LABEL_LONG_DOT` or wrapping.
- Dynamic non-ASCII device names and TF filenames are not guaranteed by the
  subset fonts; ASCII and the statically declared Chinese UI vocabulary are.
- The current 12 px subset still needs `暂中名长屏幕按钮解码` for all present
  on-screen states. Until the owner regenerates that resource, those glyphs may
  render as placeholders. The current 14 px and 16 px visible strings are covered.

## 4. Spacing & Layout

The viewport is fixed at 160x80 logical pixels. There are no breakpoints.
Spacing uses a 2 px micro-grid, with 4 px as the normal gap.

| Token | Value | Usage |
|-------|-------|-------|
| `space-1` | 2 px | Optical correction, compact inner gap |
| `space-2` | 4 px | Standard icon/text and row gap |
| `space-3` | 6 px | Control padding or grouped separation |
| `space-4` | 8 px | Maximum ordinary inset |
| `screen-edge` | 5 px | Main content edge where a full-bleed menu is not used |
| `row-height` | 22-24 px | Settings and action rows |
| `list-row-height` | 18 px | Dense Bluetooth/file lists |
| `action-height` | 24-25 px | Primary screen actions |

- Budget geometry from the parent content box, including border and padding.
  Percentage-sized children are forbidden where an LVGL default content area
  makes the resulting pixel size ambiguous.
- Align related controls to each other, not independently to the screen. Small
  1-2 px optical corrections are allowed and must preserve the 2 px grid.
- A fixed header and a scrollable list must have separate rectangles. Only the
  list scrolls; headers, paths, and primary actions remain stationary.
- Horizontal overflow is forbidden. A four-character 12 px label plus an 85 px
  dropdown is the maximum row composition.

## 5. Components

### Screen shell
- **Structure**: one 160x80 root with zero padding and zero border.
- **Typography**: explicit 12 px body font.
- **Surface**: `surface-screen`; no nested full-screen borders.

### Focusable row
- **Structure**: optional 16x16 icon, one-line label, optional value control.
- **Spacing**: 5 px inner padding, 2 px bottom separation.
- **States**: white default, gray pressed, blue visible focus, muted disabled.
- **Layout**: fixed compact height; label cannot push a value control off-screen.

### Action button
- **Structure**: centered one-line 12 or 14 px label.
- **Size**: at least 30x18 in a dialog; 42-48x24-25 for screen actions.
- **States**: blue default, darker blue pressed, visible encoder focus.

### Dense list
- **Structure**: stationary header plus 18 px rows in the list viewport.
- **States**: default, focused, selected/connected, disabled, empty, error.
- **Text**: 12 px, one line, width bounded to the row with end ellipsis.
- **Layout**: list owns vertical scrolling and focus-driven reveal.

### Device meter card
- **Structure**: 20 px tab bar, two labeled 10 px meter rows, one compact status row.
- **Layout**: every pixel dimension is explicit; status values use short symbol
  plus number forms so left and right values cannot collide.

### Message dialog
- **Structure**: heading, divider, one- or two-line body, optional action row.
- **Size**: 120x60 without actions; 120x75 with actions.
- **Text**: 12 px; body width 108 px and maximum two lines.
- **States**: first action focused on entry; both physical navigation buttons
  can reach each action before confirmation.

## 6. Motion & Interaction

- Physical previous and next buttons move focus backward and forward. Pressing
  both together confirms the focused item. Do not add a competing input model.
- Focus must always remain visible when a list scrolls. Returning from a subpage
  restores focus to the row that opened it.
- Root settings rows may use the existing 300 ms horizontal entry transition.
  Scrolling must not translate controls sideways or fade them; on an 80 px-high
  screen that motion looks like misalignment and harms readability.
- Meter animation communicates live level only. No decorative animation.

## 7. Depth & Surface

Use tonal separation first: pale screen, white controls, subtle gray dividers.
Shadows are limited to modal dialogs and one shallow main information surface.
Menus and list rows must not stack card shadows; compact alignment carries the
hierarchy.

## 8. Accessibility Constraints & Accepted Debt

- Text may not be smaller than 12 px. Focus is indicated by more than a subtle
  color change and remains visible for every encoder-reachable control.
- Dynamic text is tested with its longest current Chinese string and maximum
  numeric value. No label may cover another control or leave the viewport.
- Destructive actions require an explicit two-button confirmation dialog.
- **Accepted hardware debt**: exact LCD contrast, physical button timing, glyph
  raster quality, and screenshot-level alignment require an assembled receiver.
- **Accepted font debt**: 12/14 px are SimHei-derived despite their symbol names.
  Exit when approved HarmonyOS Sans SC source is available and regenerated
  assets preserve current glyph coverage, add the missing characters listed in
  Section 3, and fit within the flash budget.
