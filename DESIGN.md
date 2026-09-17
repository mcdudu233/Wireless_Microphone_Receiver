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
| Quality low | `quality-low` | `#0891B2` | Legacy 48 kHz icon (unused on dashboard) |
| Quality mid | `quality-mid` | `#2D6BDB` | Legacy 96 kHz icon (same value as `accent-primary`) |
| Quality high | `quality-high` | `#7C3AED` | USB JTAG / legacy 192 kHz icon |
| Status amber | `status-amber` | `#D97706` | USB SD-card mode status icon |
| Status error | `status-error` | LVGL palette red | Device-list disconnected state text (semantic error state, same rule as meter red) |

- Accent is functional, never decorative. One screen uses at most one accent
  family apart from semantic success/warning/error feedback.
- Add a color here before using it in UI source. LVGL palette colors are allowed
  only for semantic meter gradients, warnings, and errors.
- The main-screen status-bar icons reuse `text-tertiary` (`#9CA3AF`) for the
  USB-off state; every other status-icon color is one of the semantic tokens
  above (`quality-low`/`quality-mid`/`quality-high`, `accent-primary`,
  `status-amber`, `status-success`).

## 3. Typography

### Current embedded assets

| Role | Asset | Source family proven by generator metadata | LVGL line height | Usage |
|------|-------|--------------------------------------------|------------------|-------|
| Body | `lv_font_harmonyos_12` | HarmonyOS Sans SC Regular | 15 px | Rows, values, status, dialogs, dynamic text |
| Heading | `lv_font_harmonyos_14` | HarmonyOS Sans SC Medium | 17 px | Short page/dialog headings only |
| Display | `lv_font_harmonyos_16` | HarmonyOS Sans SC Medium | 19 px | Five-character boot title only |

All three sizes are generated from the approved HarmonyOS Sans SC sources in
`docs/resources/fonts/` (titles use Medium, body uses Regular) via the pipeline
in `docs/tools/README.md`. They share one 194-glyph Chinese symbol set maintained
in Receiver/src/res/font/symbols.txt, and every size falls back to
the matching built-in Montserrat font for `LV_SYMBOL_*` glyphs. Generated font
files must not be hand-edited.

- The three required sizes are 12, 14, and 16 px; do not add another size.
- Set the 12 px body font explicitly on each screen or shared component. Do not
  rely on a font inherited from a previously displayed page.
- Use 14 px only for short headings. Use 16 px only for the boot title.
- Keep all text on one line unless a dialog message explicitly permits two
  lines. Dynamic labels need a fixed width and `LV_LABEL_LONG_DOT` or wrapping.
- Dynamic non-ASCII device names and TF filenames are not guaranteed by the
  subset fonts; ASCII and the statically declared Chinese UI vocabulary are.
- When a new Chinese UI string is introduced, re-extract the symbol set and
  regenerate all three fonts (workflow in `docs/tools/README.md`).

### Iconography

- Icons are Tabler Icons (outline, 24x24 grid, 2 px stroke) rasterized to
  64x64 with the `#1F2937` (`text-primary`) stroke in RGB565A8, displayed at
  16 px via `lv_img_set_zoom(img, 64)` inside a 16x16 box.
- Tabler raster icons are for standalone 16x16 icon slots (menu rows, dialog
  icons, main-screen status bar). Inline status glyphs inside text rows use
  `LV_SYMBOL_*` text symbols via the Montserrat fallback so every glyph in one
  row shares the same visual style; device-card battery and signal indications
  are custom drawn widgets, not font glyphs.
- The boot microphone icon is 128x128 displayed at 32 px.
- Icon color stays neutral; focus and selection are expressed by row
  background/outline styles only (accent is functional, never decorative).
  Exception: main-screen USB/transport icons use semantic colors (see Main
  dashboard below). Sampling rate uses a numeric blue badge, replacing the
  historical colored volume icons (those resources remain for compatibility).
- Mapping from icon files to Tabler names and the regeneration pipeline live
  in `docs/tools/README.md`.

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
| `list-row-height` | 18 px | Dense file lists and device-page rows |
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
- **Settings header**: 19 px fixed height, leaving a 56 px content viewport in
  the 155x75 settings menu surface.

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
- **Text**: 12 px, one line, width bounded to the row. Overflowing dynamic
  filenames scroll continuously (marquee); static overflow uses end ellipsis.
- **Layout**: list owns vertical scrolling and focus-driven reveal. Selecting a
  row keeps the list scrollable and scrolls the selected row fully into view.
  The list viewport is an exact multiple of the row height and scrolling snaps
  to whole rows, so a row is never shown half-clipped.

