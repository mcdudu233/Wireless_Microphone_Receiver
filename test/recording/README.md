# Recording and TF storage QA

Run from `code/` with Python and a native MinGW compiler:

```powershell
python Receiver/test/recording/run.py --compiler-bin '<MinGW bin>' --output "$env:TEMP/receiver-recording-qa"
```

This compiles the production `tf.cpp` and `audio/buffer.cpp`, replacing only
the card filesystem, flash partition, scheduler and GPIO constants. The fake
filesystem checks exact normalized paths, tracks actual file bytes, enforces
read/write modes and injects missing media, short writes and corrupt readback.
Generated WAV fixtures and binaries stay in the supplied scratch directory.
Python's independent `wave` reader checks all 18 sample-rate/depth/channel
combinations; a separate RIFF parser checks sizes and LIST/INFO project,
author and software metadata. PCM payload bytes are checked exactly.

Assertions also cover:

- Five-digit sequence allocation, reboot counter reconstruction, existing
  files, pre-start audio exclusion, missing/incomplete frames and stop boundaries.
- Start/stop, USB ownership exclusion, remount on release, status polling
  during a slow remount, restored root directory access and missing-media release.
- Format changes, buffer resets/overruns, short writes, removal and RIFF limits.
- Crash image integrity, exact exported bytes, missing card, short/corrupt
  saved files, panic/task/exception/backtrace summary, and erasure only after
  successful readback.

The existing `test/main_dashboard` harness exercises the actual Record button
and all recording state/error dialogs against desktop LVGL. The existing
`test/usb_audio/ui_run.py` covers loading, Bluetooth, main, settings/subpages,
reader details, recording/missing-card selection rejection, file-manager
ownership notices, stale results, restoration, maximum lists, first/middle/last
file focus and exact directory-entry/parent paths. `test/usb_audio/run.py`
continues to verify TinyUSB and audio-buffer behavior.

Hardware validation remains required: real TF throughput and capacity limits
at 192 kHz/32-bit/stereo, sustained simultaneous RF/USB/playback/recording,
card insertion/removal, host writes followed by reader-mode exit, real panic
capture/export and matching-ELF decoding, 160x80 LCD/button focus/glyphs, and
the TF worker's stack high-water mark. The worker logs free stack at recording
completion and crash export. Host tests do not verify those hardware properties.
