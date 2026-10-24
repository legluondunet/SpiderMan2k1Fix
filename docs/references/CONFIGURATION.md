# Configuration and troubleshooting

This is the detailed companion to the [project README](../../README.md). Some settings, particularly input reacquisition and focus-pausing, are **experimental**. GitHub Actions passing does not establish that they work on every Windows, Wine or Proton setup.

## Upgrading from older builds

- **Updating the current fix:** back up the existing `winmm.dll`, `SpiderMan2k1Fix.dll` and `SpiderMan2k1Fix.ini` in the game folder. Replace **both DLLs from the same GitHub Actions build**, but keep your existing INI settings. The CI artifact does **not** include an INI.
- **Migrating from an older SpiderManFix beta:** remove the obsolete `SpiderManFix.dll`. To preserve settings, back up your old `SpiderManFix.ini` and rename it to `SpiderMan2k1Fix.ini` **before** launching the new fix (only if no new-named INI already exists). Otherwise the patch generates fresh defaults.
- **Other proxy mods:** `winmm.dll` is a filename shared by many mods. Back up an existing file and check for conflicts; two different proxies cannot normally occupy the same directory under that name.

## Full INI example, display and input settings

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
; Letter naming: Auto for new INIs, AZERTY, QWERTY or Physical (legacy).
KeyboardLayout=Auto
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

## Keyboard layout (AZERTY, QWERTY and Wine)

The original *Spider-Man* 2001 game reads **physical DirectInput DIK scan codes**. Those scan codes are based on keyboard position, **not on the letters printed on your keyboard**. This is why on an AZERTY keyboard a physical **M** may appear as **;** in the original game's key-binding screen, and **Z/W** or **A/Q** can look swapped.

SpiderMan2k1Fix adds a layout option for **letter names in the INI**, while retaining the original physical codes used by the game:

```ini
[Input]
KeyboardLayout=AZERTY

[Keyboard]
Kick=M
```

Available values:

| `[Input] KeyboardLayout` | Meaning |
| --- | --- |
| `Auto` | For **newly generated INIs**: use the active Windows/Wine keyboard layout reported by `GetKeyboardLayout` and `MapVirtualKeyEx`. This is the new default for first-time installs. |
| `AZERTY` | Force French AZERTY letter positions, including **A/Q, Z/W and M/;**, even when Wine does not report the expected layout. |
| `QWERTY` | Force the standard US/QWERTY physical mapping of letter names. |
| `Physical` | Keep the **historical positional meaning** of the project's original INI key names (also equivalent to QWERTY). This is the default when `KeyboardLayout` is **absent** from an existing INI, avoiding silent changes to older saved bindings. |

For example, `[Keyboard] Kick=M` becomes **DIK scan `0x27`** with `AZERTY`, versus **`0x32`** with `Physical`/QWERTY. AZERTY also maps `Z` to `0x11` (original `DIK_W`) and `W` to `0x2C` (original `DIK_Z`). The 11 action bindings remain stored as physical scan codes in the virtual 252-byte configuration; joystick controls are independent.

**Existing users:** add `KeyboardLayout=AZERTY` under the existing `[Input]` section while the game is closed, then restart it. Do not add a second `[Input]` section; the INI validator rejects duplicates. If automatic detection is incorrect under Wine/Proton, prefer explicit `AZERTY`. The layout is sampled on startup; changing the system layout while the game is running requires restarting the game.

**Important limitation:** this corrects the **INI names and gameplay action mapping**, but **not yet the key names drawn by the original game's configuration menu**. Its QWERTY legend can still show `;` when you press the physical AZERTY `M`; correcting that requires separate identification and safe patching of the game's original label-rendering code. This feature also does not rewrite text-entry/character input, alter DirectInput's events, or modify keyboard state globally. Named non-letter keys (`SPACE`, arrows, function keys) and explicit hexadecimal raw codes `0xNN` retain their original physical meaning.

## Optional Windows focus diagnostics

