# panes

Four panes, one quiet desktop. The original four-pane mark is available as
`assets/panes.svg`, `assets/app.ico`, and `assets/panes-icon-1024.png`.

A small native Windows 11 x64 app for a clock, date, month calendar, focus timer,
event countdown, battery indicator, world clock, and Music widget.
Built with C++20, Win32, Direct2D and DirectWrite. Works offline and does not
require Rainmeter, .NET, WebView, or a browser process.

## Install and use

[Download the Windows installer](https://github.com/Oraik-LLP/homescreen-widgy/raw/refs/heads/main/dist/panes-Setup-1.3.2-x64.exe),
[standalone EXE](https://github.com/Oraik-LLP/homescreen-widgy/raw/refs/heads/main/dist/panes.exe),
or [portable ZIP](https://github.com/Oraik-LLP/homescreen-widgy/raw/refs/heads/main/dist/panes-1.3.2-x64.zip).

Run `dist/panes-Setup-1.3.2-x64.exe`. Installation is per user and
does not require administrator rights. Launch at sign-in and a desktop
shortcut are optional and off by default. The ZIP can also run directly.

The first launch opens Settings. Select a widget to adjust its font, size,
opacity, colors and visibility. Clock format, seconds and theme
apply globally. Click **Apply** to save, or **Done** to save and close.
Changing the selected widget also applies the current settings.
Select **Add Widget…** for visual previews of every widget; click one to
enable it and select its settings. New widgets use sensible starting positions.
Settings has Widgets, Appearance, and Color tools pages, with a scrollable body. Closing
Settings with X or Escape discards unapplied edits and leaves widgets running.

Double-click the tray icon for Settings. Choose **Edit mode** in Settings or the
tray menu to drag widgets around your desktop. Choose **Finish editing** to save
the layout and fix their positions. The tray also lets you show/hide or exit.
Clock and information widgets pass clicks through outside Edit mode.

Choose **Add Widget… → Music** to show Windows Now Playing information from
supported players and browsers. Previous, play/pause, and next work outside Edit
mode too. In Edit mode, drag outside those controls to move the card. Artwork and
metadata stay local. Players decide what information and controls they expose.

Widgets sit above the wallpaper and behind normal applications. There is
no always-on-top mode. The Windows wallpaper is not changed. Minimal and Compact
retain the quiet clock styles. Editorial and Stacked add expressive typography;
Soft pairs warm cream and amber with subtle frosted panels inspired by Tagme.
Panes uses original layouts and privately bundles Inter, Manrope, DM Sans, Space
Grotesk, Plus Jakarta Sans, Outfit, and Google Sans Flex. Theme changes retain
positions, sizes, opacity, and widget visibility. No wallpaper or theme artwork
is included.

Settings live in `%LOCALAPPDATA%/panes/settings.json`. Positions
are stored in monitor-relative DIPs. If a monitor disappears, its widgets
move to the primary monitor. An unavailable font falls back to Segoe UI. Version 1 settings migrate
automatically with the original saved as settings.json.v1.bak.
Invalid or future-version settings are preserved and defaults run without
overwriting them. **Reset to defaults** makes a backup before recovering such a file.
Uninstall removes the app and its sign-in entry, and keeps your settings.

## New in 1.3.2

- Dragged widget positions survive Finish editing and subsequent Settings actions.
- Edit mode shows a monitor-aware grid and snaps to 24 DIP spacing. Hold Shift
  while dragging for free placement.
- Soft glass uses a cached frosted wallpaper backdrop with a contrast-aware tint.
- **Reset to defaults** restores the initial configuration and saves a backup
  named `settings.before-reset-*.json` beside your settings file.

## New in 1.3.1

- Use the Edit mode button in Settings or the tray menu to drag widgets, then
  Finish editing to fix their positions. Music controls remain interactive.

## New in 1.3.0

- Added the optional Music widget, using Windows global media sessions for track
  details, artwork, playback state, and controls. It starts hidden.
- Media work pauses when the widget is hidden or the Windows session is locked.

## New in 1.2.1

- Refined Soft with Tagme’s butter-yellow, cream, and amber palette, warm dark
  text, and a fine highlight around light translucent panels.
- Existing Soft-theme settings receive the updated theme colors automatically;
  manually assigned widget colors remain in place.

## New in 1.2

- **Add Widget…** opens a gallery with live previews of all seven widget types.
  Select a preview to enable and configure that widget. Existing widgets are
  marked in the gallery and can be selected from the same place.
- Appearance now includes Google Sans Flex, Space Grotesk, Plus Jakarta Sans,
  and Outfit, alongside Inter, Manrope, and DM Sans. Each family exposes light,
  regular, and semibold styles; all are loaded privately and have SIL OFL notices
  bundled with the app. Calendar labels follow the selected font too.

## New in 1.1

- Appearance includes five theme previews, nine bundled font styles, text/accent
  swatches, a native color chooser, and ten theme palette swatches.
- Color tools opens directly from the tray or with panes.exe --palette. Capture
  any screen color, copy HEX/RGB, keep 12 recent colors and eight favorites,
  and assign colors to a widget. Escape or right-click cancels the eyedropper.
  Recent colors/favorites save immediately; widget assignments require Apply/Done.
- Match wallpaper is optional for each theme. Panes reads the static wallpaper
  per monitor, extracts a muted accent and contrast-aware text locally on a
  background worker, and caches thumbnails of at most 128 pixels per side.
  Notifications and a 60-second identity check detect wallpaper changes without
  repeated decoding. Manual widget colors override matching; Use theme colors
  restores it. Busy backgrounds receive a subtle backing. Live wallpapers use
  Windows' static image if available; unreadable images retain preset colors.
- Four new widgets start hidden. Focus defaults to 25/5 minutes and has Start,
  Pause, Reset, and Next session in Settings and the tray. It remembers an
  absolute deadline across sleep/restarts and sends one silent completion notice.
  Countdown accepts a local YYYY-MM-DD HH:MM target, stores UTC, and shows
  Today/Reached at the boundary. Battery reports charging/AC/unavailable states.
  World clock uses Windows time zone keys and automatic daylight saving.
- Locked widgets remain click-through. Focus controls are in Settings/the tray
  so widgets can stay quiet and noninteractive on the desktop.

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
Weather, ongoing animation, analog clocks and Rainmeter-skin imports are outside this release.

## Reference research

- https://www.rainmeter.net/
- https://www.rainmeter.net/changes-4.5/
- https://github.com/rainmeter/rainmeter/issues/377
- https://github.com/rainmeter/rainmeter/pull/413
- https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-and-gdi-interoperation-overview
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setparent

Rainmeter was researched, not installed or run.
