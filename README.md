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

## Adaptive HUD — experimental investigation for 1.0

The game has several distinct HUD rendering paths. The existing opt-in
`[HUD] HUDWidescreenFix=1` (normally **0**) contains 16 strict SHA-256-
guarded hooks for the **upper-left portrait, health/web bars, cartridge
counter and associated glyphs**, developed from the earlier GDB prototype.
It scales horizontal dimensions without changing original vertical
positions, but its in-game safety and coverage are **not yet validated**.
The **bottom-right compass** and **lower-left instruction box** are rendered
through other paths and can still be stretched at 16:9.

A first **read-only F12 sampler** on the separate experimental HUD branch
is available to identify those paths accurately, without any coordinate
changes or additional COM hooks. It is deliberately excluded from `main`.

1. Keep `[Debug] Log=1`, `[HUD] HUDWidescreenFix=0` for a first baseline.
2. In the game's folder under Linux/Wine, launch
   `SPIDERMAN2K1FIX_HUD_TRACE=1 WINEDLLOVERRIDES="winmm=n,b" wine ./SpideyPC.exe`.
   On native Windows PowerShell, set
   `$env:SPIDERMAN2K1FIX_HUD_TRACE="1"` before executing `.\SpideyPC.exe`.
3. Once in **3D gameplay**, press **F12 once** and allow six frames to run,
   then quit. Send `SpiderMan2k1Fix.log` and a gameplay screenshot.
   The bounded `HUD-PROBE` records describe native vertex bounds,
   D3D7 caller RVA, source texture dimensions, color and depth for the
   player HUD, compass and instruction regions; they do not include
   copyrighted texture pixel data.
4. For a *separate* A/B test after backing up the original DLLs and INI,
   enable `HUDWidescreenFix=1` to evaluate the previously implemented
   player-HUD-only 16-site correction. Compare in 4:3 and widescreen.
   Restore `HUDWidescreenFix=0` if any unexpected behavior occurs.

**Second experimental step (after three completed Wine F12 captures):**
the actual gameplay 2D draw signatures are now known. Both mirrored
64×64 compass halves and the untextured direction pointer can be
identified by exact draw RVA, z/RHW layer, source texture and bottom-right
bounds. The two translucent instruction backgrounds and their text glyphs
have separate depths, colors and bottom-left bounds. Crucially, the F12
capture also discovered **3D particles wrongly labeled 'player' by the
loose sample**, with z≈0.9983 and RHW≈0.0229. The correction explicitly
rejects these and any unrecognized 3D calls.

This second stage is **opt-in with the existing
`[HUD] HUDWidescreenFix=1`**; the default remains `0`. The already
available 16-site upper-left player HUD prototype is enabled at the
same time, while the additional strict D3D7 filters adjust:
- compass halves plus pointer: scale X to 4:3-relative proportions and
  **anchor to the screen's right edge**;
- instruction panel and matching font glyphs: scale X to 4:3-relative
  proportions and **anchor to the left edge**.

At 1920×1080 the horizontal scale is 0.75. Y, Z, RHW, color, UV,
menu/comic/video geometry and the game's original vertex buffers are
unchanged; supported original 4:3 mode automatically disables the patch.

**Reference-frame correction (October 21, 2026):** Matching 4:3
screenshots confirm that the existing blue and red panel backgrounds are
already positioned correctly in 16:9; the blue box is x≈95–881
(relative to the 4:3 image) vs x≈93–880 in widescreen, and the same
red instruction box is x≈9–501 in both modes. Keep those transforms
unchanged. The instruction font uses TWO D3D7 passes, however: dark
shadow (`z=.0012, RHW=11.1764, ARGB FF000000`) followed by lavender
foreground (`z=.0010, RHW=12.6862, ARGB FFC0C0FF`). The initial
widescreen implementation resized only the dark pass, so light letters
extended past the corrected box. Both passes are now remapped together,
with strict screen-region, texture and depth signatures. This adjustment
has passed source-level regression checks but **still requires an in-game
screenshot before validation**.

