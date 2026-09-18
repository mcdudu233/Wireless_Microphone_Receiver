# Preferences / TF configuration and logging QA

Run from `code/`:

```powershell
python Receiver/test/storage_sync/run.py --compiler-bin '<MinGW bin>' --output "$env:TEMP/receiver-storage-sync-qa"
```

This compiles production `config.cpp`, `logger.cpp`, `tf.cpp` and audio buffer
code. Only Preferences, media, flash, scheduler and runtime hardware hooks are
replaced. Assertions check actual Preferences bytes and exact filesystem paths.
Coverage includes UI export, USB ownership and edits, reboot/offline changes,
card removal/reinsertion, strict complete-file parsing, BOM/CRLF/comments,
invalid ranges/duplicate/missing keys, BLE/USB bandwidth conflicts, stale save
generations, NVS failure, short/corrupt config writes and backup recovery.
Logging checks serial forwarding, buffered records, sequence reconstruction,
USB pause/resume, partial writes, rotation, ring wrap and overflow markers.

Editable settings live at `/config/device.ini`. Change values in the audio,
rf, usb, audio_output and screen sections; retain `[sync]`, `schema=1` and
`sync_base`. `sync_base` is the last successful settings fingerprint, rather
than a timestamp. A changed settings fingerprint makes TF authoritative at
boot, remount or the next media check. An unchanged TF file makes Preferences
authoritative, including settings saved while the card was absent or owned by
USB. Successful import rewrites the baseline. Comment-only edits do not import.
The exact older placeholder file is upgraded automatically. Invalid/unreadable
files are retained for correction and are never partially imported. Do not
change files until the host has finished writing and safely ejected the reader.

| Section / key | Allowed values |
| --- | --- |
| audio / channel | 1 mono, 2 stereo |
| audio / rate | 48000, 96000, 192000 Hz |
| audio / bit | 16, 24, 32 |
| audio / mode | 0 automatic, 1 peak reduction, 2 manual |
| audio / gain | -64..63 dB |
| rf / mode | 1 BLE, 2 WiFi; BLE requires 48000/16/mono |
| usb / mode | 0 off, 1 debug, 2 audio, 3 card reader; audio must fit USB bandwidth |
| audio_output / enabled | 0 off, 1 on |
| audio_output / mode | 0 jack detection, 1 always on |
| screen / brightness | 10..100 percent |
| screen / timeout | 0 never, 30, 60, 300 seconds |

Preferences saves are exported asynchronously. Config writes use a readback-
verified temporary file and retain a backup across the FAT rename gap. On boot
the backup is restored when the primary file is absent. The binary Preferences
layout and firmware/config version are unchanged.

Serial output remains enabled. ESP-IDF logs at the active build log level are
buffered in 16 KiB of PSRAM and flushed by the TF task. Release records warning
and error levels; debug additionally records info. Messages over 383 bytes are
truncated with a marker; a full buffer drops new records and records a loss
count when media becomes available. USB ownership and absent cards pause media
writes. Files are `/logs/LOG00001.txt`, increasing without overwriting existing
files, with a new file after remount and rotation at 1 MiB. ESP-IDF uptime fields
provide ordering within a boot; there is no wall-clock date. A panic's last
buffered messages are not guaranteed to flush; the Flash core dump is exported
independently on the next boot.

Hardware checks remain unverified: host cache/eject/remount behavior, physical
card replacement, real NVS/media power loss, sustained high-rate RF/audio with
TF logging, real RF migration success/fallback, LCD/button control refresh and
the TF task stack watermark. Native runtime-hook tests do not prove hardware
application. Run both PlatformIO environments, existing recording/TinyUSB tests
and real LVGL UI checks before committing.
