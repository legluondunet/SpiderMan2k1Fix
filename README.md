# SpiderManFix

**A modern compatibility and widescreen fix for Spider-Man (2001) on PC — Windows and Linux (Wine).**

SpiderManFix is an **unofficial, fan-made patch** for the original Windows PC release of *Spider-Man*. It helps the game run more comfortably on modern displays and operating systems while preserving its original look when desired.

It adds borderless fullscreen, desktop-resolution output, widescreen support, corrected presentation for introductory movies and comic-book artwork, improved input handling, and an integrated configuration file. It also removes the game's CD check for the supported executable.

SpiderManFix is **not a remake or a replacement for the game**. You must provide your own installed copy of *Spider-Man*. No game assets are distributed with this project.

<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/4edede76-4596-40ad-bb73-4a72386add47" />


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
- **Integrated game settings:** stores display, audio, keyboard and joystick bindings in **`SpiderManFix.ini`**, replacing the need to maintain a separate `Spidey.cfg` for supported configurations.
- **Automatic INI creation and repair:** generates the INI on first launch and can rebuild malformed configurations while preserving recoverable settings and making numbered backups.
- **Optional debug logging:** writes `SpiderManFix.log` when enabled; **disabled by default** to avoid unnecessary logging overhead.
- **30 FPS default limit:** retains the game's intended timing, with a configurable limiter.

