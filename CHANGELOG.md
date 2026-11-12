# Changelog

## 1.3.2 — 2026-11-12

- Fixed dragged positions reverting when another Settings action reapplied an
  older draft. Placement now follows the current desktop layout.
- Added a visible per-monitor alignment grid and 24 DIP snapping in Edit mode.
  Hold Shift while dragging to move freely.
- Soft glass now uses cached, blurred wallpaper tiles and a light or dark tint
  that keeps the selected text readable. Wallpaper work remains local.
- Added Reset to defaults, which backs up settings before restoring the initial
  layout, appearance, and widget visibility.

## 1.3.1 — 2026-11-12

- Added a visible Edit mode button in Settings and a matching tray command.
  Drag widgets while editing, then choose Finish editing to fix the layout.
- Music playback controls now work while the layout is fixed. Other widgets
  continue passing clicks through outside Edit mode.

## 1.3.0 — 2026-11-12

- Added an optional Music widget to the Add Widget gallery, showing the current
  Windows media session's app, track, artist, artwork, and playback state.
- Added previous, play/pause, and next controls while widgets are unlocked, with
  media-key fallback when session control is unavailable.
- Media work runs in the background only while the widget is visible. Artwork
  is limited to 128 pixels and reused until track metadata changes.
- Updated the website with both supplied wallpaper screenshots and a Windows
  inspired light appearance using Segoe UI.

## 1.2.1 — 2026-11-12

- Refined Soft with Tagme’s butter-yellow, cream, and amber palette, minimal
  frosted panels, dark warm text, and thin highlights.
- Updated existing Soft-theme color values while preserving manual widget colors.
- Updated the website theme description and release changelog.

## 1.2.0 — 2026-11-12

- Added **Add Widget…**, a preview gallery for all seven widget types. Selecting
  a card enables that widget, selects it in Settings, and starts it in a default
  position. Existing widgets are marked and can be selected from the gallery.
- Bundled Google Sans Flex, Space Grotesk, Plus Jakarta Sans, and Outfit in
  addition to Inter, Manrope, and DM Sans. All seven font families expose light,
  regular, and semibold styles, load privately through DirectWrite, and include
  their upstream SIL Open Font License notices. Calendar text uses the selected
  font family as well.
- Published the cleaned desktop screenshot on the website, with the left-side
  desktop icons and shortcuts removed.

## 1.1.0 — 2026-11-12

- Added Minimal, Compact, Editorial, Stacked, and Soft static themes with previews.
- Added HEX fields, text/accent swatches, native color pickers, screen eyedropper,
  clipboard copying, recent colors, favorites, and explicit widget color assignment.
- Added optional per-theme wallpaper matching with per-monitor sampling, a local
  thumbnail cache, contrast-aware text, and manual-color overrides.
- Added hidden-by-default focus timer, event countdown, battery, and world clock
  widgets. Notes remain in Tagme.
- Migrated version 1 settings to version 2 while retaining positions and backing
  up the original settings file before the first migrated save.

## 1.0.0 — 2026-11-12

- Released the native Windows desktop clock, date, and calendar widgets, with
  per-user installer and portable ZIP downloads.
