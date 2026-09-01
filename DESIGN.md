# Receiver LVGL Design System

## 1. Atmosphere & Identity
The receiver UI is a compact equipment control surface: quiet, light, dense, and
easy to scan while holding the device. Its signature is a blue focus accent on
white menu surfaces, with shallow tonal separation instead of decorative graphics.

## 2. Color
| Role | Token | Value | Usage |
|------|-------|-------|-------|
| Surface primary | `surface-primary` | `#F4F5F7` | Window background |
| Surface elevated | `surface-elevated` | `#FFFFFF` | Menu rows and dialogs |
| Text primary | `text-primary` | `#1F2937` | Labels and titles |
| Text secondary | `text-secondary` | `#626367` | Status and metadata |
| Border | `border-default` | `#D1D5DB` | Controls |
| Accent | `accent-primary` | `#2D6BDB` | Focus, actions, selection |
| Accent pressed | `accent-pressed` | `#1D4ED8` | Pressed controls |
| Warning | `status-warning` | LVGL yellow palette | Destructive confirmation |
| Error | `status-error` | LVGL red palette | Operation failures |

Accent is reserved for interaction. New pages use the existing LVGL palette and
must not introduce gradients or extra decorative colors.

## 3. Typography
Primary font: embedded CJK Sans resources (`lv_font_harmonyos_12` and
`lv_font_harmonyos_14`). The 12px face is the minimum readable UI size and is
used for metadata and list entries; 14px is reserved for page titles. Both
resources include the complete current UI vocabulary and GBK-compatible card
filenames are supported by the FatFs code-page setting.

## 4. Spacing & Layout
The 160x80 display uses a 4px base rhythm. Settings pages are vertical LVGL menu
stacks with a fixed header, a compact content region, and scrolling owned by the
page or list. File names may be long, so labels use LVGL ellipsis/wrap behavior
rather than expanding the layout.

## 5. Components
### Settings menu row
- **Structure**: icon, label, optional value control.
- **States**: default, pressed, focused, disabled.
- **Accessibility**: every actionable row is in the default encoder group.
- **Motion**: existing 300ms ease-out entry animation on the root settings list.

### File browser row
- **Structure**: folder symbol or trash action symbol plus name and compact size metadata.
- **Variants**: directory, file, parent, refresh, empty, unavailable, truncated.
- **States**: default, pressed, focused, disabled.
- **Accessibility**: encoder focus is explicit; removable recording/log rows expose a trash action and deleting requires a two-button confirmation. Other card files, including configuration, are read-only from this UI.
- **Layout**: list owns vertical scrolling; the path label remains above it.

## 6. Motion & Interaction
File operations are immediate and visibly reflected by a list refresh. No
decorative motion is added. Existing focus and press feedback remain enabled.

## 7. Depth & Surface
Use the existing mixed strategy: white elevated rows/dialogs, pale gray page
surfaces, and restrained shadows on dialogs. Avoid nested decorative cards.

## 8. Accessibility Constraints & Accepted Debt
The encoder must reach every action, the destructive action must be explicit,
and empty/unavailable/error states must be readable at 12px. Hardware screen
contrast and actual card behavior require verification on the assembled receiver;
that hardware QA is accepted debt until a board and TF card are available.