[![Windows x86 build](https://github.com/legluondunet/SpiderManFix/actions/workflows/windows-x86.yml/badge.svg)](https://github.com/legluondunet/SpiderManFix/actions/workflows/windows-x86.yml)

## Requirements and compatibility

- An installed copy of the **original Windows PC version of Spider-Man**, including its game data.
- The supported **32-bit** `SpideyPC.exe` executable.
- Windows, or Linux with a working **32-bit-capable Wine** installation.
- Two SpiderManFix DLLs: `winmm.dll` and `SpiderManFix.dll`.

SpiderManFix currently checks the game's executable before applying its main patches. The supported `SpideyPC.exe` has this SHA-256 checksum:

```text
0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
```

Other executable versions, modified executables, and some third-party wrappers may not be compatible. Do not rename an unsupported executable or bypass the compatibility check.

### Installing on Windows

1. Install *Spider-Man* normally and locate the directory containing `SpideyPC.exe`.
2. Back up any existing `winmm.dll`, `SpiderManFix.dll`, or `SpiderManFix.ini` files in that directory.
3. Get the **two compiled DLLs** from a SpiderManFix release package once one is published. (The source archive alone is not a compiled release.)
4. Copy `winmm.dll` and `SpiderManFix.dll` into the **same directory** as `SpideyPC.exe`:

   ```text
   Spider-Man/
   ├── SpideyPC.exe
   ├── winmm.dll
   ├── SpiderManFix.dll
   └── SpiderManFix.ini    # generated automatically on first launch
   ```

5. Start the game using `SpideyPC.exe`.

On the first supported launch, SpiderManFix creates `SpiderManFix.ini` automatically if it does not already exist. You do not need to create it manually.

**Important:** Do not overwrite another mod's `winmm.dll` without backing it up first. Two different `winmm.dll` proxy mods cannot normally occupy the same location under the same filename.

### Installing on Linux with Wine

1. Install *Spider-Man* in a Wine prefix and locate the directory containing `SpideyPC.exe`.
2. Copy the **same two Windows DLLs** (`winmm.dll` and `SpiderManFix.dll`) next to `SpideyPC.exe`.
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

4. Launch the game. On a supported executable, SpiderManFix initializes and creates `SpiderManFix.ini` if necessary.

**Wine override explained:** `winmm=n,b` means **native, then built-in**. This allows Wine to load SpiderManFix's local `winmm.dll` proxy first. The proxy forwards the original multimedia functions while loading `SpiderManFix.dll`.

If you use Lutris or another Wine launcher, add a DLL override with **Name:** `winmm` and **Value:** `n,b` in that game's Wine configuration (or set the equivalent environment variable). You generally do not need a separate override for `SpiderManFix.dll`.

## Using SpiderManFix

There is **no separate launcher or graphical configuration utility**. Start *Spider-Man* as usual and change its settings through the in-game menus or `SpiderManFix.ini`.

### Configuration file

SpiderManFix creates `SpiderManFix.ini` alongside the game executable. **Close the game before editing the file manually**, because in-game settings may be saved back to it.

The main options include:

```ini
[Debug]
; 0 = disabled (default), 1 = write SpiderManFix.log
Log=0

[Display]
; Auto = desktop monitor resolution
Resolution=Auto
; Auto or 4:3 for a native gaming experience
AspectRatio=Auto
Brightness=4
FPSLimit=30

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

**Audio:** the three volume levels use user-friendly percentages; `Stereo=1` enables stereo and `Stereo=0` selects mono.

### Debug log

Debug logging is **off by default**:

```ini
[Debug]
Log=0
```

If you need to investigate a problem, set `Log=1`, save the file, and **restart the game**. SpiderManFix will write `SpiderManFix.log` in the game directory. After collecting the information you need, return to `Log=0`.

### Automatic INI recovery

If the configuration is missing, SpiderManFix generates a new one. If it detects malformed lines, unknown sections/options, invalid values, or duplicates, it can:

1. Rename the previous file to the next unused backup name, such as `SpiderManFix_0.old`, `SpiderManFix_1.old`, and so on.
2. Generate a clean `SpiderManFix.ini` using the recognized sections and defaults.
3. Recover known, valid values from the old configuration. When duplicate values exist, the **first valid** one is kept.

Unknown settings are retained in the `.old` backup rather than being copied into the clean INI. The `[Internal]` section contains `RawHex`, the game's 252-byte internal settings image; **do not edit this value manually**.

## Troubleshooting

**The game launches, but SpiderManFix does not seem active**

- Verify that both DLLs are in the same directory as `SpideyPC.exe`.
- On Linux, check the Wine DLL override: `WINEDLLOVERRIDES="winmm=n,b"`.
- Check that the executable is the supported version. On Linux, run `sha256sum SpideyPC.exe`; on Windows PowerShell, run `Get-FileHash .\SpideyPC.exe -Algorithm SHA256`.
- Temporarily enable `Log=1` and check `SpiderManFix.log`, if an INI was generated.

**The INI seems to have reset itself**

Look for a numbered `SpiderManFix_N.old` backup. The previous configuration may have contained unknown or malformed entries; known valid settings should be recovered in the replacement file.

**Controls are not responding after switching applications**

Make sure you are using the latest test build for your installation. Input reacquisition is intended to restore control when returning from Alt+Tab. If the problem persists, enable the debug log and include the reproduction steps in a bug report.

**The game has visual or audio glitches**

Try the original `AspectRatio=4:3` mode to help isolate widescreen-specific problems. Some edge-of-screen 3D rendering issues and audio artifacts are still being investigated; see the roadmap below.

## Roadmap

These are **planned improvements**, not features promised for version 1.0:

- **Fully adaptive HUD:** improve positioning and scaling of HUD elements across widescreen resolutions, beyond existing aspect-ratio protection.
- **Audio improvements:** reduce or eliminate unwanted audio noise and artifacts.
- **Widescreen 3D geometry:** investigate 3D objects that appear displaced near the left or right edges of the screen in landscape/widescreen mode.
- **Electro:** investigate and fix the outstanding Electro-related issue.

## Building from source

GitHub Actions also creates a ZIP containing the two DLLs and configuration files. Open **Actions → Windows x86 DLLs → latest successful run → Artifacts** to download it.

The source package includes scripts for building the 32-bit Windows DLLs on Linux with an **MSVCRT-targeting MinGW-w64 toolchain**:

```bash
./build.sh
```

If your system compiler targets UCRT instead, consult `build-native-msvcrt.sh` in the source tree. The build scripts check the 32-bit Windows architecture and expected imports/exports. The resulting `winmm.dll` and `SpiderManFix.dll` are Windows DLLs even when built on Linux.

## Project and contributions

SpiderManFix is an independent compatibility project maintained by **[legluondunet](https://github.com/legluondunet)**.

Bug reports and reproducible test results are welcome, especially for different Windows versions, Wine versions, GPUs, resolutions and controller models. When reporting an issue, include your platform, game executable checksum, relevant INI settings, and a debug log when available.

**Project repository:** <https://github.com/legluondunet/SpiderManFix>

*Spider-Man and all associated trademarks belong to their respective owners. This project is unofficial and is not affiliated with or endorsed by the game's original developers or publishers.*
