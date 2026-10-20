# Linux / Wine compatibility regression

**Goal:** verify that the Windows 11 changes merged in PRs #1–#4 also
work when the supported Windows x86 Spider-Man (2001) game runs in Wine.

There are **two different levels of confidence**:

1. GitHub Actions' `spiderman2k1fix-wine-smoke.yml` compiles both
   production PE32 DLLs, launches a tiny synthetic 32-bit Windows host
   in Wine (Xvfb), and confirms that Wine loads the local native
   `winmm.dll`, WinMM forwarding resolves, and the main fix DLL
   loads. This deliberately **cannot** validate the actual game
   because the repository must not distribute the original game assets.
   The smoke host is rejected by the game's SHA-256 guard.
2. The real **game compatibility** check must run on a Linux machine
   containing the user's legally installed original Spider-Man game.
   This checks the real DirectDraw/Direct3D7 hooks and rendering,
   cinematics, focus handling, cursor, sound and input.

No changes to game runtime code are proposed in this testing PR.

## 1. Preparation — existing Wine installation

Use a working Wine environment (system Wine, Wine-Staging, Lutris,
or a custom Wine runner) that can execute **32-bit Windows games**.
Keep the user's current Wine prefix, graphics settings and game files.

From your installed Spider-Man directory, check:

```bash
wine --version
sha256sum SpideyPC.exe
file SpideyPC.exe winmm.dll SpiderMan2k1Fix.dll
```

The supported executable's SHA-256 is:

```text
0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
```

Install the latest **same-build pair** `winmm.dll` and
`SpiderMan2k1Fix.dll` next to `SpideyPC.exe`. Back up any other
`winmm.dll` first. Wine needs `WINEDLLOVERRIDES="winmm=n,b"`
or the equivalent `winmm` library override in Lutris.

Use `[Debug] Log=1` in `SpiderMan2k1Fix.ini` to record diagnostics.
Do not enable `ExperimentalAltTabReacquire` or
`ExperimentalFocusTrace` for the normal test.

## 2. Automatic log collection during a real game run

From a checkout of this PR's source:

```bash
bash tests/wine-game-regression.sh "/full/path/to/Spider-Man"
```

For a specific prefix and custom Wine runner:

```bash
WINEPREFIX="$HOME/Games/spider-man-prefix" \
WINE_BIN="/path/to/your/wine" \
bash tests/wine-game-regression.sh "/full/path/to/Spider-Man"
```

The script does **not** edit your executable, DLLs, INI or prefix.
It validates the game EXE hash, requires `Log=1`, launches the
installed game with the required `winmm=n,b` override, and records
Wine stdout/stderr and *only newly appended* project log lines.
It creates a timestamped `wine-regression-*.tar.gz` beside the
current working directory; inspect its content before sharing it,
as Windows/Wine logs can contain local user paths.

For additional DLL loading diagnostics, rerun with `WINEDEBUG=+loaddll`.
This produces a larger console log.

**A `0` game process exit and a successful startup log do not alone
prove video, controllers, sound or gameplay are correct**: these must
be assessed on screen and recorded in the test report.

## 3. Native Wine acceptance matrix

| Test | Expected behavior |
|---|---|
| Startup + proxy | `OK: supported SpideyPC.exe SHA-256 verified`, `OK: real system winmm.dll loaded`, and `OK: SpiderMan2k1Fix.dll LoadLibrary succeeded` |
| Native desktop window | Borderless monitor-size window; no desktop resolution switch; inspect `DISPLAY-DPI` and any physical/logical mismatch |
| Menus + four intro movies | Centered 4:3, correct proportions, black margins when needed |
| In-game cinematics/comics | Correct proportions and positioning without unexpected stretching |
| Gameplay + FOV | No rendering corruption; view aspect matches physical display |
| Game settings / virtual CFG | `SPIDEY-CFG: BOOTSTRAP ... READ CONFIRMED`; menu settings update correctly |
| Keyboard + mouse | Normal controls in menu and in gameplay |
| Audio | Voice, music and sound effects all audible as expected |
| Alt+Tab with `PauseOnFocusLoss=0` | Normal game input resumes when returning; no crash; desktop cursor stays visible |
| Alt+Tab with `PauseOnFocusLoss=1` | Game waits and resumes; desktop cursor visible; no stuck keyboard or mouse |
| Cursor option | `RestoreCursorOnFocusLoss=1` enabled by default and `0` disables this specific restoration |
| Taskbar/icon | `ExtractIconExW` is attempted and failure must not break the game; Linux desktops may display an icon differently from Windows Explorer |
| Quit | Clean normal exit; no freeze or access violation |

Recommended run order: first test the **stock INI**
(`AspectRatio=Auto`, `PauseOnFocusLoss=0`,
`RestoreCursorOnFocusLoss=1`). Then repeat with
`PauseOnFocusLoss=1` (the Windows 11 Alt+Tab configuration).
If all works, optionally test `AspectRatio=4:3` and compare
other available monitor resolutions.

If any test regresses, repeat on the **same Wine runner, prefix,
game files and display** using the earlier stable v1.0.0 DLL pair
as an A/B control. Never mix DLLs from different builds.

## 4. Interpreting a failure

- No `START: winmm proxy loaded`: check the Wine `winmm=n,b`
  override and that both DLLs are in the EXE directory.
- `ERROR: cannot load real system winmm.dll`: native proxy
  started, but real multimedia forwarding failed; send Wine console log.
- `DLL-LOAD ... LoadLibrary failed`: examine missing x86 imports,
  wrong architecture, or Wine support for the imported API.
- `EXE-CHECK: ... unsupported`: wrong/modified game binary; do not
  bypass checksum.
- `DDRAW: ... hook not installed`: inspect Wine's DirectDraw loader
  and possible conflicting wrappers.
- `WINDOW-ICON: ... unavailable` alone does **not** establish that
  gameplay or input is broken; icon support varies between platforms.
- `INPUT-WINDOW: CURSOR-TEST` messages: cursor visibility counter
  adjustments should be paired across loss/return transitions.
- Startup works but rendering is black, stretched or broken: provide
  screenshots and new project log. The synthetic CI smoke
  **cannot** detect these real rendering regressions.

### Result to report

Please attach the generated `wine-regression-*.tar.gz` and briefly
state: Linux distribution/session type (X11 or Wayland), Wine version
and runner, GPU, desktop resolution, whether startup/menu/videos/
gameplay/audio/keyboard/mouse/Alt+Tab/cursor/exit worked, and any
difference with v1.0.0.

No merge or claim of Wine game compatibility until the real game
has been exercised and the result reviewed.

---

legluondunet assisted by ChatGPT (OpenAI)