### Device selection page
- **Structure**: screen-background page with a centered 14 px heading
  `麦克风设备连接` (2 px from the top edge) above one near-fullscreen rounded
  154x56 card (3 px margins, `border-subtle` outline, `surface-control` white,
  6 px radius, corner-clipped rows). The card viewport is 54 px = exactly three
  18 px rows; there are no side buttons. The LAST row is the `完成` action row
  (success-green surface, white centered 14 px label, blue focus outline),
  pinned to the card bottom — the list pads its top whenever fewer than three
  rows exist. Device rows are appended above it as they are discovered (at
  most five rows total, scrolling beyond three); focus starts on `完成` and
  the group order wraps bottom→top. An empty list shows a quiet `暂无设备`
  hint centered on the card.
- **Device rows**: left `设备N` label in `text-primary`, right-aligned status
  text in parentheses; the device number N is the persistent per-MAC numbering
  stored in NVS (same MAC keeps the same number across reboots). Status text
  color encodes state: `(已连接)` in `status-success`, `(连接中)` in
  `accent-primary`, `(未连接)` in `status-error`. Connected rows additionally
  use the `status-success-bg` surface; all other rows are white.
- **Behavior**: discovery auto-connects unlinked transmitters (the row runs
  through `连接中` to `已连接` without user input); a transmitter the user
  explicitly disconnects from its info dialog is not auto-reconnected until the
  page is re-entered. A single encoder confirm on a device row opens the device
  info dialog — confirm never toggles the link directly. After boot, the
  loading card switches to `正在连接设备...` while the receiver scans and
  auto-connects: if any transmitter links within 1 s the main screen is entered
  directly (a WiFi configuration runs the finish migration first, briefly
  showing this page with a connecting dialog); only a 1 s timeout with no link
  falls back to this page for manual pairing, and scanning with auto-connect
  continues on the page.
- **Layout**: same scrolling, snapping, and focus-driven reveal rules as the
  dense list; the scrollbar appears only when rows exceed the viewport.

### File browser
- **Structure**: full-bleed 18 px rows directly under the menu header; no
  in-page path label.
- **Title**: the menu header title displays the live TF path (`TF:<path>`) and
  updates on every directory change; leaving the page restores the normal page
  title automatically.
- **Rows**: back-to-parent row first (omitted at the TF root), then entries;
  re-entering a directory reloads its contents; long names use end ellipsis;
  destructive delete keeps the two-button confirmation.
- **States**: same as dense list, plus a truncated-list notice row.

### Info row
- **Structure**: one-line focusable row with a secondary-color name on the left
  and a right-aligned primary-color dynamic value on the right.
- **Usage**: system information pages; rows stay focusable so the encoder can
  scroll the page, with 2 px bottom separation.

### USB mode detail block
- **Structure**: one compact mode dropdown followed by a fixed two-line white
  detail surface; the first line contains the mode's primary capability or TF
  capacity/type, and the second line contains availability or access behavior.
- **Layout**: the complete selector and detail block must fit in the USB page's
  fixed viewport. The page is not scrollable and never shows a scrollbar.
- **Behavior**: details preview the highlighted dropdown option immediately.
  Missing TF media is shown as an unavailable state; mass-storage mode states
  that the host has exclusive access while it is active.

### Audio and screen settings
- Microphone capture and local 3.5 mm playback are separate root pages. The
  output page can either follow the 3.5 mm jack-detect signal or keep the DAC
  enabled continuously; playback format always follows the microphone sample
  rate, bit depth, and channels.
- Gain mode drives the gain slider state: in auto-gain mode the slider is
  disabled with the same muted treatment as the BLE-locked dropdowns and
  read-only displays the transmitter's live AGC gain; in peak-reduction mode
  the slider value is the initial gain commanded to the transmitter and then
  follows the transmitter's gain reductions (never increases) at about 1 Hz;
  manual mode edits a fixed gain. Peak-reduction reductions are written back
  to the stored gain so the next commanded initial gain matches reality.
- While the transport protocol is BLE, the microphone format is fixed at
  48 kHz / 16 bit / mono: the sample-rate, bit-depth, and channel dropdowns are
  shown in a muted disabled state (60% text opacity, `border-subtle` outline)
  with a centered one-line quiet hint `BLE模式已固定格式` above them; gain mode
  and gain stay editable. Switching the protocol to BLE resets a stored wider
  format to the BLE-supported one, and switching back to WiFi re-enables the
  dropdowns.
- Screen brightness uses a 10-100% slider so an accidental edit cannot make the
  settings UI unreadable. Auto-off offers never, 30 seconds, 1 minute, and 5
  minutes; changing either value applies immediately.
- Sliders use a 64x6 px track with a 12 px knob, followed by a fixed-width,
  right-aligned value label. Values never overlay the track or knob. The gray
  track, blue filled portion, white knob, and blue focus outline use existing
  design tokens.

### Main dashboard (2026-09 refresh)
- Fixed header at y=2..20: a 40x18 settings button at x=4, then a numeric
  sample-rate badge at x=84 (32x18), USB at x=120 and transport at x=140
  (both 16x16). Rate badges read `48k`, `96k`, `192k`: sampling rate in kHz,
  not loudness. All use accent-primary on accent-surface, differentiated by
  the number rather than color alone; they replace the old volume icons.
