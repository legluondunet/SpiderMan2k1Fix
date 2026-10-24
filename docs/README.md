# Project documentation

This directory contains the maintained technical documentation and an index of supporting references for **SpiderMan2k1Fix**. Historical experiments may describe superseded instructions; for active behavior consult the [project README](../README.md), source code and current configuration.

## Main documentation

- [Development and original EXE reverse engineering](DEVELOPMENT.md) — synthesized architecture, implementation decisions, and known limitations
- [Version 1.0.0 candidate — source/code audit and release-readiness gates](references/RELEASE_1_0_AUDIT.md) — evidence, cleanup, regression risks and publication checklist

## Configuration and gameplay reference

- [Advanced configuration, INI recovery and troubleshooting](references/CONFIGURATION.md)
- [Startup and INI recovery technical design — early DirectDraw synchronization, native resolution, rolling backup, Wine evidence](references/STARTUP_INI_RECOVERY_TECHNICAL.md)
- [Automatic keyboard layout: Windows APIs, DIK mapping, CHEATS table patch, safety and test evidence](references/KEYBOARD_LAYOUT_TECHNICAL.md)
- [F9 screenshot format, safety and validation](references/F9_PNG_CAPTURE.md)
- [Historical experimental focus-pause UI notes (superseded)](references/PAUSE_UI_HISTORY.md)

## Preserved reverse-engineering findings and test methodology

- [Original `SpideyPC.exe` function/address catalog](references/ORIGINAL_EXE_FUNCTION_CATALOG.md) — recovered engine roles, call sites and evidence limits
- [Historical F12 native process-exit results](references/F12_EXIT_ARCHIVE_SUMMARY.md) — Windows/Wine phase-specific passes and open yellow-window issue; **not an active shortcut**
- [Manual Linux / Wine / Proton compatibility checklist](references/WINE_REGRESSION_CHECKLIST.md) — updated from unmerged PR #5; no implied Wine CI or full gameplay certification
- [Branch-pruning research preservation audit](references/BRANCH_RESEARCH_PRESERVATION.md) — findings already archived and branches worth temporarily retaining for their **experimental source code**

## Detailed references (`docs/references/`)

### Gameplay features and validation

- [F10 validation matrix](references/F10_VALIDATION_MATRIX.md)
- [F10/F12 debug research](references/F12_DEBUG_TEST_MODE.md)
- [Native shutdown research](references/F12_NATIVE_SHUTDOWN.md)
- [Native ESC pause menu and 4:3 rendering](references/NATIVE_PAUSE_MENU_43.md)

### DirectInput diagnostics

- [Static DirectInput analysis](references/diagnostics/directinput/DIRECTINPUT_STATIC_ANALYSIS.md)
- [External DirectInput diagnosis](references/diagnostics/directinput/DIRECTINPUT_EXTERNAL_DIAG.md)
- [INI-only DirectInput recovery validation](references/diagnostics/directinput/DIRECTINPUT_INI_FINAL_TEST.md)

### Display and graphics diagnostics

- [Windows viewport and shutdown diagnosis](references/diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md)
- [Windows primary-surface reuse tests](references/diagnostics/graphics/WINDOWS_PRIMARY_REUSE_TEST.md)

### Focus and synchronized media pause diagnostics

- [Cursor visibility on focus loss](references/diagnostics/focus/CURSOR_FOCUS_TEST.md)
- [Focus-pause test procedure](references/diagnostics/focus/PAUSE_ON_FOCUS_LOSS_TEST.md)
- [Synchronized Bink / DirectSound pause](references/diagnostics/focus/FOCUS_AUDIO_VIDEO_SYNC_TEST.md)
- [Unexpected Windows Ghost focus loss](references/diagnostics/focus/UNEXPECTED_FOCUS_PAUSE_DIAGNOSTIC.md)

### Feature test reports

- [Menu version display](references/tests/MENU_VERSION_TEST.md)
- [Windows taskbar icon](references/tests/TASKBAR_ICON_TEST.md)

## Development-only scripts and fixtures

The [ChatGPT workspace](../ChatGPT/README.md) contains the automated regression tests, [INI fixtures](../ChatGPT/tests/fixtures/README.md), PowerShell utilities and an external WinDbg prototype. Reports and explanatory Markdown formerly stored alongside these tools are now collected under `docs/references/`.

## License, game files and non-affiliation

- **License:** SpiderMan2k1Fix's own source code is licensed under [GNU GPL version 3](../LICENSE) (**GPL-3.0-only**). The license does not apply to the proprietary original game.
- **Original game required:** No game executable or original assets are distributed here. Users must provide their own legally obtained copy of the original *Spider-Man* Windows PC game.
- **Independent project:** Not affiliated with, sponsored by, or endorsed by Activision, Neversoft, LTI Gray Matter / Gray Matter Interactive, or any other original developer, publisher, distributor or rights holder.

## Repository root

The main project files at the repository root are [README.md](../README.md) and [CHANGELOG.md](../CHANGELOG.md). Development builds are available through GitHub Actions; no stable release is available for the current project state.

Historical external URLs to the previous file paths (issues, commits and comments) are not automatically updated by this move.
