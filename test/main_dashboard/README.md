# Dashboard host QA

This harness compiles the actual `src/ui/ui_main.cpp` and shared `ui.cpp`
against the managed LVGL library, embedded fonts and icon assets. Only hardware
locks, telemetry, and transitions to Bluetooth/settings are replaced by fakes.
The stubbed settings transition verifies the button event, not settings behavior.

Run from `code/` with Python, CMake and MinGW on PATH (or supply its bin path):

```powershell
python Receiver/test/main_dashboard/run.py --compiler-bin '<MinGW bin>' --output "$env:TEMP/receiver-dashboard-qa"
```

Build products and PPM screenshots stay in the chosen scratch directory, outside
firmware sources. The generated scratch CMake project does not modify the
PlatformIO-managed ESP-IDF bridge.

Assertions and screenshots cover:

- Three sample rates, four USB modes, both transports (24 combinations).
- 0/1/4 devices, first/middle/last tab focus, settings entry and repeat entry.
- Exact telemetry labels, full/empty battery, weak/unknown RSSI, 100% loss.
- Geometry bounds and text widths at maximum values and device number 255.
- One/multiple reconnect overlays, longest link-loss dialog and group restoration.
- Removal of middle/active devices: tab buttons, group count, selected device.

2026-09-17: host assertions passed; rendered mode grid, focus states, empty state,
reconnect, telemetry extremes and link-loss dialog inspected. Font generator
verified all 194 symbols in all three sizes. Existing loading, Bluetooth and
settings code/font assignments were inspected; full visual regression of those
pages and dropdowns was not performed by this focused harness.

Hardware validation remains unperformed: physical 160x80 LCD contrast/rendering,
Previous/Next/both-button timing, live RF telemetry, and screen-task stack
high-water mark. Desktop snapshots are not evidence of those hardware properties.

PlatformIO debug and release builds passed; release merged firmware generated.
