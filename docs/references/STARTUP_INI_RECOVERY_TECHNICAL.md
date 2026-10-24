# Startup synchronization, native display mode and INI recovery (PR #46)

**Scope:** SpiderMan2k1Fix (Windows x86 DLL + `winmm.dll` proxy), targeting the
unmodified supported **Spider-Man (2001) PC** executable. This is a technical
implementation and regression reference, not documentation of the unmodified
game. For the exact PE file, SHA-256, No-CD instructions and IAT guards, see
[DEVELOPMENT.md](../DEVELOPMENT.md).

**Validation date:** 2026-10-24. The user verified the malformed-INI startup,
native 1920×1080 borderless presentation and Alt+Tab focus pause/resume on
**Linux/Wine**. The GitHub Actions Windows x86 compilation and source-level
regression tests passed separately (**470 tests** at PR commit
`c9c9074c663088543964b3f6c8eb422dcf48342a`). This is **not**
evidence of equivalent native Windows 11 runtime behavior, multi-monitor
coverage or all original gameplay paths.

## 1. Why the original startup sequence was unsafe

Historically, the game asked DirectDraw for a **physical 640×480×16**
exclusive display mode. Our borderless fix acknowledged that request as
*logical* 640×480 while preserving the active monitor resolution. However,
an invalid `SpiderMan2k1Fix.ini` triggered a foreground/topmost
`MessageBoxA` while the fix's graphics interception was initialized on
another thread. The game could therefore reach DirectDraw before the patch
was ready; a user observed a 640×480 game window during the dialog.

There were **two distinct mitigations**, and both are needed:
1. Remove all modal MessageBox calls from automatic INI validation/repair.
2. Guarantee that the **first real** `DirectDrawCreateEx` cannot run ahead
   of the patch's one-time initialization.

The screenshots and runtime logs demonstrate the symptom and successful
mitigation under Wine. They do **not** isolate every race instruction or prove
the same behavior on all Windows/Wine graphics stacks.

## 2. Verified two-DLL boot sequence

```text
SpideyPC.exe imports local winmm.dll
   |
   +-- winmm_proxy.c: DllMain (DLL_PROCESS_ATTACH)
   |     +-- patch_early()                 [No-CD native opcode guard]
   |     +-- install_early_ddraw_gate()   [retail DirectDrawCreateEx IAT]
   |     +-- spawn loader worker          [no waiting under loader lock]
   |
   +-- game calls DirectDrawCreateEx
         |
         +-- winmm: proxy_first_directdraw()
         |     +-- load/locate SpiderMan2k1Fix.dll
         |     +-- resolve SpiderMan2k1Fix_DirectDrawGate
         |
         +-- fix: InitOnceExecuteOnce(core_start_once)
         |     +-- validate retail EXE SHA-256
         |     +-- ensure / validate / repair INI without MessageBox
         |     +-- install virtual Spidey.cfg + display, FOV, HUD, input,
         |         pause and media hooks (signature checked as applicable)
         |
         +-- fix: hooked_create()
               +-- determine game HWND and its monitor
               +-- probe_desktop_mode(game HWND)
               +-- SpiderCfg_BootstrapGameRead()
               +-- invoke original DirectDrawCreateEx safely
               +-- install windowed DirectDraw7 interface/vtable hooks
```

The **early IAT slot** is at executable image **RVA `0x13B010`**, checked
against the exact supported PE image and the named
`ddraw.dll!DirectDrawCreateEx` import. `winmm_proxy.c` installs the pointer
substitution only after its strict original No-CD opcode/PE guard succeeds.
No `LoadLibrary`, INI I/O, logging dialog, or blocking wait is performed by
this gate installer under `DllMain`'s loader lock.

The proxy's game-thread `proxy_first_directdraw()` obtains the main
DLL's exported `SpiderMan2k1Fix_DirectDrawGate` and synchronously invokes
the same `InitOnceExecuteOnce` used by its background initialization
worker. This avoids racing *two independent worker threads* against the
game thread. `build.sh` validates the exported symbol in the finished
x86 DLL before publishing the artifact.

**Fail closed:** If the main DLL cannot be loaded, the export is unavailable,
the supported executable is rejected or initialization fails, the gated
DirectDraw call is **not forwarded** into the unprotected original exclusive
display path. A failed launch is preferable to silently bypassing the native
mode fix; errors are diagnosed using the proxy log when available.

