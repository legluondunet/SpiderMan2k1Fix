# SpiderMan2k1Fix

**A modern compatibility and widescreen fix for Spider-Man (2001) on PC — Windows and Linux (Wine).**

SpiderMan2k1Fix is an **unofficial, fan-made patch** for the original Windows PC release of *Spider-Man*. It helps the game run more comfortably on modern displays and operating systems while preserving its original look when desired.

It adds borderless fullscreen, desktop-resolution output, widescreen support, corrected presentation for introductory movies and comic-book artwork, improved input handling, and an integrated configuration file. It also removes the game's CD check for the supported executable.

SpiderMan2k1Fix is **not a remake or a replacement for the game**. You must provide your own installed copy of *Spider-Man*. No game assets are distributed with this project.

> **Stable release:** Download **[SpiderMan2k1Fix v1.0.0](https://github.com/legluondunet/SpiderMan2k1Fix/releases/tag/v1.0.0)**. Automated Windows x86 builds also remain available through GitHub Actions.

## Features — Version 1.0

- **No-CD:** removes the disc check in memory, without modifying the original executable on disk.
- **Native desktop resolution:** automatically uses your monitor's resolution (`Resolution=Auto`).
- **Borderless fullscreen:** runs in a fullscreen-sized borderless window instead of exclusive fullscreen, without changing your desktop display mode.
- **Widescreen by default:** supports modern aspect ratios while retaining an optional **original 4:3 presentation**.
- **Automatic field-of-view adjustment:** adjusts the game's view for its display aspect ratio, without requiring manual FOV values.
- **Intro movie presentation:** displays the opening publisher/developer movies with a centered **4:3** aspect ratio instead of stretching them across widescreen displays.
- **In-game cinematics:** preserves the intended presentation of the game's low-resolution cinematic content rather than indiscriminately stretching it.
- **Comic-book artwork:** corrects the presentation of comic-style images and panels across display resolutions.
- **HUD presentation:** avoids unwanted stretching of supported HUD elements; further adaptive positioning is planned.
- **Alt+Tab input recovery:** restores keyboard and mouse control when the game regains focus, avoiding the old loss-of-input behavior.
- **Integrated game settings:** stores display, audio, keyboard and joystick bindings in **`SpiderMan2k1Fix.ini`**, replacing the need to maintain a separate `Spidey.cfg` for supported configurations.
- **Automatic INI creation and repair:** generates the INI on first launch and can rebuild malformed configurations while preserving recoverable settings and making numbered backups.
- **Optional debug logging:** writes `SpiderMan2k1Fix.log` when enabled; **disabled by default** to avoid unnecessary logging overhead.
- **30 FPS default limit:** retains the game's intended timing, with a configurable limiter.

[![Windows x86 build](https://github.com/legluondunet/SpiderMan2k1Fix/actions/workflows/spiderman2k1fix-windows-x86.yml/badge.svg)](https://github.com/legluondunet/SpiderMan2k1Fix/actions/workflows/spiderman2k1fix-windows-x86.yml)

### F10 fast-test shortcut — [Debug], default OFF

For testing changes rapidly, you can enable a separate F10 mode:

```ini
[Debug]
F10TestMode=1\nF10Auto3DScripts=1
Log=1
```

With this enabled, **F10 before the main menu navigates to the main
menu**, using the original Bink and static-page cleanup. The first
auto-timed page is now **accelerated** by its own native timeout
comparison, without waiting several seconds or bypassing its
resource/SEH cleanup. **F10 in active 3D gameplay quits the game**
via the existing native controller-7 exit.
F10 in the main menu is ignored. **New Game skip development (partial):**
F10 during the first New Game cover `L1COV.bmp` now satisfies the
original **300-native-tick expiry test immediately** on the game
thread: the game itself advances through its existing native branch
to Bink movie #4. The patch intercepts only the original local
clock READ at `0x4279B8`, never the shared game clock or script
dispatcher, and only after verifying the exact `L1COV.bmp`
and movie #4 instruction signatures. The same single F10 request
then ends movie #4 via its original video cleanup. If signature
validation fails, the old timed-cover fallback is preserved.
**This new comic acceleration is not yet runtime-validated.**
After the Bink movie (which can be skipped using Enter), there are
**two consecutive skippable 3D scenes**: Stan Lee's narrated introduction,
then another 3D cinematic. **Space skips each 3D scene separately**.
These stages and the first genuinely playable frame cannot yet be
distinguished using verified native scene flags. After movie #4, F10
is intentionally blocked from shutting down the process until the
actual playable boundary is established. **F10 does NOT yet skip
either 3D scene or re-enable gameplay exit in the unclassified run.**
With `Log=1` the patch captures separate physical Space key-down
events, a read-only original `0x440EF0` phase-state snapshot, and
the input masks/results of five signature-verified native
`0x50C180` 3D CALL sites (the original function and BYTE result are
preserved). The original input mask `0x04000000` maps to
DIK_SPACE (`0x39`) inside `0x50C180`, but none of the five
observed game sites uses that mask. The read-only `DEBUG-3D-SPACE-GATE` probe observes original
`0x455427` / `0x455444` Space polling sites, but Wine
tests showed their native guard bit `0x100` remains clear
during BOTH introduction skips: those are NOT their handlers.
The `DEBUG-3D-RAW-SPACE` probe now also traces the original
`0x50A650` central keyboard helper's two raw-read calls to
`0x501CB0` (Space key only), including original caller VA,
mode, and raw result, without modifying game input.
Neither evidence alone establishes cinematic completion
or gameplay readiness. No synthetic Space/Enter,
process termination, or arbitrary controller transition is used.
The single `[Debug] F10TestMode` option overrides the older
`[System]` F10 controls and defaults to `0`; `Log=1` is recommended.

The complete previously validated F10 implementation was archived
at [`backup/f12-native-exit-validated-20261023`](https://github.com/legluondunet/SpiderMan2k1Fix/tree/backup/f12-native-exit-validated-20261023)
(commit `4adf16aa13a867a3256f7a2f7ce506cc53b9b82a`).
When `F10TestMode=1`, the game's original menu clock synchronization
is **always preserved**. The experimental menu clock-wait bypass is removed
because it can expose a visible menu before the native clock is ready.
The original approximately 3–4-second first-menu wait may return after F10.
The prior branch is archived at
[`backup/f12-debug-menu-wait-before-experiment-20261023`](https://github.com/legluondunet/SpiderMan2k1Fix/tree/backup/f12-debug-menu-wait-before-experiment-20261023).
See [F10 debug mode and restoration guide](docs/F10_DEBUG_TEST_MODE.md).
**The accelerated startup path has been user-tested. Native clock waits are now preserved; this rollback needs fresh runtime validation.**
The original F10 full-exit behavior remains backed up separately.
PR #26 remains draft and unmerged.

### Legacy F12-named options (now activated using F10) — native cleanup, experimental stages

**F10 is the only physical shortcut recognized by SpiderMan2k1Fix.**
F11 and F12 remain free for MangoHud on Linux. The ORIGINAL game's
F12 -> PostMessageA(WM_CLOSE) WndProc instruction is still suppressed
to avoid an unsafe abrupt window close. Legacy `F12TestMode`,
`F12Auto3DScripts`, `QuickExitF12` and other existing F12-named
INI options remain accepted as configuration aliases for F10.
Their names **do not** reactivate the F12 keyboard shortcut.

For new installations, use `[Debug] F10TestMode=1`,
`F10Auto3DScripts=1` and `Log=1`; this enables the
independently opt-in two-script native New Game skip with a
single F10 on the comic. Off by default; still experimental.

**F12 is OFF by default.** The original 2001 game shortcut can
post `WM_CLOSE` before the game's Bink, DirectDraw and audio
resources are cleaned up, sometimes leaving a headless process.
This fix suppresses that unsafe shortcut and uses the game's
native cleanup paths instead. No `ExitProcess`,
`TerminateProcess`, synthetic Enter or forced window destruction
is used. Changes are being developed in
[draft PR #26](https://github.com/legluondunet/SpiderMan2k1Fix/pull/26);
**the PR is not merged**.

| Stage | Implementation and manual validation |
| --- | --- |
| Four opening Bink videos | **One F12 validated on Windows 11, build #605**: original Bink cleanup, then native timed/input-wait page cleanup and real-menu Quit; no residual `SpideyPC.exe`, **no yellow window**. A very brief main-menu flash remains. Requires `ExperimentalQuickExitF12InIntroStills=1` |
| First timed pre-menu still | **Validated on Windows 11 and Linux/Wine**: F12 defers to original timed-page completion and input-wait cleanup, then original menu Quit; no residual `SpideyPC.exe` in Windows Task Manager |
| Second `Press Enter` pre-menu still | **Validated on Windows 11 and Linux/Wine**: F12 takes original input-wait completion, then original menu Quit; no residual `SpideyPC.exe` in Windows Task Manager |
| Interactive main menu | Original `Shell_MainMenu` Quit command (slot 3, action 18); **validated on Windows 11** |
| New Game comic still | **Validated under Windows 11 and Linux/Wine**: deferred until the comic's original timer and native movie #4 cleanup; Windows confirms no residual `SpideyPC.exe`. **Known ~1s yellow window before exit on both systems** |
| New Game Bink cinematic | **Validated on Windows 11 and Linux/Wine**: original Bink/script return and controller-7 final teardown; Windows confirms no residual `SpideyPC.exe`. **Yellow window for ~1 second before exit** |
| 3D gameplay | **Validated on Windows 11 and Linux/Wine** through native game-owner and controller-7 cleanup; Windows Task Manager confirms no orphan and **no yellow window** |

**Process exit was independently confirmed by the user for the
Wine comic, cinematic and gameplay tests:** no residual game or
Wine process. The real `Shell_MainMenu` native frame tick at
`0x00493DD6` selects the game's own Quit action.
Both F12 during the New Game comic and F12 **directly during
New Game Bink movie #4** are now user-validated on Windows 11
with no residual `SpideyPC.exe`. Both reproduce the same
~1-second yellow-window anomaly before process exit.
The pre-menu static screens have a **separately opt-in**
F12 prototype. The first screen retains its native timed-page completion;
the second uses its original input-wait completion and cleanup.
**Both fixed pages are now independently user-validated under
Windows 11 and Linux/Wine**, with the original menu Quit
executing at the end. Windows Task Manager confirmed no
residual `SpideyPC.exe` after F12 on either fixed page.

Enable only the desired stages in `SpiderMan2k1Fix.ini`
while using the experimental PR build:

```ini
[Debug]
Log=1

[System]
QuickExitF12=1
ExperimentalQuickExitF12In3D=1
ExperimentalQuickExitF12InNewGameMovie=1
ExperimentalQuickExitF12InNewGameComic=1
; Optional pre-menu still-screen F12 (default 0; both pages tested under Windows and Wine):
ExperimentalQuickExitF12InIntroStills=1
```

- `QuickExitF12` activates the main menu and opening-video handling.
  **Opening-video F12:** the original startup dispatcher
  may bypass its Quit-byte check (bug reproduced with
  Windows build #589). The corrected **single-key** handoff
  preserves that SAME F12 request through the original timed
  page and native input-wait cleanup, followed by real
  Shell_MainMenu Quit. **Validated under Windows 11 on
  build #605:** the game exited with no remaining process,
  no yellow window and only a very brief main-menu glimpse.
  This fallback requires `ExperimentalQuickExitF12InIntroStills=1`;
  with that separate setting OFF, the earlier limitation may
  still be present.
- `ExperimentalQuickExitF12In3D` allows the native 3D shutdown.
- `ExperimentalQuickExitF12InNewGameMovie` additionally allows
  clean shutdown during **New Game Bink movie #4**, after native
  video cleanup; it requires `QuickExitF12=1`,
  `ExperimentalQuickExitF12In3D=1`, and `[Debug] Log=1`.
- `ExperimentalQuickExitF12InIntroStills` (default `0`) is
  a separately guarded prototype for **both screens after
  the opening videos but before the real main menu**.
  F12 during the timed first page waits until it ends normally;
  F12 during the second page takes the original input-wait
  loop's regular completion branch `0x4A18AE`, which releases
  its resources and restores SEH. Only after this does
  original `Shell_MainMenu` Quit action 18 execute.
  It requires `QuickExitF12=1` and `[Debug] Log=1`.
  Both pages have been user-validated under Wine
  **and Windows 11**. Independent Windows Task Manager
  checks found no remaining `SpideyPC.exe` process
  after quitting from either page.

- `ExperimentalQuickExitF12InNewGameComic` enables **deferred**
  F12 while the New Game comic still is visible. It requires
  the preceding movie option; the comic is *not immediately
  interrupted*, and a 12-second timeout cancels an unmatched
  request. The normal comic lasts around five seconds in
  the tested sequence.

All five F12 INI settings default to `0`. The two New Game
options are alternatives to the existing **per-process**
environment switches
`SPIDERMAN2K1FIX_F12_POSTMENU_MOVIE_EXIT=1` and
`SPIDERMAN2K1FIX_F12_COMIC_EXIT=1`; those older switches
are retained for compatibility, and still require the same
base F12, 3D and debug gates. If using the INI settings above,
**no environment variables are needed**, including under Wine
or Lutris.

The New Game movie shutdown relies on original movie-owner
CALL `0x004279FB` and the game's cleanup at
`0x00470871`. The comic exit waits until this same movie
starts. Both reuse the normal post-script input checkpoint,
owner cleanup at `0x00455BF9`, and original controller-7
final teardown at `0x0045619D`/`0x0045620C`.
This is why the comic stage does not close instantly.

**Known visual caveat:** a yellow game window remains visible
for approximately one second before the final F12 shutdown
after **both comic-deferred F12 and direct Bink-movie F12
on Windows 11** (also observed with the comic path under Wine).
The original Bink/script, game-owner and controller-7
cleanup checkpoints are reached, and there is no remaining
game process. The exact surface presenting yellow and the
interval between last native teardown and window disappearance
are **not measured**; no display workaround has been introduced.
**Controlled native Windows 11 comparison:** F12 in
active **3D gameplay**, after all Bink films have ended,
also reaches the original controller-7 native final
teardown, but **does not produce any yellow window**.
A fourth control test — F12 on an **opening Bink movie**
with the single-key fix enabled — also exited with
**no yellow window**, although it briefly displayed
the normal main menu as the game's own Quit action ran.
Task Manager confirms no residual process in all three
cases. The common controller-7 cleanup by itself is
therefore unlikely to explain yellow; the starting
Bink/graphics presentation state is a more promising
lead, not a demonstrated root cause.

The next discriminator is F12 during one of the **four
opening Bink videos** on Windows 11, where a different
original Bink→top-level Quit path is used. If yellow
reproduces there, investigate common movie presentation
and teardown rather than only the New Game cinematic.

For detailed disassembly, stage guards, test logs, safety
constraints and the verification plan, see
[`docs/F12_NATIVE_SHUTDOWN.md`](docs/F12_NATIVE_SHUTDOWN.md).
The read-only Windows process watcher
[`tools/Verify-F12QuickExit.ps1`](tools/Verify-F12QuickExit.ps1)
can help confirm that `SpideyPC.exe` terminates rather than
leaving only its window closed.

## Requirements and compatibility

- An installed copy of the **original Windows PC version of Spider-Man**, including its game data.
- The supported **32-bit** `SpideyPC.exe` executable.
- Windows, or Linux with a working **32-bit-capable Wine** installation.
- Two SpiderMan2k1Fix DLLs: `winmm.dll` and `SpiderMan2k1Fix.dll`.

SpiderMan2k1Fix currently checks the game's executable before applying its main patches. The supported `SpideyPC.exe` has this SHA-256 checksum:

```text
0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
```

Other executable versions, modified executables, and some third-party wrappers may not be compatible. Do not rename an unsupported executable or bypass the compatibility check.

### Installing on Windows

1. Install *Spider-Man* normally and locate the directory containing `SpideyPC.exe`.
2. Back up any existing `winmm.dll`, `SpiderMan2k1Fix.dll`, or `SpiderMan2k1Fix.ini` files in that directory.
3. Download **`SpiderMan2k1Fix-1.0.0-Windows-x86.zip`** from [GitHub Releases](https://github.com/legluondunet/SpiderMan2k1Fix/releases/tag/v1.0.0) and extract it. (The GitHub-generated source archives do **not** include compiled DLLs.)
4. Copy `winmm.dll` and `SpiderMan2k1Fix.dll` into the **same directory** as `SpideyPC.exe`:

   ```text
   Spider-Man/
   ├── SpideyPC.exe
   ├── winmm.dll
   ├── SpiderMan2k1Fix.dll
   └── SpiderMan2k1Fix.ini    # generated automatically on first launch
   ```

5. Start the game using `SpideyPC.exe`.

On the first supported launch, SpiderMan2k1Fix creates `SpiderMan2k1Fix.ini` automatically if it does not already exist. You do not need to create it manually.

**Upgrading from an earlier SpiderManFix beta:** Remove the old `SpiderManFix.dll` (it is no longer used). To keep your settings, rename `SpiderManFix.ini` to `SpiderMan2k1Fix.ini` **before launching** the game. If no new-named INI exists, fresh defaults will be generated. Keep a backup of your original INI.

**Important:** Do not overwrite another mod's `winmm.dll` without backing it up first. Two different `winmm.dll` proxy mods cannot normally occupy the same location under the same filename.

### Installing on Linux with Wine

1. Install *Spider-Man* in a Wine prefix and locate the directory containing `SpideyPC.exe`.
2. Copy the **same two Windows DLLs** (`winmm.dll` and `SpiderMan2k1Fix.dll`) next to `SpideyPC.exe`.
3. Configure Wine to load the local `winmm.dll` **before** its built-in implementation.

   In `winecfg`, open **Libraries**, add `winmm` and set it to **Native then Builtin**.

   Alternatively, start the game from a terminal in its directory:

   ```bash
   WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
   ```

   If you use a custom Wine prefix, select it explicitly:

   ```bash
   WINEPREFIX="$HOME/Games/spider-man-prefix" WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
   ```

4. Launch the game. On a supported executable, SpiderMan2k1Fix initializes and creates `SpiderMan2k1Fix.ini` if necessary.

**Wine override explained:** `winmm=n,b` means **native, then built-in**. This allows Wine to load SpiderMan2k1Fix's local `winmm.dll` proxy first. The proxy forwards the original multimedia functions while loading `SpiderMan2k1Fix.dll`.

If you use Lutris or another Wine launcher, add a DLL override with **Name:** `winmm` and **Value:** `n,b` in that game's Wine configuration (or set the equivalent environment variable). You generally do not need a separate override for `SpiderMan2k1Fix.dll`.

## Using SpiderMan2k1Fix

There is **no separate launcher or graphical configuration utility**. Start *Spider-Man* as usual and change its settings through the in-game menus or `SpiderMan2k1Fix.ini`.

### Configuration file

SpiderMan2k1Fix creates `SpiderMan2k1Fix.ini` alongside the game executable. **Close the game before editing the file manually**, because in-game settings may be saved back to it.

The main options include:

```ini
[Debug]
; 0 = disabled (default), 1 = write SpiderMan2k1Fix.log
Log=0

[Display]
; Auto = desktop monitor resolution
Resolution=Auto
; Auto or 4:3 for a native gaming experience
AspectRatio=Auto
Brightness=4
FPSLimit=30

[Input]
; Experimental Windows 11 input recovery; default OFF.
DirectInputReacquire=0
; Pause rendering after a real Alt+Tab; default OFF.
PauseOnFocusLoss=0
; Resume only by pressing and releasing any keyboard key (consumed).
; 1 = show a centered, transparent, red-outlined pause message; 0 = hide it.
PauseOverlay=1
; Darken the paused game behind the centered PAUSE banner (0 disables shade).
PauseDim=1
; Strength of black shade: 0..90 percent (default 50).
PauseDimOpacity=50
; No grid by default; set to 1 only if you prefer the optional 16px grid.
PauseGrid=0
; Pause cinematic Bink video and the game's audio-only Bink music stream.
PauseBinkOnFocusLoss=0
; Pause active game-owned secondary DirectSound effects/voices (default off).
PauseDirectSoundOnFocusLoss=0
RestoreCursorOnFocusLoss=1

[Audio]
; Volume levels: 0..100 percent
MusicSFXLevel=69
VoiceLevel=79
MovieLevel=69
; 1 = Stereo, 0 = Mono
Stereo=1

[Keyboard]
Up=UP
Down=DOWN
Left=LEFT
Right=RIGHT
Jump=SPACE
Kick=X
Punch=C
Web=V
Aim=D
Zipline=F
Swing=G

[Joystick]
; DirectInput button numbers start at zero
Jump=BUTTON3
Kick=BUTTON4
Punch=BUTTON0
Web=BUTTON1
Aim=BUTTON6
Zipline=BUTTON9
Swing=BUTTON7
```

The generated INI may contain additional internal or development-specific settings depending on the release. Leave unfamiliar settings unchanged unless the release notes explain them.

**Display modes:**

- `AspectRatio=Auto` (default): use a modern widescreen layout matching your desktop.
- `AspectRatio=4:3`: request the original 4:3 game presentation. The desktop is not switched to an exclusive display mode; the result should preserve the classic game proportions.
- `Resolution=Auto` (default): use your desktop resolution. A specific `WIDTHxHEIGHT` value may also be configured where supported.
- `FPSLimit=30` (default): preserve the intended 30 FPS cap. `FPSLimit=0` disables the limiter, although higher frame rates may affect an older game's behavior.

**Controls:** keyboard and joystick bindings are independent. Keyboard values use physical DirectInput key names or hexadecimal scan codes. Joystick values use the same `BUTTON0`, `BUTTON1`, etc. numbering shown in the game's **Joystick Configuration** menu. Joystick movement axes/directions are handled by the game and are not configurable through this section.

**Alt+Tab recovery (Windows 11):** Use `DirectInputReacquire=1` and `PauseOnFocusLoss=1` for the tested keyboard/mouse recovery and focus-pause combination. After switching back to the game, press and release any key to resume; the wake key is consumed. The game reuses its native `DIERR_NOTACQUIRED` retry path without wrapping DirectInput COM interfaces. These options remain disabled by default; earlier unsafe COM experiments are no longer active, even if their keys remain in an older user INI. The former environment-variable override is supported for legacy diagnostic scripts, but is unnecessary with the INI setting.

**Windows 11 class=Ghost false focus-loss experiment (PR #11):** If the game repeatedly pauses without Alt+Tab after about five to six seconds, capture `SpiderMan2k1Fix.log`: repeated `FOCUS-PAUSE-OWNER: ... class=Ghost` lines indicate Windows is replacing an apparently unresponsive game window with a ghost surrogate. `[Input] DisableWindowsGhosting=1` (default **0**) requests Microsoft's `DisableProcessWindowsGhosting` API at startup to prevent that replacement and test whether the repeated false pauses stop. **Restart required; it applies to the whole game process until exit and suppresses Windows' normal ability to manage a hung application via a ghost window. It does not fix a truly hung game or change the Alt+Tab pause/input behavior.** Do not enable by default before native Windows testing. See [diagnostic procedure](UNEXPECTED_FOCUS_PAUSE_DIAGNOSTIC.md).

**Pause indicator (PR #14):** With `PauseOnFocusLoss=1`, the game displays a centered, comic-style red-framed `PAUSE` message above the English-only instruction **`PRESS ANY KEY TO RESUME`**. Its white bold-italic wake hint is enlarged, and the rounded red border is strengthened to 4 px. The center of the already-validated 55% dark, 16px gridded pause shade gains an optional secondary nonactivating rounded translucent panel (~71% effective darkening), masked to keep the red frame and white letters fully bright and to attenuate the grid behind the hint. The original shaped child window remains unchanged in principle: its background is not filled with opaque pixels, so the 2001 DirectDraw frame remains visible. If the optional panel cannot be created, the regular 55% shade and indicator continue to work. The only supported pause wake is now pressing and releasing **any key** after returning to the game; the key is consumed. Legacy `PauseKey=F9`, `Space` and `Any` INI entries are tolerated for migration but do nothing. If the Any-key hook cannot be installed, the game skips the pause instead of freezing waiting for a deprecated wake key. `PauseOverlay=0` hides only the indicator, not the underlying focus pause.

**Background grid and shading (PR #17 follow-up):** In addition to the game-bound PAUSE banner, an ordinary full-client `WS_CHILD` carries the **16px grid and a dark pixel dither** while Spider-Man is in the background. A sparse GDI window region removes all untouched pixels, so the paused game remains visible between strokes and the grid can never float over Thunar. Background opacity is an **approximation** (default 55% requests 4 of 8 opaque rows, or roughly 50% area coverage), not a full alpha blend, because the original unmanifested game cannot reliably create layered child windows. When the game is active, the previously approved **true 55% alpha** plus its 16px grid is unchanged. `PauseDim=0` still disables both backgrounds; `PauseGrid=0` omits the grid in either mode. Wine runtime validation required.

**Experimental native ESC-inspired Alt+Tab pause artwork (draft PR #20 based on PR #18):** The original focus-loss mechanics are unchanged: keep the frozen DirectDraw7 frame, pause at the Flip boundary and restore the saved surface on Any-key resume. Based on the maintainer's comparison screenshots, the **large orange spider motif is now completely removed** (including the earlier placeholder oval). Only the original-style wide **translucent violet menu ribbon** remains, with the traced D3D7 diffuse `0x7F3232A0` panel at native y≈400.5–675.0 and a restrained 12-strip blue-to-amethyst vertical tint. The top and bottom rims have a two-tone lavender/white highlight and dark inner accent; they are **GDI approximations**, because the game's actual 8×8 border texels have not been recovered. The heavier, outlined red `PAUSED` title now sits **inside the top half of the ribbon** (native logical y≈445) with a clearly separated white `PRESS ANY KEY TO RESUME` line lower in the ribbon (y≈545). The icon before the instruction is now the **real, largest available icon extracted directly from the running `SpideyPC.exe` at runtime**. The maintainer-supplied reference ICO contains 16×16 and 32×32 images, so the Win32 `ExtractIconExW` LARGE handle selects the 32×32 game icon, which `DrawIconEx` scales to the existing 48-unit ribbon position. No game icon image is copied, committed or shipped. The title and instruction remain horizontally centered; extraction failure shows the text centered without an icon and does **not** prevent pausing. In a 16:9 gameplay frame, the purple ribbon, its borders, both texts and the original EXE icon are constrained to a **centered 4:3-sized region** (at 1920×1080, x=240–1680) rather than stretching across the full screen; the actual 4:3 menu region remains unchanged. Full-screen dimming and pause-surface capture/restoration are unchanged. All UI drawing remains in the game's content region with no overlay HWND; darkening remains `PauseDimOpacity=50` and `PauseGrid=0` by default, respecting existing INI overrides. This is an experimental visual version requiring Wine/Windows testing, not yet considered visually approved. PR #18, PR #19 and `main` are not changed or merged.

**Background-visible pause hint (PR #17):** When Spider-Man is paused but another application is foreground, the English `PAUSE / PRESS ANY KEY TO RESUME` banner remains **inside the game's own client window** as a conventional `WS_CHILD`. It uses an opaque dark-plum fill and 2px muted-red frame, avoiding the unsupported layered-child alpha and inheriting game-window clipping and desktop stacking. Thunar, Firefox and other active desktop windows should cover that background banner normally. When Spider-Man becomes active again, the banner switches to its per-pixel translucent comic version and the **55%-black, 16px-grid shade** reappears; while the game is in the background, the floating grid popup stays hidden to prevent focus leakage. The underlying game/Bink/DirectSound remain paused during either mode and Any-key resume remains unchanged. Test on Wine and Windows 11 before merging.

**Simplified comic PAUSE panel (PR #16):** To avoid black glyph fragments and cross-window artifacts in Wine, replace the former shaped child plus two alpha popups with a **single game-owned, nonactivating, per-pixel alpha banner** for the red frame and full-white text, plus the original independently configurable 55% grid shade behind it. The entire rounded banner is removed from the grid overlay region. The banner uses an atomic `UpdateLayeredWindow` bitmap (transparent outside rounded corners; muted dark-plum background; fully opaque glyphs/border), **not** an extra 35% panel, a color key or a layered child. Only display while Spider-Man is the Windows foreground **and active** window; otherwise keep game/audio paused but hide the visual layers. Wine and Windows 11 runtime acceptance is required; no merge/release yet.

**Wine/Linux focus-only pause visuals (PR #15):** The game and enabled Bink/DirectSound streams pause as soon as actual loss of focus is confirmed, but the PAUSE panel and its game-owned translucent black/grid popups are **not shown while another application owns the foreground**. They appear only when Spider-Man is foreground again and disappear if it loses focus a second time while paused; this avoids drawing a floating red border on top of Linux desktop applications. Inside the banner, all 16 px grid strokes are clipped out and the additional darkened center remains translucent; the 25 px bold upright `PRESS ANY KEY TO RESUME` hint is protected by expanded shaped-glyph masks. Retains consumed ordinary-key resume (no dedicated F9/Space modes), while passing **Alt+Tab and Windows desktop-switch shortcuts** through so users can leave the game again during a pause. **Note:** requires real Windows 11 and Wine regression testing before release, since GDI layering and popup stacking are compositor-dependent.\n\n**Dimmed pause background (PR #12):** When `[Input] PauseOnFocusLoss=1`, a non-topmost, nonactivating layered **popup owned by the game window** darkens the paused scene with a **55% black tint** and a fine **16-pixel square grid**. The original red-framed comic-style `PAUSE` message stays centered and bright: the popup is cut out around the exact border and white glyphs of the existing shaped banner. The effect appears only during a latched focus pause, and both windows are destroyed before video/audio resumes. `[Input] PauseDim=1` enables the shade (default on, although the underlying focus pause remains disabled by default). `PauseDimOpacity=55` adjusts darkness from `0` to `90` percent. `PauseGrid=1` enables the fine square pattern, and `PauseGrid=0` keeps a smooth dark tint. Set `PauseDim=0` to restore the previous validated plain paused image. The grid is painted inside the original nonactivating alpha popup. PR #14 can add a second small **game-owned, nonactivating popup** for the darker center only; neither changes DirectDraw, Bink, DirectSound or DirectInput. The popup avoids the original 2001 EXE's layered-child compatibility restriction and is not desktop-topmost. Verify the final look and Z-order on native Windows 11; automated CI cannot validate the compositor.

**Synchronized audio and video focus pause (PR #9):** The separately enabled `PauseBinkOnFocusLoss=1` pauses Bink cinematic streams **and the Bink-based music heard during 3D gameplay** using the game's original `BinkPause` API. `PauseDirectSoundOnFocusLoss=1` pauses currently playing game-owned secondary DirectSound buffers and resumes the same buffers from their retained cursor position. The primary buffer is untouched. Both new media options are **disabled by default**. Their combined operation was tested in Windows 11 gameplay, the menu, and cinematics with no audio desynchronization. See [technical notes and test results](FOCUS_AUDIO_VIDEO_SYNC_TEST.md) for diagnostics, compatibility limits, and rollback.

**Audio:** the three volume levels use user-friendly percentages; `Stereo=1` enables stereo and `Stereo=0` selects mono.

### Debug log

Debug logging is **off by default**:

```ini
[Debug]
Log=0
```

If you need to investigate a problem, set `Log=1`, save the file, and **restart the game**. SpiderMan2k1Fix will write `SpiderMan2k1Fix.log` in the game directory. **Each game launch starts a fresh log:** the previous session is overwritten (the proxy startup lines and main DLL diagnostics are retained together). Copy or rename the log before your next launch if you need to keep it. After collecting the information you need, return to `Log=0`.

### Automatic INI recovery

If the configuration is missing, SpiderMan2k1Fix generates a new one. If it detects malformed lines, unknown sections/options, invalid values, or duplicates, it can:

1. Rename the previous file to the next unused backup name, such as `SpiderMan2k1Fix_0.old`, `SpiderMan2k1Fix_1.old`, and so on.
2. Generate a clean `SpiderMan2k1Fix.ini` using the recognized sections and defaults.
3. Recover known, valid values from the old configuration. When duplicate values exist, the **first valid** one is kept.

Unknown settings are retained in the `.old` backup rather than being copied into the clean INI. The `[Internal]` section contains `RawHex`, the game's 252-byte internal settings image; **do not edit this value manually**.

## Troubleshooting

**The game launches, but SpiderMan2k1Fix does not seem active**

- Verify that both DLLs are in the same directory as `SpideyPC.exe`.
- On Linux, check the Wine DLL override: `WINEDLLOVERRIDES="winmm=n,b"`.
- Check that the executable is the supported version. On Linux, run `sha256sum SpideyPC.exe`; on Windows PowerShell, run `Get-FileHash .\SpideyPC.exe -Algorithm SHA256`.
- Temporarily enable `Log=1` and check `SpiderMan2k1Fix.log`, if an INI was generated.

**The INI seems to have reset itself**

Look for a numbered `SpiderMan2k1Fix_N.old` backup. The previous configuration may have contained unknown or malformed entries; known valid settings should be recovered in the replacement file.

**Controls are not responding after switching applications**

When testing the current development build, set `DirectInputReacquire=1` and `PauseOnFocusLoss=1`. Any keyboard key now resumes after focus returns; the old `PauseKey` selection is ignored. If recovery fails, enable `[Debug] Log=1`, then check for `DI8-RETRY: EXPERIMENTAL requested source=INI` and `DI8-RETRY: EXPERIMENTAL active at Flip #120`. For synchronized sound, enable both additional media-pause settings described above. A previous report of intermittent audio crackling occurred under ProcDump but was not reproduced during ordinary launches without ProcDump; its cause remains unknown. Stable release builds may provide a different option set.

**The game has visual or audio glitches**

Try the original `AspectRatio=4:3` mode to help isolate widescreen-specific problems. Some edge-of-screen 3D rendering issues and audio artifacts are still being investigated; see the roadmap below.

### Experimental native F9-to-PNG screenshots — PR #25

The original game detects F9 via DirectInput. In the supported SHA-256
verified executable, its screenshot writer lives at RVA `0x001033E0`.
**Two independent game call sites** invoke that function: RVA
`0x0010A1DC` and `0x0010A62B`. Both sites were confirmed by
disassembling the user's exact `SpideyPC.exe` (1,507,328 bytes). Both
calls follow the game's own F9 polling and 500-ms repeat limit.

Our **second-stage experimental PNG-only mode** replaces just those two
5-byte x86 `CALL rel32` instructions after verifying their original
bytes. It leaves the native DirectInput polling, sound, debounce,
gameplay, and UI code unchanged. When F9 fires, SpiderMan2k1Fix locks
the real 3D backbuffer, copies the full-resolution 32-bit pixels into
private memory, unlocks DirectDraw, then saves the picture as a lossless
PNG on a worker thread through Windows/Wine GDI+ (loaded dynamically).
Only `scrnNNNN.png` is created; the original BMP routine is **never
called** while both PNG-only hooks are installed successfully.

**Test PNG-only (Windows 11 and then Wine) using matching DLLs
from the latest draft PR #25 build:**

1. Back up both DLLs, set `[Debug] Log=1` in the INI, and launch with:

   ```powershell
   $env:SPIDERMAN2K1FIX_F9_PNG_ONLY = "1"
   .\\SpideyPC.exe
   ```

2. Press F9 during 3D gameplay. Confirm a new `scrnNNNN.png` appears,
   **without any new `scrnNNNN.bmp`**. Inspect the PNG for correct
   colors, HUD, and orientation. Press F9 multiple times at least
   500 ms apart; test Alt+Tab and both supported aspect ratios.
   **After the PNG is actually saved**, the game briefly displays
   `Screenshot saved to:` followed by the **full absolute path** of
   the new PNG. The notification disappears after about 3.5 seconds
   and is painted over the visible primary surface **after** the
   backbuffer is presented, so it is **not embedded in F9 PNGs**.
   On a narrow display the complete path wraps onto additional lines.
   The message does not appear for unsuccessful/skipped captures.
3. Check the log for `F9-PNG-ONLY: two SHA-verified F9 call sites`,
   `F9-PNG-ONLY: backbuffer capture=queued`, and
   `F9-PNG: saved scrnNNNN.png`. A lock/encoder failure is logged and
   **does not silently create a BMP** in PNG-only mode.
4. Clear the environment variable when finished:
   `Remove-Item Env:SPIDERMAN2K1FIX_F9_PNG_ONLY -ErrorAction SilentlyContinue`.

**Safety:** signature mismatch or protection failure leaves both
original F9 calls intact. The mode is disabled by default; this is
still an unmerged experiment. Only the supported EXE SHA-256 may be
patched. No global keyboard or COM DirectInput vtable hooks are added.

**Original dual-format comparison test remains available** by setting
`SPIDERMAN2K1FIX_F9_PNG=1` (not `PNG_ONLY`) with `Log=1`. It
captures the same real backbuffer to PNG while letting the game write
its original BMP for pixel comparison. The Windows 11 dual-format test
previously found **99.9977% RGB-identical pixels** in a 1920x1200
capture, correctly oriented, with intact HUD. The original magenta-BMP
fix is maintained separately in PR #24. No new external codec
install is needed on normal Windows or Wine installations.

### Native Windows 11: experimental F9 screenshot repair

On native Windows 11 the game has produced `scrnNNNN.bmp` at
1920×1200 containing only **RGB(255,0,255)**, even though the game is
visible on screen. The same F9 shortcut captures a valid 1920×1080 BMP
under Wine; the two hosts simply use different display resolutions.

**The root cause is now localized.** Native Windows F9 logs show the
SHA-verified game calls `IDirectDrawSurface7::Lock` on the **windowed
PRIMARY** at game RVA `0x001034D4`, with `DDLOCK_READONLY`,
1920×1200, 32-bit color and HRESULT success, then `Unlock` at RVA
`0x00103537`. The 3D scene is actually rendered into a separate
`borderless_back` surface and copied to PRIMARY during each Flip. It
appears native DirectDraw returns stale/magenta pixels when PRIMARY is
locked for CPU readback, even though the window itself is displaying
valid frames; this still needs verification with a fresh BMP.

**Experimental fix (disabled by default):** the separate F9 PR #24
allows `SPIDERMAN2K1FIX_F9_BACKBUFFER=1` with `[Debug] Log=1`.
When enabled, only this exact screenshot `Lock` call is transparently
redirected to the real 3D backbuffer, and only its matching `Unlock`
is redirected back. All other Lock/Unlock calls and the rest of the
game continue unchanged. The backbuffer lock is accepted only when it
returns a full-resolution 32-bit buffer with expected pitch. If the
lock or format fails, the game falls back to the original PRIMARY
readback, and the log records the failure.

**Native Windows 11 test (both matching DLLs from the latest PR #24
artifact):**

1. Back up the existing DLLs and INI. Enable `[Debug] Log=1` but
   leave all validated HUD, pause, aspect ratio and borderless options
   unchanged.
2. In PowerShell from the game folder, launch with both diagnostics
   and experimental fix enabled:

   ```powershell
   $env:SPIDERMAN2K1FIX_F9_TRACE = "1"
   $env:SPIDERMAN2K1FIX_F9_BACKBUFFER = "1"
   .\\SpideyPC.exe
   ```

3. Start a 3D level and press **F9 once**. Quit normally, then inspect
   the fresh `scrnNNNN.bmp` and `SpiderMan2k1Fix.log`.
4. Look for `F9-FIX: BACKBUFFER Lock success` followed by
   `F9-FIX: BACKBUFFER Unlock`, or a clean fallback reason.
   **Success is only established if the new BMP contains the scene**.
   If the game crashes or its display changes unexpectedly, restore
   the previous DLLs and provide the log.
5. To disable the experiment in PowerShell:
   `Remove-Item Env:SPIDERMAN2K1FIX_F9_BACKBUFFER -ErrorAction SilentlyContinue`
   and `Remove-Item Env:SPIDERMAN2K1FIX_F9_TRACE -ErrorAction SilentlyContinue`.

Read-only observation without redirection is still available by
setting only `SPIDERMAN2K1FIX_F9_TRACE=1` together with `Log=1`.
Neither setting is added to the default INI or enabled for normal
users. PR #24 remains **DRAFT** pending native Windows and Wine
validation; this change is deliberately isolated from HUD/default-on
and pre-1.0 cleanup PRs.

## Adaptive widescreen HUD

**Enabled by default.** SpiderMan2k1Fix automatically corrects the
proportions and positioning of the gameplay HUD on supported widescreen
resolutions. It preserves the original 4:3 presentation, menu, videos,
world geometry and game-owned vertex buffers.

The corrected elements include Spider-Man's portrait, life and web
gauges, web cartridge counter, mission instructions, compass, boss
health bars, boss portraits, damaged-health black masks, and the
Scorpion/Jameson objective bar and icons. The game uses several
rendering paths, so the correction uses verified original-EXE call
sites plus conservative texture, depth and geometry checks. Unknown
gameplay draws are passed through.

### Configuration

```ini
[HUD]
HUDWidescreenFix=1
```

- `HUDWidescreenFix=1`: enable the adaptive HUD (**factory default**).
- `HUDWidescreenFix=0`: explicitly disable the adaptive HUD.
- If `[HUD]` or the setting is absent from an otherwise valid INI, the
  correction is **enabled** as though `HUDWidescreenFix=1`.
- With `[Display] AspectRatio=4:3`, widescreen HUD transformation is
  automatically bypassed, regardless of the enabled setting.

**Existing user settings are preserved.** If an INI written before this
update contains `HUDWidescreenFix=0`, that is an explicit opt-out and
continues to disable the correction. Change it to `1` (or remove the
key from a valid INI) to enable the HUD without resetting unrelated
settings. Missing or repaired INIs use the new default `1`.

### Validation and optional diagnostics

Tested against original 4:3 reference screenshots, with the HUD
visually checked under Linux/Wine at **1920×1080 (16:9)** and native
Windows 11 at **1920×1200 (16:10)**. Tests covered player UI,
instructions, compass, both single (Venom) and dual (Scorpion +
J. Jonah Jameson) boss health bars, moving black lost-health areas,
boss portraits and a horizontal objective bar with its icons. Other
boss fights and ultrawide resolutions were not individually tested.

The original game's F9 BMP capture can produce a magenta image under
native Windows 11; that is a separate issue, not a HUD problem.

The developer-only F12 read-only geometric diagnostic remains
available temporarily when BOTH `[Debug] Log=1` and the environment
variable `SPIDERMAN2K1FIX_HUD_TRACE=1` are enabled. In 3D gameplay,
press F12 once for six frames of `HUD-PROBE` diagnostics. It is **not**
needed for normal HUD operation and will be removed during the
pre-1.0 cleanup. See [PR #21](https://github.com/legluondunet/SpiderMan2k1Fix/pull/21)
for test evidence and development details.

## Roadmap

These are **planned improvements**, not features promised for version 1.0:

- **Further HUD coverage:** test additional boss encounters and ultrawide aspect ratios (such as 21:9).
- **Audio improvements:** reduce or eliminate unwanted audio noise and artifacts.
- **Widescreen 3D geometry:** investigate 3D objects that appear displaced near the left or right edges of the screen in landscape/widescreen mode.
- **Electro:** investigate and fix the outstanding Electro-related issue.

## Building from source

For ready-to-use DLLs, use the [latest GitHub release](https://github.com/legluondunet/SpiderMan2k1Fix/releases). GitHub Actions also makes its compiled ZIP available under **Actions → SpiderMan2k1Fix Windows x86 → successful run → Artifacts**.

The source package includes scripts for building the 32-bit Windows DLLs on Linux with an **MSVCRT-targeting MinGW-w64 toolchain**:

```bash
./build.sh
```

If your system compiler targets UCRT instead, consult `build-native-msvcrt.sh` in the source tree. The build scripts check the 32-bit Windows architecture and expected imports/exports. The resulting `winmm.dll` and `SpiderMan2k1Fix.dll` are Windows DLLs even when built on Linux.

## Project and contributions

SpiderMan2k1Fix is an independent compatibility project maintained by **[legluondunet](https://github.com/legluondunet)**.

Bug reports and reproducible test results are welcome, especially for different Windows versions, Wine versions, GPUs, resolutions and controller models. When reporting an issue, include your platform, game executable checksum, relevant INI settings, and a debug log when available.

**Project repository:** <https://github.com/legluondunet/SpiderMan2k1Fix>

*Spider-Man and all associated trademarks belong to their respective owners. This project is unofficial and is not affiliated with or endorsed by the game's original developers or publishers.*
