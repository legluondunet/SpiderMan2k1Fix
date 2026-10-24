# Project documentation

This directory contains technical references, historical investigation notes and test procedures for **SpiderMan2k1Fix**. Documents describing historical experiments can contain superseded instructions; consult the current [README](../README.md), source code and configuration for the active behavior.

## Main references

- [Development and original EXE reverse engineering](DEVELOPMENT.md)
- [F10 validation matrix](F10_VALIDATION_MATRIX.md)
- [F10/F12 debug research](F12_DEBUG_TEST_MODE.md)
- [Native shutdown research](F12_NATIVE_SHUTDOWN.md)
- [Native ESC pause menu and 4:3 rendering](NATIVE_PAUSE_MENU_43.md)

## Historical diagnostics

### DirectInput

- [Static DirectInput analysis](diagnostics/directinput/DIRECTINPUT_STATIC_ANALYSIS.md)
- [External DirectInput diagnosis](diagnostics/directinput/DIRECTINPUT_EXTERNAL_DIAG.md)
- [INI-only DirectInput recovery validation](diagnostics/directinput/DIRECTINPUT_INI_FINAL_TEST.md)

### Display and graphics

- [Windows viewport and shutdown diagnosis](diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md)
- [Windows primary-surface reuse tests](diagnostics/graphics/WINDOWS_PRIMARY_REUSE_TEST.md)

### Window focus and audio/video pause

- [Cursor visibility on focus loss](diagnostics/focus/CURSOR_FOCUS_TEST.md)
- [Focus-pause test procedure](diagnostics/focus/PAUSE_ON_FOCUS_LOSS_TEST.md)
- [Synchronized Bink / DirectSound pause](diagnostics/focus/FOCUS_AUDIO_VIDEO_SYNC_TEST.md)
- [Unexpected Windows Ghost focus loss](diagnostics/focus/UNEXPECTED_FOCUS_PAUSE_DIAGNOSTIC.md)

## Other feature tests

- [Menu version display](tests/MENU_VERSION_TEST.md)
- [Windows taskbar icon](tests/TASKBAR_ICON_TEST.md)

## Developer-only tools and regression tests

Development helpers are organized under [ChatGPT/](../ChatGPT/README.md), including automated tests, standalone debugging utilities and an external WinDbg prototype. They are not required to install or run the game.

## Repository root

The root retains [README.md](../README.md), [CHANGELOG.md](../CHANGELOG.md) and [RELEASE_NOTES_v1.0.0.md](../RELEASE_NOTES_v1.0.0.md) as the principal public-facing documents.

Some relative paths in older GitHub issues, PR comments or previously shared links may still refer to the original root filenames; those immutable historical references are not rewritten by this documentation-only organization.