- White 152x54 information surface at (4,24), without shadow. A 20 px tab
  strip uses fixed 32 px tabs with persistent device numbers; focused tabs have an inset blue
  outline and the selected tab has a pale blue surface. Tab labels inherit
  state color from their button. Focus order is devices, then settings.
- The 152x34 card has two side-by-side L/R meters (52x6), followed by one
  15 px status line: battery outline and percentage, four signal bars,
  and explicit `丢包N%` text. Text remains within the card even at 100%.
  Healthy indicators use success-green; low levels use amber/red thresholds,
  avoiding the former rainbow gradient. Meter tracks are muted gray.
- USB uses gray USB / blue headphones / amber SD card / violet CPU for
  off / audio / storage / JTAG. WiFi is green, Bluetooth blue. Image slots
  use LVGL CONTAIN alignment to scale source pixels into the actual box.
- Reconnect overlay covers the entire 152x34 content area at (0,20), leaves
  tabs/settings reachable, and uses a bounded 144 px one-line message.
- Empty state occupies only the content area. Header states refresh even
  with no device cards. No new page or global font role is introduced.

### Reconnect status chip
- Non-modal warning-surface panel covering the dashboard content (152x34),
  below the tab strip. It never joins an encoder group or blocks settings.
- One device shows `设备N重连中...`; multiple devices show `重连中... xN`.
  The bounded 144 px label uses the body font and end ellipsis.
- It disappears when all waiting devices reconnect or are removed. Removing
  a card also removes its tab button and keeps the active index in range.

### Connection-loss dialog and auto-return
- **Trigger**: when a device stays offline past the reconnect grace window
  (`RF_RECONNECT_TIMEOUT_MS`), its card is removed and a message dialog
  reports `设备N连接失败`; when it was the last card, the second line states
  `已返回选择设备` and the UI returns to the device selection screen.
- **Dialog**: standard message dialog without action buttons; it closes on
  encoder press or automatically after ~2.5 s.
- **Auto-return while in settings**: if the settings overlay is in front when
  the last card is removed, the return to the device screen is deferred until
  the user leaves settings, then the same dialog is shown.
- **Device selection list after auto-return**: the scan re-adds a missing row
  for an already-known transmitter without touching the selection/focus of
  existing rows; in WiFi mode the receiver closes WiFi and reopens BLE first
  so pairing can resume.

### Message dialog
- **Structure**: heading, divider, one- or two-line body, optional action row.
- **Size**: 120x60 without actions; 120x75 with actions.
- **Text**: 12 px; body width 108 px and maximum two lines.
- **States**: first action focused on entry; both physical navigation buttons
  can reach each action before confirmation.

### Device info dialog
- **Structure**: 144x75 dialog opened by a single confirm on a device row:
  icon + `设备信息` title, divider, device name with right-aligned colored
  status (same states and colors as the device row), and one `MAC:<address>`
  line in `text-secondary` (bounded width, end ellipsis).
- **Actions**: bottom row with one primary action — `断开连接` (56x18) when
  connected, otherwise `连接` (34x18) — plus a secondary white `关闭` button
  (34x18, `border-default` outline) on the right; the primary action is focused
  on entry. Actions close the dialog before running; link state changes are
  then reflected on the device row (`连接中` → `已连接`/`(未连接)`).

## 6. Motion & Interaction

- Physical previous and next buttons move focus backward and forward. Pressing
  both together confirms the focused item. Do not add a competing input model.
- On the device selection page, discovered transmitters auto-connect without
  user input, and a single confirm on a device row opens its info dialog
  instead of toggling the link. The main-screen tabs, reconnect chip, and
  link-lost dialogs use the same persistent `设备N` numbering as the device
  rows. After boot the receiver auto-connects while the loading card shows
  `正在连接设备...`; it enters the main screen directly once any transmitter
  links (WiFi configuration runs the BLE→WiFi finish migration first) and only
  falls back to the device selection page after a 1 s timeout with no link,
  continuing to scan and auto-connect there.
- Settings dropdowns and sliders save and apply their selected value as soon as
  it changes. Returning from a settings subpage never asks whether to apply it.
- When auto-off has blanked the display, the first physical-button action wakes
  it and is consumed; navigation resumes with the next action. Auto-off fades
  the backlight out over about 0.5 s, waking fades it back in over about 0.5 s,
  and a button press during the fade-out cancels it (the press acts as normal
  input) and restores the working brightness.
- A short Previous/Next press changes a focused slider by one step. Holding the
  button starts rapid one-step repeats after a brief delay; repeat is enabled
  only when the press began on a slider, so holding a menu navigation action
  cannot accidentally edit a slider reached later.
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
- Font debt resolved: all three sizes now come from approved HarmonyOS Sans SC
  sources with unified glyph coverage and Montserrat symbol fallback. The new
  12 px/14 px line heights (15/17 px) replaced the SimHei metrics (13/16 px);
  on-device vertical fit of the tightest rows and dialogs still needs
  confirmation on an assembled receiver.
