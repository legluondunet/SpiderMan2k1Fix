# Project documentation

This directory contains the maintained technical references for **SpiderMan2k1Fix**. Historical per-feature test reports and diagnostics are archived under [ChatGPT/](../ChatGPT/README.md). Documents describing historical experiments can contain superseded instructions; consult the current [README](../README.md), source code and configuration for the active behavior.

## Main references

- [Development and original EXE reverse engineering](DEVELOPMENT.md)
- [F10 validation matrix](F10_VALIDATION_MATRIX.md)
- [F10/F12 debug research](F12_DEBUG_TEST_MODE.md)
- [Native shutdown research](F12_NATIVE_SHUTDOWN.md)
- [Native ESC pause menu and 4:3 rendering](NATIVE_PAUSE_MENU_43.md)

## Archived diagnostics (ChatGPT workspace)

### DirectInput

- [Static DirectInput analysis](../ChatGPT/diagnostics/directinput/DIRECTINPUT_STATIC_ANALYSIS.md)
- [External DirectInput diagnosis](../ChatGPT/diagnostics/directinput/DIRECTINPUT_EXTERNAL_DIAG.md)
- [INI-only DirectInput recovery validation](../ChatGPT/diagnostics/directinput/DIRECTINPUT_INI_FINAL_TEST.md)

### Display and graphics

- [Windows viewport and shutdown diagnosis](../ChatGPT/diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md)
- [Windows primary-surface reuse tests](../ChatGPT/diagnostics/graphics/WINDOWS_PRIMARY_REUSE_TEST.md)

### Window focus and audio/video pause

- [Cursor visibility on focus loss](../ChatGPT/diagnostics/focus/CURSOR_FOCUS_TEST.md)
- [Focus-pause test procedure](../ChatGPT/diagnostics/focus/PAUSE_ON_FOCUS_LOSS_TEST.md)
- [Synchronized Bink / DirectSound pause](../ChatGPT/diagnostics/focus/FOCUS_AUDIO_VIDEO_SYNC_TEST.md)
- [Unexpected Windows Ghost focus loss](../ChatGPT/diagnostics/focus/UNEXPECTED_FOCUS_PAUSE_DIAGNOSTIC.md)

## Archived feature reports (ChatGPT workspace)

- [Menu version display](../ChatGPT/tests/reports/MENU_VERSION_TEST.md)
- [Windows taskbar icon](../ChatGPT/tests/reports/TASKBAR_ICON_TEST.md)

## Developer-only tools and regression tests

Development helpers are organized under [ChatGPT/](../ChatGPT/README.md), including automated tests, [INI test fixtures](../ChatGPT/tests/fixtures/README.md), standalone debugging utilities and an external WinDbg prototype. They are not required to install or run the game.

## Repository root

The root retains [README.md](../README.md), [CHANGELOG.md](../CHANGELOG.md) and [RELEASE_NOTES_v1.0.0.md](../RELEASE_NOTES_v1.0.0.md) as the principal public-facing documents.

Some relative paths in older GitHub issues, PR comments or previously shared links may still refer to the original root filenames; those immutable historical references are not rewritten by this documentation-only organization.