**Boss HUD experimental correction (Venom, Scorpion and J. Jonah Jameson,
October 21, 2026):** Native Windows 11 reference screenshots (1920×1200
16:10 versus centered 1600×1200 4:3) revealed previously uncorrected,
~20%-stretched boss health bars. Two complete F12 trace series now
identify the shared drawing path for **single and double boss bars**:
colored fill (solid quad), 32×32 border tiles, 64×64 boss portrait,
and an extra black health marker on the second row (Jameson).
Their shared caller is `0x00102C72`, but it is *not* HUD-specific,
so the matcher also requires bounded upper-right coordinates, exact
texture size, layer/depth range, opaque color and plausible quad size.
RHW changes slightly between levels: the implementation uses measured
ranges rather than an inaccurate single numeric constant.

**Damaged boss health bar regression (Windows 11, 21 Oct, 23:05):**
The screenshot of injured Venom showed a detached black mask at
`x=1080..1204`, exactly the original unscaled bar starting coordinate,
while the frame, colored segments and portrait had shifted to the
right. The underlying empty-health black quad uses the boss's separate
`z≈.0007` layer (first measured as a 15px Jameson marker), but its
width varies with lost health. The initial matcher wrongly capped this
width to `4% of height`; the revised matcher accepts damaged-health
masks up to a complete bar's measured width, **only** for the opaque
black, textureless, expected depth/RHW, 40px-tall top-right boss rows.
Regression tests cover variable missing-health amounts and unrelated
black geometry. Visual confirmation of this change remains pending.

With `[HUD] HUDWidescreenFix=1` the boss sprites are experimentally
**aspect-corrected and anchored to the right edge**, just like the
already-validated compass. The same transform applies to both bars
and both portraits. 4:3, menu, media and original game-owned vertices
remain untouched. Source-level tests use the exact captured Venom
and Scorpion/Jameson draw signatures; negative tests reject unrelated
geometry. The HUD fix remains **opt-in, disabled by default**, pending
in-game Windows 11 and Wine validation; the pull request remains DRAFT.

**Next boss visual test** (no additional F12 probe or new 4:3
reference required): install the *two matching DLLs* from the latest
successful CI artifact of this draft branch, configure
`[Display] AspectRatio=auto`, `[HUD] HUDWidescreenFix=1`,
`[Debug] Log=1`, and launch Venom and Scorpion under native Windows.
Capture one 1920×1200 image of each fight, ideally with their full
life bars. Check that Scorpion and Jameson's two frames, health fills,
and both portraits stay aligned, with no clipped/squashed components.
`HUD-ADAPTIVE: region=boss` should appear in the log.
Do not set `SPIDERMAN2K1FIX_HUD_TRACE` during this visual test;
it is only a read-only development sampler. If there is any visual
regression, disable `HUDWidescreenFix`, preserve the log, and avoid
merging the PR.

**Wine visual A/B:** Back up the game DLLs and INI, install the DLLs from
this experimental PR *as a matching pair*, and leave `Log=1`.
First launch with `HUDWidescreenFix=0`, take a 16:9 3D screenshot;
then set `HUDWidescreenFix=1`, restart and take a screenshot of the same
scene, with player gauges, compass and instructions visible. Check
`HUD-ADAPTIVE: region=compass` and `region=instructions` in the log.
If anything crashes, flickers or shifts unexpectedly, revert to
`HUDWidescreenFix=0` and preserve the log. The F12 probe can remain
disabled during visual testing by omitting its environment variable.

This feature is **not yet approved for the 1.0 release**. It needs
visual/runtime validation under Wine and Windows 11 before it can be
enabled by default or merged into `main`.

## Roadmap

These are **planned improvements**, not features promised for version 1.0:

- **Fully adaptive HUD:** improve positioning and scaling of HUD elements across widescreen resolutions, beyond existing aspect-ratio protection.
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
