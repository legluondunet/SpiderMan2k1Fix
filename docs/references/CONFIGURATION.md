# Configuration and troubleshooting

This is the detailed companion to the [project README](../../README.md). Some settings, particularly input reacquisition and focus-pausing, are **experimental**. GitHub Actions passing does not establish that they work on every Windows, Wine or Proton setup.

## Upgrading from older builds

- **Updating the current fix:** back up the existing `winmm.dll`, `SpiderMan2k1Fix.dll` and `SpiderMan2k1Fix.ini` in the game folder. Replace **both DLLs from the same GitHub Actions build**, but keep your existing INI settings. The CI artifact does **not** include an INI.
- **Migrating from an older SpiderManFix beta:** remove the obsolete `SpiderManFix.dll`. To preserve settings, back up your old `SpiderManFix.ini` and rename it to `SpiderMan2k1Fix.ini` **before** launching the new fix (only if no new-named INI already exists). Otherwise the patch generates fresh defaults.
- **Other proxy mods:** `winmm.dll` is a filename shared by many mods. Back up an existing file and check for conflicts; two different proxies cannot normally occupy the same directory under that name.

## Player INI: compact configuration

SpiderMan2k1Fix automatically generates `SpiderMan2k1Fix.ini` beside the supported game executable. Edit it **only while the game is closed**. The generated file exposes only the options that a player should need:

```ini
[Debug]
Log=0

[Display]
AspectRatio=Auto
Brightness=4
FPSLimit=30

[Input]
KeyboardLayout=Auto

[Audio]
MusicSFXLevel=69
VoiceLevel=79
MovieLevel=69
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
Jump=BUTTON3
Kick=BUTTON4
Punch=BUTTON0
Web=BUTTON1
Aim=BUTTON6
Zipline=BUTTON9
Swing=BUTTON7

[Internal]
; 252 original configuration bytes, written automatically
RawHex=<504 hexadecimal characters>
```

The example `RawHex` line is **illustrative only**: the generated file contains a genuine 504-character value. Do not manually delete, replace or edit it: unrecognized original-game settings are still preserved there.

| Setting | Meaning |
| --- | --- |
| `[Debug] Log` | `0` disables the log, `1` writes `SpiderMan2k1Fix.log`. |
| `[Display] AspectRatio` | `Auto` uses the monitor's aspect ratio, `4:3` selects the classic presentation. |
| `[Display] Brightness` | Game brightness from 0 through 9. |
| `[Display] FPSLimit` | **Player-controlled frame limit.** Default `30`; `0` disables the limiter, `1..240` sets a target. Higher FPS can expose timing issues in the original game. |
| `[Input] KeyboardLayout` | `Auto` detects the Windows/Wine input layout, `Off` leaves original QWERTY positions. |
| `[Audio]` | Music/effects, voice and movie volumes (0..100%); `Stereo=1` selects stereo. |
| `[Keyboard]` / `[Joystick]` | The eleven original keyboard actions and seven joystick button bindings, also saved from the game's menus. |
| `[Internal] RawHex` | Lossless copy of the game's original 252-byte configuration record. Not a player-facing control. |

### Display resolution: native monitor first

**Physical output resolution is not an INI setting.** At launch, SpiderMan2k1Fix
uses the **current resolution of the monitor hosting the Spider-Man window**.
The game remains borderless (`WS_POPUP`, `DDSCL_NORMAL`), with a monitor-sized
window; no `ChangeDisplaySettings` or exclusive DirectDraw `SetDisplayMode`
call changes the desktop's actual display mode. The original game's
`640×480×16` intro/menu request is intercepted and acknowledged internally
rather than sent to the monitor.

On Windows 11 and Wine, the patch identifies the game's window on the
DirectDraw creation thread (`GetActiveWindow` /
`EnumThreadWindows`, with a same-process `EnumWindows` fallback), then queries
`MonitorFromWindow`, `GetMonitorInfoA` and
`EnumDisplaySettingsA(info.szDevice, ENUM_CURRENT_SETTINGS)`.
It supplies this monitor-specific width/height to
`SpiderCfg_ObserveNativeMonitor` **before**
`SpiderCfg_BootstrapGameRead` copies the virtual `Spidey.cfg` values into
the original engine. `SetCooperativeLevel` confirms the authoritative HWND
and can correct an early window/monitor mismatch before the first game mode.
If no window is discoverable early, the existing primary-monitor fallback is
used temporarily; its dimensions are corrected once the real HWND arrives,
though already-loaded original game state cannot always be retroactively
reloaded. This edge case is a useful regression test on multi-display systems.