**Expected non-error diagnostic:** The main DLL's old
`install_ddraw_hook()` may still print
`DDRAW: IAT target mismatch; hook not installed`. This happens because
the early `winmm.dll` gate already owns that IAT slot: the old installer
expects a pointer to the unmodified `ddraw.dll` export. It is **not**
a failure if the new `DDRAW-STARTUP` confirmation, the DirectDraw7 hooks
and borderless presentation are subsequently observed. Do not infer success
from the mismatch line alone.

## 3. Resolution and aspect invariants

The physical output mode belongs to the user's monitor, **not** to the
original `640×480×16` request or any obsolete `Resolution` INI key.

- At the first DirectDraw creation, the patch looks for a valid game HWND
  via `GetActiveWindow`, `EnumThreadWindows` and a process-filtered
  `EnumWindows` fallback; it does not use another application's foreground
  window.
- `probe_desktop_mode(hwnd)` uses `MonitorFromWindow`,
  `GetMonitorInfoA` and
  `EnumDisplaySettingsA(info.szDevice, ENUM_CURRENT_SETTINGS)` to select
  the monitor's current physical dimensions and 32-bit output policy.
- `SpiderCfg_ObserveNativeMonitor` adjusts the virtual settings **before**
  the original game reads them via `SpiderCfg_BootstrapGameRead`. The
  intercepted `SetCooperativeLevel` confirms the actual game HWND and
  monitor.
- The renderer uses `DDSCL_NORMAL`, a monitor-sized `WS_POPUP` borderless
  window and a windowed primary/offscreen backbuffer. Original
  `SetDisplayMode(640,480,16,...)` is acknowledged without applying a
  physical mode switch.
- `AspectRatio=Auto` uses native-monitor physical output; `4:3`
  retains that same output but centers a **logical 4:3 game canvas**.
  On 1920×1080 the classic canvas is **1440×1080**, offset 240 pixels
  horizontally; menu/media assets remain 4:3 in both policies.

If the game HWND does not yet exist at the early hook, a primary-monitor
fallback is temporary until `SetCooperativeLevel` provides the actual
window. This is not yet a fully validated multi-screen/mixed-DPI solution.

## 4. Automatic INI validation and repair

The generated `SpiderMan2k1Fix.ini` presents only the player controls:
`Log`, `AspectRatio`, `Brightness`, **configurable `FPSLimit=30`**
(`0` disabled, `1..240`), `KeyboardLayout=Auto|Off`, audio volumes,
keyboard and joystick mappings, and required `[Internal] RawHex`.
No obsolete resolution, experimental HUD or pause toggles are generated.
The gameplay fixes, pause on focus loss, native input reacquire and media
synchronization are built in, with the native safety guards retained.

`PauseDimOpacity` is an **optional manual** `[Input]` setting:
**60** when absent, **0** disables *only* the black dim (not pause or
PAUSED banner), and **1..90** selects a custom opacity. The old
`PauseGrid` rendering and recognized INI option are removed.

Startup file handling in `virtual_spidey_cfg.c`:

1. If no INI exists, generate complete defaults.
2. If the INI is invalid, recover the **first valid occurrence** of known,
   supported fields. Reject unknown, malformed, duplicated/bad and retired
   entries. The lossless 252-byte original settings image is retained in
   `RawHex` when valid.
3. Write a *temporary* factory INI, overlay recovered values (including
   `FPSLimit`, bindings, audio, `PauseDimOpacity` when present and
   `RawHex`), then validate the whole temporary result **before**
   touching the old INI.
4. Rename the current INI to the **single rolling**
   `SpiderMan2k1Fix.ini.old`, replacing any existing .old, then install
   the validated temporary INI. This is a guarded *two-step rename*, not
   one indivisible transaction across both file names.
5. On installation failure, attempt to restore the original INI from
   `.old`; if restoration fails, log that the prior content remains at
   the backup path. Do not allow game-origin INI writes after a failed
   repair. After successful repair, prune only former
   `SpiderMan2k1Fix_<digits>.old` files from historical patch versions
   (permission errors may leave them behind).
6. An otherwise valid legacy INI containing deprecated switches is
   normalized through the same backup/recovery process. An INI that
   validates and contains no obsolete options is left untouched.

**No topmost `MessageBoxA` is shown** by automatic repair. Diagnostics are
written only if logging is active. Because new factory INIs have
`[Debug] Log=0`, an old `.log` file may remain unchanged on a later
launch; check the active setting before interpreting a stale log.

