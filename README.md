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

### Optional F12 quick exit — menu validated; startup cinematic exit experimental

> **Windows 11 validation:** Build #471 successfully exits from the actual main menu through the native `Shell_MainMenu` Quit action (slot 3, command 18); the user verified that `SpideyPC.exe` disappears from Task Manager. In build #476, F12 during the opening videos successfully skips the **four** native Bink clips, but the game then stops on one of **two static startup screens**, the latter prompting Enter to reach the main menu. The new *full startup exit* path is **experimental and has not yet been tested on Windows**. Comic pages remain unsupported; 3D has a separately enabled, **unvalidated** native-exit proof of concept. [PR #26](https://github.com/legluondunet/SpiderMan2k1Fix/pull/26) stays a draft.

F12 is off by default. To opt into the experimental build, set:

```ini
[System]
QuickExitF12=1

[Debug]
Log=1
```

The 2001 game's original VK_F12 directly posts `WM_CLOSE`, which can destroy only the game window while native Bink audio and DirectDraw continue forever. SpiderMan2k1Fix disables that exact signature-checked native shortcut to avoid a headless process.

**Validated menu:** The real `Shell_MainMenu` at `0x00493990` executes the game-owned frame tick at `0x00493DD6`. When F12 is pressed in the interactive main menu, the original game receives selection **3**, corresponding to **Quit command 18**, and follows its own cleanup branch at `0x004941D5`. The native menu mechanism is unchanged from the user-validated build #471.

**Opening movie shutdown — revised mechanism:** The game's startup at `0x00455C90` calls movie owner `0x00470750` **four times**. F12 during these movies latches a startup exit request; the original Bink frame-tick CALLs at `0x0047081D` and `0x0047085F` can then report completion. The video-owning game thread follows its existing Bink cleanup at `0x00470871` for all remaining startup clips.

**Important difference from #476:** The first `Shell_MainMenu` callback after the videos may belong to the **two static startup screens**, not the interactive main menu. Delivering Quit there did **not** terminate the game. Rather than entering or simulating input on those screens, this revision hooks the exact native call at `0x00455D23` to transition-cleanup function `0x00443AD0`, *executes the original cleanup first*, then, if an opening F12 request is latched, sets the game's own Quit byte at `0x0060CF88`. Its original top-level check at `0x00455E50` is downstream of that transition and branches to native engine cleanup at `0x004561F2`. This may let the game terminate **before reaching either static screen**, without invoking a native destructor from the key watcher thread.

The new path is protected by exact native machine-byte signatures for the video ticks, the post-movie call and the game's Quit check. **No `WM_CLOSE`, synthetic Enter, `ExitProcess` or `TerminateProcess`** is used.

**3D gameplay F12 — new, separate experimental opt-in (not yet Windows-validated):** The original 3D game loop executes `0x00440EF0` and checks the controller request immediately after each frame. Actual Windows logs #503 and #506 prove that request `7` produced during this native frame leads through game-owned cleanup. Build #508 proves that the top-level controller normally returns to menu with `bit4=0,status=-1` after the native `0x0048A820` cleanup.

Enable *both* options **only when testing this development PR**:

```ini
[System]
QuickExitF12=1
ExperimentalQuickExitF12In3D=1

[Debug]
Log=1
```

When F12 is pressed in a recognized 3D frame, the key-watcher thread only latches an intent. At the **end of the game's own 3D input-owner callback**, the game thread requests controller transition `7`, so the original 3D loop takes its existing owner cleanup branch `0x00455BF3`. After the original cleanup at `0x004302D0` has returned, and **after** the top-level controller executes both `0x00443AD0(0)` and `0x0048A820`, the same native game thread sets **only bit 4** in the original engine shutdown flag at `0x02E0988C`. The game's **unchanged** `0x0045619D` test then enters its final cleanup `0x0045620C`. No fake keystrokes, window destruction, `ExitProcess` or `TerminateProcess` are used.

This new path is isolated behind `ExperimentalQuickExitF12In3D=0` by default, and is only eligible when the real 3D owner and non-menu renderer have run recently. The owner cleanup checkpoint must be observed before any final-exit flag is set. A first live Windows test should press F12 **during actual 3D gameplay**, confirm the window, audio and `SpideyPC.exe` process all disappear, and check both these markers:

```text
QUICK-EXIT-F12: 3D game-thread frame completed; native request 7 queued for original 0x455BF3 cleanup
QUICK-EXIT-F12: 3D owner+controller cleanup complete; engine native final-exit flag set at 0x45619D
```

The last marker alone does **not** prove an OS process exit. Check the process and crash events with the PowerShell watcher. Regression-test normal menu F12 with the new option set back to `0`. Comics and the two static startup screens are **still not implemented**.

**Next Windows test:** Press F12 in the first opening cinematic, confirm the audio ends and `SpideyPC.exe` fully disappears without remaining on either static image. Inspect the log for:

```text
QUICK-EXIT-F12: intro movie exit requested
QUICK-EXIT-F12: four native movie calls completed; intro transition cleanup finished; engine Quit flag set BEFORE top-level 0x455E50 check
```

If the second marker occurs but the game still remains, the top-level native check may not have been reached on that startup path. This would require further investigation; do not call it a successful exit.

For external process-exit and Windows crash-event checks, run [`tools/Verify-F12QuickExit.ps1`](tools/Verify-F12QuickExit.ps1) from the game folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\\Verify-F12QuickExit.ps1
```

This read-only watcher reports the game exit code and checks for leftover processes. It never kills the game. A stopped process releases Windows-owned handles/memory but does not prove that every COM resource was explicitly released by the game.

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
