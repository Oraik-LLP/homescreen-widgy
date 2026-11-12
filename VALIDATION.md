# Panes 1.2.1 validation — 2026-11-12

## Build and release checks

- Native 1.2.1 executable compiled with the updated Soft palette and renderer.
- Existing settings with the original Soft palette update on load; manually
  assigned widget colors are preserved.
- Installer and portable ZIP carry the updated release notes and metadata.
- The updated website build includes the Soft theme copy and release summary.
- Interactive appearance across varied wallpaper, display scaling, and
  wallpaper-match modes remains for hands-on review.

# Panes 1.2 validation — 2026-11-12

## Build and release checks

- Native release compiled from the updated sources with the LLVM-MinGW C++20
  toolchain and warning flags. The seven private font resources and 1.2.0
  version metadata are embedded in the executable.
- The static website build completed and includes the cleaned desktop screenshot,
  the 1.2.0 download links, and the release changelog entry.
- The 1.2 interactive widget gallery, mixed-DPI keyboard navigation, full
  installer upgrade/uninstall flow, and real multi-monitor use still need
  hands-on verification.

# Panes 1.1 validation — 2026-11-12

Windows 11 x64, one 2560×1600 display at 150% scaling; LLVM-MinGW 20261104.

## Completed for 1.1

- Native app compiles with C++20, -O2, -Wall -Wextra -Wpedantic without warnings.
- 52 core checks pass, including version 1 migration/backup preservation, all
  widget settings, timer deadline/pause/restart semantics, countdown boundaries,
  Windows daylight saving and relative dates, unknown battery states, bounded
  color history, and light/dark/busy/manual wallpaper contrast behavior.
- 37 real-app integration checks pass. This includes seven widget windows with
  configured visibility, click-through/non-topmost styles, original settings
  validation, missing-font fallback, simulated session/power messages, missing
  monitor recovery and 80 repeated edits without GDI handle growth.
- A headless renderer test exports 125 actual previews: all five themes and seven
  widget types at 96/144/192 DPI, plus 12/24-hour clocks with/without seconds.
  Long event/city labels are included. The contact sheet was visually reviewed.
- Three private bundled font families are verified. Light and semibold produce
  distinct rendered pixels for Inter, Manrope, and DM Sans.
- A short isolated run measured 23.31 MiB peak private bytes, 42.70 MiB peak
  working set and 0.000% sampled total CPU over 37 post-warmup samples. This
  is a short smoke measurement, not the required five-minute resource audit.
- The running normal app was relaunched from dist/panes.exe for user testing;
  its existing version 1 settings migrated to version 2 with a backup.

## Remaining 1.1 checks

- Full five-minute diagnostics for default widgets and all seven widgets, with
  wallpaper matching enabled. Targets: <0.5% average total CPU; <40 MiB private
  memory for default widgets and <60 MiB for all seven. Extraction transient
  memory and decode count are reported separately.
- Real wallpaper/slideshow changes and fit/fill/center/tile/span across multiple
  monitors. The core tests cover synthetic contrast and manual-color behavior;
  they do not substitute for these display checks.
- Hands-on native picker cancellation, screen eyedropper/multi-monitor sampling,
  clipboard copying, palette repeat launches, Settings layout/scrolling and
  keyboard navigation. Computer-use capture was not approved for the test binary;
  the user is testing the relaunched app.
- Re-run duplicate-instance and installer lifecycle scripts against the 1.1
  package with the normal instance closed. The settings-root mutex identity now
  normalizes equivalent absolute/relative paths.
- Physical sleep/lock, Explorer restart, mixed-DPI unplug/reconnect, virtual
  desktops, Remote Desktop, alternate shells and the MSVC build path.

## Commands

Close the normal app before presenter/installer tests; run sequentially:

```powershell
./build.ps1 -Package
./tests/run_diagnostics.ps1
./tests/run_diagnostics.ps1 -AllWidgets -MatchWallpaper
./tests/instance_tests.ps1
./tests/installer_tests.ps1
```

Core and RenderTests are headless and may run alongside the normal app.
The prior 1.0 validation record follows as historical evidence only.

---

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
