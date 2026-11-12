# panes

Four panes, one quiet desktop. The original four-pane mark is available as
`assets/panes.svg`, `assets/app.ico`, and `assets/panes-icon-1024.png`.

A small native Windows 11 x64 app for a clock, date, and month calendar.
Built with C++20, Win32, Direct2D and DirectWrite. Works offline and does not
require Rainmeter, .NET, WebView, or a browser process.

## Install and use

[Download the Windows installer](https://github.com/Oraik-LLP/homescreen-widgy/raw/refs/heads/main/dist/panes-Setup-1.0.0-x64.exe),
[standalone EXE](https://github.com/Oraik-LLP/homescreen-widgy/raw/refs/heads/main/dist/panes.exe),
or [portable ZIP](https://github.com/Oraik-LLP/homescreen-widgy/raw/refs/heads/main/dist/panes-1.0.0-x64.zip).

Run `dist/panes-Setup-1.0.0-x64.exe`. Installation is per user and
does not require administrator rights. Launch at sign-in and a desktop
shortcut are optional and off by default. The ZIP can also run directly.

The first launch opens Settings. Select a widget to adjust its font, size,
opacity, colors and visibility. Clock format, seconds and layout preset
apply globally. Click **Apply** to save, or **Done** to save and close.
Changing the selected widget also applies the current settings. Closing
Settings with X or Escape discards unapplied edits and leaves widgets running.

Double-click the tray icon for Settings. Right-click it to unlock,
show/hide, or exit the widgets. Unlock to drag widgets. Lock again to make
them click-through so desktop icons and selection work normally.

Widgets sit above the wallpaper and behind normal applications. There is
no always-on-top mode. The Windows wallpaper is not changed. Thin and
Compact are original presets inspired by minimalist desktop clocks; no
reference wallpaper, skin artwork, or font files are included.

Settings live in `%LOCALAPPDATA%/panes/settings.json`. Positions
are stored in monitor-relative DIPs. If a monitor disappears, its widgets
move to the primary monitor. An unavailable font falls back to Segoe UI.
Invalid or future-version settings are preserved and defaults run without
overwriting them. **Reset layout** makes a backup before recovering such a file.
Uninstall removes the app and its sign-in entry, and keeps your settings.

## Build

Portable compiler: run `./tools/bootstrap.ps1 -Inno`, then
`./build.ps1 -Package`. The tools stay in this project; the app has no
dependency on them. Alternatively install CMake, MSVC C++ Build Tools, and
the Windows SDK, then run `./build.ps1`. Pass `-InnoCompiler <ISCC.exe>`
when packaging with a separately installed Inno Setup.

The icon is already included. To regenerate it, run `assets/make_icon.py`
with Python and Pillow. The portable executable statically links the C++
runtime and uses Windows-supplied DLLs. Third-party notices and runtime
licenses accompany the distribution.

## Diagnostics

Run a measured idle test from PowerShell (waits for completion):

```powershell
./tests/run_diagnostics.ps1
./tests/instance_tests.ps1
./tests/installer_tests.ps1
```

Run these scripts sequentially, with the normal app closed. Installer tests
temporarily exercise the app's HKCU startup value and restore any previous
value afterward. They install only into the project's test-output folder.

The executable also supports `--prototype`, `--application-test`, `--settings`,
`--diagnostic`, `--duration=305`, and `--export-previews`, with a separate
`--test-root <folder>`. The prototype shows only the clock using the same
placement implementation.
Application tests exercise the real app, native Settings controls, display
fallback and session/power message handling, including 80 repeated edits.
Session and power messages are simulated; the tests do not lock or suspend
the computer or restart Explorer.
Diagnostics require an explicit test root, isolate settings and single-instance
state, and disable startup registration.
Only one visible widget set may run at a time, even across different test
roots. A second presenter exits with code 2 instead of overlapping clocks.
Run the diagnostic commands sequentially.

Preview BMPs contain the actual rendered surfaces. Diagnostics record peak
private bytes, peak working set,
CPU normalized across logical processors, window styles and placement.
The first five seconds are excluded from resource samples. CPU and memory
targets are under 0.5% average total CPU and 40 MiB private memory for the
three default widgets; they are engineering targets, not a zero-RAM claim.

`--quit` stops the normal instance. `--settings` opens or activates its settings.
Test instances exit automatically after their diagnostic duration.

## Desktop compatibility

The app discovers the Explorer window containing `SHELLDLL_DefView` and uses
its top-level ancestor for z-order placement. It does not assume one fixed
Progman/WorkerW hierarchy or use cross-process SetParent. Foreground and
minimize events repair stacking without activating widgets. The tray returns
after Explorer restarts. Missing-shell retries are bounded; widgets hide until
the desktop can be found. Session lock and suspension stop widget timers.

Explorer's desktop hierarchy is an implementation detail that Windows updates
may change. See `VALIDATION.md` for actual validation and remaining manual checks.
Weather, system meters, animation, analog clocks and Rainmeter-skin imports
are outside this release.

## Reference research

- https://www.rainmeter.net/
- https://www.rainmeter.net/changes-4.5/
- https://github.com/rainmeter/rainmeter/issues/377
- https://github.com/rainmeter/rainmeter/pull/413
- https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-and-gdi-interoperation-overview
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setparent

Rainmeter was researched, not installed or run.