The backup is **rolling**, not an archive of every previous correction. An
already existing `.ini.old` is overwritten by the immediately previous
INI on each successful repair. Users who need long-term preservation should
copy it elsewhere. Manual deletion or modification of valid `RawHex` is
not supported.

## 5. Runtime evidence: Linux/Wine, 2026-10-24

The user observed correct startup after deliberately inserting an unknown
INI option and provided a fresh log showing the recovery and display path:

```text
DDRAW-STARTUP: early IAT gate status=1 (1=installed)
INI-REPAIR: fixed 8 error(s), 3 unknown/other warning(s); recovered 29 setting(s)
... original file saved as SpiderMan2k1Fix.ini.old
DDRAW-STARTUP: initial hook and configuration ready before first DirectDrawCreateEx
DISPLAY-NATIVE: pre-bootstrap game HWND identified; capturing its monitor
SPIDEY-CFG: BOOTSTRAP game loader returned: ... configured=1920x1080 depth=32 [READ CONFIRMED]
BORDERLESS: window=... style=WS_POPUP rect=(0,0)+1920x1080 cooperativity=WINDOWED
DISPLAY-MENU-SAFE: INTERCEPT 640x480x16 => physical 1920x1080x32
```

A **real** Alt+Tab/focus-loss cycle in that session additionally logged:

```text
FOCUS-PAUSE: confirmed external focus loss; pause latched until manual resume
FOCUS-MEDIA: Bink pause ... BinkPause-result=1
FOCUS-SURFACE: frozen scene+60% shade + native-ESC-inspired ... presented
FOCUS-PAUSE: Any-key resume swallowed virtual-key=0x20
FOCUS-MEDIA: Bink resume ... BinkPause-result=0
DI8-RETRY: redirected DIERR_NOTACQUIRED to game's Acquire: keyboard=1 mouse=1
```

The same session showed `FOCUS-DSOUND: pause slots=32 occupied=0`:
the hook ran, but **no playing secondary voice was present** to confirm a
nonzero stop/resume count in that particular capture.

**What this validates:** on the tested Wine configuration, malformed-INI
repair no longer blocked native-resolution borderless startup; virtual
configuration, movie pause/resume, manual key wake and DirectInput retry
executed. The observed repair retained **29 recognized settings**.

**Still unproven by this capture:** a *second* repair replacing a previously
existing `.ini.old`, verification of every recovered value after a restart,
native Windows 11 first-DirectDraw timing, arbitrary monitors/DPI, and every
3D cutscene/audio/gameplay scenario. Unit tests and x86 CI build are
additional static/build evidence, not substitutes for runtime tests.

## 6. Code and test index

| Function/file | Responsibility |
| --- | --- |
| `winmm_proxy.c::patch_early` | SHA-/opcode-guarded early game patch prerequisite |
| `winmm_proxy.c::install_early_ddraw_gate` | Install first DirectDrawCreateEx IAT gate on known retail layout |
| `winmm_proxy.c::proxy_first_directdraw` | Resolve and invoke main DLL's startup barrier |
| `spiderman2k1fix.c::SpiderMan2k1Fix_DirectDrawGate` | Join `InitOnceExecuteOnce`, reject failed startup |
| `spiderman2k1fix.c::hooked_create` / `probe_desktop_mode` | Discover window's monitor before virtual config read |
| `virtual_spidey_cfg.c::SpiderCfg_ValidateCurrentIni` / `SpiderCfg_RepairIni` | Validate, rebuild and preserve rolling old INI |
| `pause_surface.c`, `focus_media_pause.c`, `focus_dsound_pause.c` | Focus pause visual / Bink / secondary-voice components |
| `ChatGPT/tests/test_ini_repair_render_startup.py` | Static startup race and no-modal repair regression |
| `ChatGPT/tests/test_ini_auto_repair.py` | Recognized option recovery and one rolling backup |
| `ChatGPT/tests/test_native_monitor_early_priority.py` | Source-level selected-monitor priority and 4:3 policy |
| `ChatGPT/tests/test_ini_permanent_cleanup.py` | Player INI contract, default fixes, migration safety |

Related: [current configuration](CONFIGURATION.md),
[original EXE / system research](../DEVELOPMENT.md),
[focus-media test history](diagnostics/focus/FOCUS_AUDIO_VIDEO_SYNC_TEST.md),
[Windows graphics diagnosis](diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md).