`AspectRatio=Auto` fills the selected monitor for 3D gameplay with
automatic horizontal FOV/aspect adjustment. `AspectRatio=4:3` keeps
the **same native physical output** but centers a logical 4:3 viewport
with pillarboxing; menus and source videos always keep their intended 4:3
presentation. Neither mode changes the Windows/Wine screen resolution.

Turn on `[Debug] Log=1` to verify `DISPLAY-NATIVE`,
`DISPLAY-PROBE`, `DISPLAY-POLICY` and `BORDERLESS` records.
In particular, the reported monitor resolution and
`BORDERLESS: window ... +WIDTHxHEIGHT` should agree with the
selected monitor's active Windows/Wine resolution.

### Built-in fixes: no INI switches

The supported executable receives **automatic desktop resolution**, borderless fullscreen, centered 4:3 intro/menu media, native comic correction, automatic FOV/aspect adjustment, adaptive widescreen HUD and the verified in-game Display menu lock. These are no longer enabled or disabled through experimental flags; the original 4:3 mode retains its original geometry where a widescreen transform is unnecessary.

**Alt+Tab and pause are permanent features:** once another application's focus loss is confirmed, the game latches pause at the original DirectDraw Flip boundary; a **press and release of any keyboard key after returning** resumes it. The frozen game frame receives the native violet/red PAUSED banner and **60% black shading by default**, with no grid. This behavior also coordinates the game's native Bink video/music streams and its game-owned secondary DirectSound voices. The native `DIERR_NOTACQUIRED` keyboard/mouse retry is requested automatically and installed only after its verified signature check at Flip #120; desktop cursor restoration is automatic. The old unsafe DirectInput COM-vtable hook stays disabled.

**Windows-specific behavior:** the patch requests `DisableProcessWindowsGhosting` automatically on systems providing the API, avoiding Windows' false Ghost-window foreground transitions. This also disables Windows' normal ghost-window handling for a genuinely hung game **until the process exits**; it does not solve a real hang. Wine implementation details may differ.

In all cases, the original executable's SHA-256 check and per-feature native signature checks remain active. A failed optional hook is logged and skipped instead of attempting an unchecked patch. Experimental HorPlus/AnySize modes remain disabled, not silently promoted to permanent behavior.

### Optional PAUSED background opacity

The focus-loss pause and its PAUSED banner are permanent, but **background
darkening may be adjusted independently**. The generated INI intentionally
does **not** contain `PauseDimOpacity`. When omitted, it uses **60%**.

To disable only the background shade (not the PAUSED banner, game pause,
input wake-up, Bink or DirectSound synchronization), manually add:

```ini
[Input]
PauseDimOpacity=0
```

For a different shade, set `PauseDimOpacity` to any integer from `1` to
`90`; for example, `PauseDimOpacity=75`. To restore the built-in default,
remove the line (or set it to `60`). The value is applied when the game
starts. Values outside `0..90` are rejected by the strict INI validator.

The former `PauseGrid` setting and all grid-rendering code in the active
DirectDraw pause path are removed. Old INIs containing this key are migrated
with their other player settings preserved, including an explicit
`PauseDimOpacity` value if previously configured.

### Upgrading existing INIs

An existing valid INI containing retired display, HUD, pause or focus switches is **automatically migrated at startup**: the old INI is renamed to the single rolling backup `SpiderMan2k1Fix.ini.old` (replacing any earlier `.ini.old`), and a new compact INI is installed after recovery and validation. This carries over **FPSLimit, AspectRatio, Brightness, KeyboardLayout, audio volumes, keyboard/joystick mappings and `RawHex`**. Deprecated flags are removed, not merely hidden while still changing the game. Other recognized developer-only F10 settings remain available through manual configuration but are not generated by default. If migration fails, the original file stays untouched and game-origin INI saving is disabled for safety.

## Keyboard layout (Auto or Off)

The original *Spider-Man* 2001 reads **physical DirectInput scan codes**, whereas its CHEATS character-entry and keyboard configuration menu use a separate fixed US QWERTY key-name table. This is why a French AZERTY M originally behaves like US semicolon, while the French comma enters M.

