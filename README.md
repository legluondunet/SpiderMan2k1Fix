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
; Strength of black shade: 0..90 percent (default 55).
PauseDimOpacity=55
; Draw a fine, subtle 16-pixel grid over the shade (0 disables grid).
PauseGrid=1
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

**Wine/Linux focus-only pause visuals (PR #15):** The game and enabled Bink/DirectSound streams pause as soon as actual loss of focus is confirmed, but the PAUSE panel and its game-owned translucent black/grid popups are **not shown while another application owns the foreground**. They appear only when Spider-Man is foreground again and disappear if it loses focus a second time while paused; this avoids drawing a floating red border on top of Linux desktop applications. Inside the banner, all 16 px grid strokes are clipped out and the additional darkened center remains translucent; the 25 px bold upright `PRESS ANY KEY TO RESUME` hint is protected by expanded shaped-glyph masks. Retains the sole consumed Any-key resume action, without F9/Space key modes. **Note:** requires real Windows 11 and Wine regression testing before release, since GDI layering and popup stacking are compositor-dependent.\n\n**Dimmed pause background (PR #12):** When `[Input] PauseOnFocusLoss=1`, a non-topmost, nonactivating layered **popup owned by the game window** darkens the paused scene with a **55% black tint** and a fine **16-pixel square grid**. The original red-framed comic-style `PAUSE` message stays centered and bright: the popup is cut out around the exact border and white glyphs of the existing shaped banner. The effect appears only during a latched focus pause, and both windows are destroyed before video/audio resumes. `[Input] PauseDim=1` enables the shade (default on, although the underlying focus pause remains disabled by default). `PauseDimOpacity=55` adjusts darkness from `0` to `90` percent. `PauseGrid=1` enables the fine square pattern, and `PauseGrid=0` keeps a smooth dark tint. Set `PauseDim=0` to restore the previous validated plain paused image. The grid is painted inside the original nonactivating alpha popup. PR #14 can add a second small **game-owned, nonactivating popup** for the darker center only; neither changes DirectDraw, Bink, DirectSound or DirectInput. The popup avoids the original 2001 EXE's layered-child compatibility restriction and is not desktop-topmost. Verify the final look and Z-order on native Windows 11; automated CI cannot validate the compositor.

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
