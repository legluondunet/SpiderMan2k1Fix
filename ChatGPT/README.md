# ChatGPT development workspace

This directory stores **internal development tooling** for SpiderMan2k1Fix. It is distinct from the public game installation and the project's maintained [technical documentation](../docs/README.md).

- [Regression tests](tests/) — static/source checks, C helper runners, packaging checks, [INI fixtures](tests/fixtures/) and [feature reports](tests/reports/)
- [Developer utilities](tools/) — optional PowerShell inspection and exit-verification scripts
- [Temporary and external debugger diagnostics](diagnostics/) — WinDbg/CDB experiments and explanatory notes

## Running the tests

From the repository root, run:

```bash
python3 -m pytest -q ChatGPT/tests
```

The CI workflows use this same path. The two C test runners compile against headers at the **repository root**. Build scripts remain at the repository root:

```bash
./build.sh
```

**Distribution:** these files are tracked for developers and CI, but `.gitattributes` marks `ChatGPT/` as `export-ignore` for future GitHub source archives. Binary artifacts and official binary releases independently package just the two DLLs and the public README/CHANGELOG. None of the tooling is needed to install or run the game.

**Scope:** the permanent original-game architecture and reverse-engineering synthesis remains in [`docs/DEVELOPMENT.md`](../docs/DEVELOPMENT.md). Historical standalone **diagnostic and test reports**, experimental methods, and development scripts belong in this workspace. Previously published issue/PR comments may mention the old paths and are not rewritten.

## Archived investigations

### DirectInput

- [Native DirectInput static analysis](diagnostics/directinput/DIRECTINPUT_STATIC_ANALYSIS.md)
- [External DirectInput diagnostics](diagnostics/directinput/DIRECTINPUT_EXTERNAL_DIAG.md)
- [INI-only DirectInput validation](diagnostics/directinput/DIRECTINPUT_INI_FINAL_TEST.md)

### Display and graphics

- [Windows viewport and crash diagnostics](diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md)
- [DirectDraw PRIMARY reuse investigation](diagnostics/graphics/WINDOWS_PRIMARY_REUSE_TEST.md)

### Focus, cursor and audio/video

- [Windows cursor visibility](diagnostics/focus/CURSOR_FOCUS_TEST.md)
- [Focus-pause test procedure](diagnostics/focus/PAUSE_ON_FOCUS_LOSS_TEST.md)
- [Bink and DirectSound synchronized pause](diagnostics/focus/FOCUS_AUDIO_VIDEO_SYNC_TEST.md)
- [Unexpected Windows Ghost focus-pause diagnostic](diagnostics/focus/UNEXPECTED_FOCUS_PAUSE_DIAGNOSTIC.md)

### Feature-validation reports

- [Menu version display](tests/reports/MENU_VERSION_TEST.md)
- [Windows taskbar icon](tests/reports/TASKBAR_ICON_TEST.md)