There are exactly **two user-facing modes**:

```ini
[Input]
KeyboardLayout=Auto
```

| Value | Behavior |
| --- | --- |
| `Auto` (default for new INIs) | Read the active Windows/Wine input layout using `GetKeyboardLayout` and `MapVirtualKeyEx`. Build a complete, unique A-Z physical scan-code map. Use it for the 11 INI keyboard actions and the original game's CHEATS and keyboard-menu key names. |
| `Off` | No translation. Preserve the original game's US QWERTY key names and physical DirectInput behavior. |

**Automatic detection is based on the active system layout, not the keyboard's manufacturer or engraving.** If Windows/Wine misreports that layout, an application cannot know the physical keyboard's printed letters reliably. An incomplete/ambiguous letter mapping falls back to original physical codes instead of partially remapping controls. Layout is sampled during game startup; restart the game after switching your operating-system keyboard layout.

For compatibility, existing INIs **without** `KeyboardLayout` default to `Off`, preserving saved physical control names. The deprecated values `AZERTY` (migrated to `Auto`) and `QWERTY`/`Physical`/`Legacy` (migrated to `Off`) are accepted only to upgrade older configuration files automatically. No other modes are generated or documented as current options.

```ini
[Keyboard]
Kick=M
```

With `Auto` and a French layout, `M` resolves to physical DirectInput `0x27` rather than QWERTY `0x32`, and `A/Q` and `Z/W` also resolve to their printed physical positions. Named non-letter controls and explicitly entered hexadecimal `0xNN` bindings retain physical semantics. Joystick bindings are unchanged.

**CHEATS and keyboard-menu label correction (validated on Windows 11 and Linux/Wine with French AZERTY):** For the supported SHA-verified 2001 executable, the fix verifies the original `Shell_InputName` → `DXINPUT_GetKeyName` call path and its QWERTY name table. In `Auto`, a valid non-QWERTY layout remaps the native name pointers in **process memory**, reusing original game strings. Displaced US letter positions receive the corresponding punctuation name where possible, otherwise `KEY`, rather than a duplicate CHEATS letter. The game's DirectInput event stream and executable on disk remain unchanged. Detection requires the **active system keyboard layout** to be accurately reported; not every locale or punctuation mapping has been runtime-tested. See [the detailed technical implementation, Windows API calls, verified addresses and failure guards](KEYBOARD_LAYOUT_TECHNICAL.md).

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

## Startup safety: no modal INI dialogs before native DirectDraw

An invalid INI must **never** open a topmost `MessageBoxA` while the
original game starts its graphics device. Repair and migration now write
diagnostics only; game startup continues without a modal interruption, even
if the original INI cannot be replaced. If repair fails, the original INI
remains intact and game-origin INI writes are disabled.

To eliminate the initialization race between the game's primary thread and
the patch's background loader threads, the initial `winmm.dll` proxy
installs a narrowly scoped **first-DirectDrawCreateEx gate** in the
SHA-/opcode-guarded supported game's original IAT, at RVA `0x13B010`.
Only minimal memory checks and the IAT write run in the loader callback.
When the game makes its first `DirectDrawCreateEx` call **outside the
loader lock**, the gate locates the main patch DLL and calls
`SpiderMan2k1Fix_DirectDrawGate`. This invokes or joins one-time
`InitOnceExecuteOnce` initialization: virtual config and the DirectDraw
interception must be ready **before** the real DirectDraw device is
created. If the DLL is absent or unsupported, the guarded call fails
rather than forwarding the original game's unsafe exclusive 640×480
startup request.

This is separate from native-monitor detection: after initializing the
patch, the game-thread hook identifies the actual game HWND/monitor before
reading virtual `Spidey.cfg`, preserving the OS monitor mode and 4:3
letter/pillarbox policies. A broken INI does not reinstate a physical
`Resolution` setting or bypass the borderless requirement.

**Diagnostic identifiers:** `DDRAW-STARTUP: early IAT gate status=1` in
the proxy, `DDRAW-STARTUP: initial hook and configuration ready ...` in
the main DLL, plus `INI-REPAIR` and `DISPLAY-NATIVE` messages. Note
that `[Debug] Log=1` is needed for log output and may itself be absent
or corrupted; the absence of log lines alone is not proof of failure.

