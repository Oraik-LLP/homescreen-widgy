# Validation

Test host: Windows 11 x64, build 26300, one 2560×1600 display at 150% scaling.
Release built with llvm-mingw 20261104 UCRT, C++20, `-O2`, static C++ runtime.
Rainmeter was not installed or executed.

## Completed

- Native release compiles with `-Wall -Wextra -Wpedantic` without warnings.
- 20 core checks: Gregorian dates, leap years/centuries, weekday alignment,
  midnight/noon/evening clock formats, timer boundaries, negative monitor
  coordinates, reachability, color validation, Unicode, configuration round
  trips, bounds, malformed/future-version preservation, and seconds sizing.
- 25 app integration checks against the real executable and native Settings
  controls. Includes placement styles, click-through flags, focus preservation,
  hide/show/lock, invalid settings, missing fonts, simulated session/power/time
  messages, missing-monitor recovery, both presets and seconds, and 80 repeated
  settings edits without GDI-handle growth.
- Duplicate-instance regression: a second test root exits with code 2 while
  the first is displaying widgets. One global presentation mutex covers normal,
  installed and diagnostic copies. This addresses the doubled digits in the
  user's screenshot, caused by concurrent test presenters at identical positions.
- 15 installer lifecycle checks: panes branding and icon assets, per-user install, installed-app integration
  checks, upgrade/binary verification, preserving settings, uninstall, default
  startup remaining unchanged, and uninstall cleanup of startup enabled later.
  The prior HKCU startup value is restored after testing.
- Actual widget surfaces exported and visually reviewed for Thin and Compact.
  The user's desktop screenshot also shows native Settings and widgets behind
  ordinary applications. Corrected Segoe UI Light/weight mapping in DirectWrite.
- Executable dependency inspection found only Windows DLLs; no compiler,
  .NET, browser, or third-party runtime DLL is required at runtime.
- An initial 305-second run (300 samples after warmup) measured 17.34 MiB peak
  private memory, 42.51 MiB peak working set and 0.00598% average total CPU.
  This run preceded the duplicate-presenter guard and included Settings.

The final single-presenter run lasted 305.609 seconds, with 300 samples after
warmup: **19.64 MiB peak private memory, 40.67 MiB peak working set, and
0.00182% average total CPU** (0.15625% peak). All three default widgets were
visible, locked, click-through and non-topmost, with no warning. This measurement
preceded the branding-only rename to panes; the widget runtime was unchanged.

Raw results live in `test-output/core`, `test-output/instance-primary`,
`test-output/installer`, `test-output/instance-*/primary`, and `test-output/release-idle`. These are build artifacts,
not repository source.

## Remaining interactive compatibility checks

These have implementation support but are not claimed as physically tested:

- Win+D and taskbar Show Desktop on different Windows 11 builds.
- Restarting Explorer while widgets are visible.
- Actual lock/unlock and sleep/resume (notification handling was simulated).
- Mixed-DPI multi-monitor dragging and physical monitor unplug/reconnect
  (missing-monitor fallback was exercised with an unavailable monitor ID).
- Virtual desktops, Remote Desktop, and alternate shells.
- MSVC/CMake build path; the delivered executable uses the tested LLVM path.

The desktop hierarchy is undocumented Explorer behavior. Future Windows
updates can require compatibility changes. The app does not inject into
Explorer or modify the wallpaper. The installer/executable are unsigned.