**Windows 11 class=Ghost false focus-loss experiment (PR #11):** If the game repeatedly pauses without Alt+Tab after about five to six seconds, capture `SpiderMan2k1Fix.log`: repeated `FOCUS-PAUSE-OWNER: ... class=Ghost` lines indicate Windows is replacing an apparently unresponsive game window with a ghost surrogate. `[Input] DisableWindowsGhosting=1` (default **0**) requests Microsoft's `DisableProcessWindowsGhosting` API at startup to prevent that replacement and test whether the repeated false pauses stop. **Restart required; it applies to the whole game process until exit and suppresses Windows' normal ability to manage a hung application via a ghost window. It does not fix a truly hung game or change the Alt+Tab pause/input behavior.** Do not enable by default before native Windows testing. See [diagnostic procedure](diagnostics/focus/UNEXPECTED_FOCUS_PAUSE_DIAGNOSTIC.md).

For obsolete designs, see the [historical focus-pause UI archive](PAUSE_UI_HISTORY.md). **Those older prototypes are not configuration instructions for the current build.**

## Bink music and DirectSound focus pausing

**Synchronized audio and video focus pause (PR #9):** The separately enabled `PauseBinkOnFocusLoss=1` pauses Bink cinematic streams **and the Bink-based music heard during 3D gameplay** using the game's original `BinkPause` API. `PauseDirectSoundOnFocusLoss=1` pauses currently playing game-owned secondary DirectSound buffers and resumes the same buffers from their retained cursor position. The primary buffer is untouched. Both new media options are **disabled by default**. Their combined operation was tested in Windows 11 gameplay, the menu, and cinematics with no audio desynchronization. See [technical notes and test results](diagnostics/focus/FOCUS_AUDIO_VIDEO_SYNC_TEST.md) for diagnostics, compatibility limits, and rollback.

**Audio:** the three volume levels use user-friendly percentages; `Stereo=1` enables stereo and `Stereo=0` selects mono.

## Debug logging

Debug logging is **off by default**:

```ini
[Debug]
Log=0
```

If you need to investigate a problem, set `Log=1`, save the file, and **restart the game**. SpiderMan2k1Fix will write `SpiderMan2k1Fix.log` in the game directory. **Each game launch starts a fresh log:** the previous session is overwritten (the proxy startup lines and main DLL diagnostics are retained together). Copy or rename the log before your next launch if you need to keep it. After collecting the information you need, return to `Log=0`.

## Automatic INI recovery

If the configuration is missing, SpiderMan2k1Fix generates a new one. If it detects malformed lines, unknown sections/options, invalid values, or duplicates, it can:

1. Rename the previous file to the next unused backup name, such as `SpiderMan2k1Fix_0.old`, `SpiderMan2k1Fix_1.old`, and so on.
2. Generate a clean `SpiderMan2k1Fix.ini` using the recognized sections and defaults.
3. Recover known, valid values from the old configuration. When duplicate values exist, the **first valid** one is kept.

Unknown settings are retained in the `.old` backup rather than being copied into the clean INI. The `[Internal]` section contains `RawHex`, the game's 252-byte internal settings image; **do not edit this value manually**.

## Troubleshooting cases

**The game launches, but SpiderMan2k1Fix does not seem active**

- Verify that both DLLs are in the same directory as `SpideyPC.exe`.
- On Linux, check the Wine DLL override: `WINEDLLOVERRIDES="winmm=n,b"`.
- Check that the executable is the supported version. On Linux, run `sha256sum SpideyPC.exe`; on Windows PowerShell, run `Get-FileHash .\SpideyPC.exe -Algorithm SHA256`.
- Temporarily enable `Log=1` and check `SpiderMan2k1Fix.log`, if an INI was generated.

**The INI seems to have reset itself**

Look for a numbered `SpiderMan2k1Fix_N.old` backup. The previous configuration may have contained unknown or malformed entries; known valid settings should be recovered in the replacement file.

**Controls are not responding after switching applications**

When testing the current development build, set `DirectInputReacquire=1` and `PauseOnFocusLoss=1`. Any keyboard key now resumes after focus returns; the old `PauseKey` selection is ignored. If recovery fails, enable `[Debug] Log=1`, then check for `DI8-RETRY: EXPERIMENTAL requested source=INI` and `DI8-RETRY: EXPERIMENTAL active at Flip #120`. For synchronized sound, enable both additional media-pause settings described above. A previous report of intermittent audio crackling occurred under ProcDump but was not reproduced during ordinary launches without ProcDump; its cause remains unknown. Future builds may provide a different option set.

**The game has visual or audio glitches**

Try the original `AspectRatio=4:3` mode to help isolate widescreen-specific problems. Some edge-of-screen 3D rendering issues and audio artifacts are still being investigated; see the roadmap below.

## Further technical references

- [Optional F10 shortcut and validation](F10_VALIDATION_MATRIX.md), disabled by default.
- [Native ESC 4:3 corrections and debug switches](NATIVE_PAUSE_MENU_43.md).
- [F9 PNG screenshot capture](F9_PNG_CAPTURE.md).
- [HUD scaling and original game implementation](../DEVELOPMENT.md#7-display-and-rendering-research).