**Runtime verification is still required on Windows 11 and Linux/Wine:**
deliberately add an unknown INI entry, launch the game, confirm no modal
dialog and a monitor-sized borderless window from the first visible frame,
then verify gameplay 3D, 4:3 menus/video and Alt+Tab. Static tests and CI
compilation do not establish all native DirectDraw timings or display
backend behaviors.

## Automatic INI recovery

When `SpiderMan2k1Fix.ini` does not exist, the patch generates a new one using its defaults. When it is **corrupted or invalid** (malformed lines, unknown sections/options, bad values or duplicates), it silently recovers all recognized **valid** settings using a safe rebuild, without showing a startup dialog:

1. Parse the existing `SpiderMan2k1Fix.ini` and keep the **first valid** occurrence of each supported key. Discard malformed, unknown and retired entries.
2. Create a temporary INI from factory defaults, apply recovered values (controls, joystick, volumes, FPS, aspect, brightness, optional pause opacity and valid `RawHex`), then validate the **entire** replacement.
3. Only after validation, **rename** the current `SpiderMan2k1Fix.ini` to `SpiderMan2k1Fix.ini.old`. If that backup already exists, **replace it**. There is no backup counter and no series of `_0.old`, `_1.old`, etc.
4. Install the validated temporary file as the new `SpiderMan2k1Fix.ini`. If installation fails, attempt to restore the original file from `.ini.old`; when restoration is impossible, the original remains in the backup and an error is logged.
5. After a successful repair, delete older **numbered backups created by previous patch versions** (matching `SpiderMan2k1Fix_<number>.old` only). Unrelated user files are not touched. Files that Windows/Wine refuses to delete are reported in the log.

The same process compacts an otherwise valid legacy INI that still contains retired settings. **At most one rolling `.ini.old` is normally retained**; a subsequent repair replaces it with the immediately preceding configuration, so this backup is not a permanent history. Copy it elsewhere if you wish to preserve it long-term.

Unrecognized data stays in the previous version stored in `.ini.old`, but is not copied to the clean file. An invalid `[Internal] RawHex` is replaced by a valid factory image; a valid `RawHex` is preserved. **Never manually edit the 504-character RawHex value.**

## Troubleshooting cases

**The game launches, but SpiderMan2k1Fix does not seem active**

- Verify that both DLLs are in the same directory as `SpideyPC.exe`.
- On Linux, check the Wine DLL override: `WINEDLLOVERRIDES="winmm=n,b"`.
- Check that the executable is the supported version. On Linux, run `sha256sum SpideyPC.exe`; on Windows PowerShell, run `Get-FileHash .\SpideyPC.exe -Algorithm SHA256`.
- Temporarily enable `Log=1` and check `SpiderMan2k1Fix.log`, if an INI was generated.

**The INI seems to have reset itself**

Look for `SpiderMan2k1Fix.ini.old` (the single rolling backup). The previous configuration may have contained unknown or malformed entries; known valid settings should be recovered in the replacement file.

**Controls are not responding after switching applications**

When testing the current development build, set `DirectInputReacquire=1` and `PauseOnFocusLoss=1`. Any keyboard key now resumes after focus returns; the old `PauseKey` selection is ignored. If recovery fails, enable `[Debug] Log=1`, then check for `DI8-RETRY: EXPERIMENTAL requested source=INI` and `DI8-RETRY: EXPERIMENTAL active at Flip #120`. For synchronized sound, enable both additional media-pause settings described above. A previous report of intermittent audio crackling occurred under ProcDump but was not reproduced during ordinary launches without ProcDump; its cause remains unknown. Future builds may provide a different option set.

**The game has visual or audio glitches**

Try the original `AspectRatio=4:3` mode to help isolate widescreen-specific problems. Some edge-of-screen 3D rendering issues and audio artifacts are still being investigated; see the roadmap below.

## Further technical references

- [Optional F10 shortcut and validation](F10_VALIDATION_MATRIX.md), disabled by default.
- [Native ESC 4:3 corrections and debug switches](NATIVE_PAUSE_MENU_43.md).
- [F9 PNG screenshot capture](F9_PNG_CAPTURE.md).
- [HUD scaling and original game implementation](../DEVELOPMENT.md#7-display-and-rendering-research).
