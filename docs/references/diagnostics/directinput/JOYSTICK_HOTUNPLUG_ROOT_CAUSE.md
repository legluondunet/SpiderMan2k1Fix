# Native joystick unplug termination — SHA-verified root cause (2026-10-28)

**Status:** offline x86 disassembly completed against the user-supplied
**unchanged retail `SpideyPC.exe`**, size **1,507,328 bytes**, SHA-256
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.
No game/EXE memory patch was applied by this investigation. All VAs below
are for the original image base **`0x00400000`**.

## Critical correction to earlier evidence

Wine's user-supplied exit relay logged

```text
0160:Call KERNEL32.ExitProcess(80040209) ret=0052a1c6
```

**`0x80040209` is `DIERR_UNPLUGGED` in DirectInput's `dinput.h`.**
The value is also reused as DirectShow's `VFW_E_NOT_CONNECTED`; this
collision previously made attribution ambiguous. The device-detach timing,
original DirectInput joystick poll/reacquire callsites and *the original
EXE's embedded `DIERR_UNPLUGGED` error string* establish DirectInput as
the relevant interpretation. `0x80040209` does **not** occur as a
literal DWORD in the original binary: it is dynamically returned by an API.

Source definitions:
- https://chromium.googlesource.com/chromium/deps/perl/+/master/c/i686-w64-mingw32/include/dinput.h
- https://sources.debian.org/src/wine/1.0.1-1/include/dinput.h
- https://www.winehq.org/pipermail/wine-devel/2022-January/204481.html

## The precise native joystick failure path

The original joystick interface pointer is stored at **`0x006B7A2C`**.
The native polling function begins at **`0x00501E50`**.

| VA | Exact original bytes | Explanation |
| --- | --- | --- |
| `0x00501E79` | `FF 51 64` | COM `IDirectInputDevice8::Poll` (vtable +0x64) |
| `0x00501E7C` | `3D 1E 00 07 80` | Compare with `DIERR_INPUTLOST=0x8007001E` |
| `0x00501E83` | `3D 0C 00 07 80` | Compare with `DIERR_NOTACQUIRED=0x8007000C` |
| `0x00501E9F` | `8B 10 FF 52 1C` | COM `IDirectInputDevice8::Acquire` (vtable +0x1c) |
| `0x00501EA6` | `81 FE 05 00 07 80` | `E_ACCESSDENIED`, alias `DIERR_OTHERAPPHASPRIO`, is handled nonfatally |
| `0x00501EC6` | `85 F6 74 22` | Test `Acquire`'s HRESULT in ESI |
| `0x00501EDD` | `85 F6 7D 0B` | Only **negative** error results take fatal branch |
| `0x00501EE6` | `56` | Push the exact returned HRESULT for exit |
| `0x00501EE7` | `E8 15 82 02 00` | `CALL 0x0052A101` — native CRT exit wrapper |
| `0x0052A101` | `6A 00 6A 00` | CRT `exit` wrapper accepts status arg |
| `0x0052A1C0` | `FF 15 CC B0 53 00` | IAT `KERNEL32.ExitProcess` |
| `0x0052A1C6` | `5F C3` | **Return address observed in Wine relay** |

The negative `Acquire` HRESULT is also sent to the game's DirectInput
error-message formatter at **`0x004FC240`**, with embedded historical
source path **`C:\backup\SpideyPC\SpideyPC\D3d\DXInput.cpp`**
and source line **`0x2EA = 746`**. The original source files themselves
have not been recovered.

Interpretation: on a detached pad, Wine returns `DIERR_INPUTLOST`
from polling, then **`DIERR_UNPLUGGED=0x80040209` from the native
`Acquire`**. Original Spider-Man incorrectly treats this as a fatal
error, calling the standard CRT exit path and finally
`ExitProcess(0x80040209)` (shell exit status `9`).

The combined Wine `+dinput` trace and binary control flow make this
a **very strong root-cause reconstruction**, but no CPU-level stack
trace has yet independently recorded that `0x00501EE7` was executed
during the physical unplug. The other `ExitProcess` error branches
in the executable remain theoretically possible. Do not overstate
that last increment of certainty.

## Recovery design (not implemented in this analysis)

The proper fix is **not** intercepting all `ExitProcess` calls or
returning forged success to `IDirectInputDevice8::Acquire`.

1. Under strict original-binary signature and user-config opt-in,
   treat only `DIERR_UNPLUGGED` from the original joystick Acquire
   as a recoverable *device missing* event, without entering CRT exit.
   The game's existing `E_ACCESSDENIED` case is a **proven example
   of graceful return with `EAX=0`** (`0x00501EBB`).
2. Ensure all joystick actions, buffered button state and synthetic
   keys are released so a disconnected device cannot leave movement
   or menu confirmations stuck. Preserve keyboard/mouse and Alt+Tab.
3. Re-enumerate and bind a **new game-owned DirectInput joystick**
   after the physical device has returned or been replaced. PR #76's
   secondary DS4 device discovery alone cannot make the original
   engine's pointer `[0x006B7A2C]` refer to a newly plugged controller.
4. Build regression checks for the exact x86 bytes and branch jump
   bounds, keep the guard disabled by default until genuine Wine and
   Windows hardware tests. All changes through a draft PR.

Do not simply jump to the original successful-data path after
`DIERR_UNPLUGGED`: `GetDeviceData` and gameplay state should remain
invalid until a genuinely acquired new native device is available.
Preserve the game's other fatal error handling.

**Conclusion:** Original game-native joystick recovery, not
the patch's secondary DS4 DirectInput device, is the first blocking
dependency for hotplug. PR #76 remains experimental and unmerged.

legluondunet assisted by ChatGPT (OpenAI)


## Experimental stage 1 implemented (opt-in, PR #76 only)

`native_joystick_unplug.c` adds a **single** original-game x86 guard at
`0x00501EA6`. The original bytes at that site are
`81 FE 05 00 07 80`, the `cmp esi,DIERR_OTHERAPPHASPRIO`
immediately following native `IDirectInputDevice8::Acquire`.
The guard allocates its own RX stub, replaces **only those 6 bytes**
with a relative JMP and NOP, and restores the original compare inside
the stub for every other HRESULT.

The stub handles **only `DIERR_UNPLUGGED=0x80040209`**, clears exactly
the original buffered joystick-button bytes
`0x006B7A34..0x006B7A53` (32 bytes) to avoid held face buttons,
increments a counter, then jumps to the game's own nonfatal
`0x00501EBB` return, which sets `EAX=0` and restores the
original stack. No successful acquisition is forged; an unplugged
game-owned DirectInput interface remains detached. All other
HRESULTs follow the unchanged original `cmp esi,0x80070005`
and its native branch/formatter/exit handling. The patch checks the
original PE image base, 32-bit architecture and **10 exact byte
signatures** and is only requested after the existing full retail
SHA-256 verification. It installs on the game render thread at
Flip #120 (not under loader lock), using a permanent executable
allocation. Failure to meet any gate preserves the original behavior.

Add to the installed `SpiderMan2k1Fix.ini`:

```ini
[Input]
ControllerProfile=Off
ExperimentalNativeJoystickUnplugGuard=1

[Debug]
Log=1
```

`ControllerProfile=Off` isolates the native bug and avoids the
independent DS4 UI bridge on this first test; `Off` does NOT disable
the original game's own joystick. The new guard option **defaults to
0** and is repaired/validated with the other INI boolean settings.
On startup, confirm the log shows
`NATIVE-UNPLUG: experimental guard requested` and, after 120
frames, `NATIVE-UNPLUG: ACTIVE at Flip #120`. If the second
entry is missing, the guard is not installed and detaching the pad
can still intentionally exit the retail game. The next Flip after
a suppressed error should log
`NATIVE-UNPLUG: suppressed fatal native DIERR_UNPLUGGED`.
A *successful* first test means the game remains open and keyboard
controls still work when disconnecting the DS4 from the main menu.
Do **not** test repeated unplugs or live device replacement until
this is verified.

**Limitations and risks:** (1) native DirectInput device
re-enumeration/rebinding has **not** been implemented, so a newly
plugged controller will likely not recover movement; (2) the original
32-button buffer was verified statically but other analog/action caches
may need separate clearing; (3) intercepting a native code path is
experimental, even when SHA/signatures match. Physical Wine and
Windows tests remain pending; CI compilation and unit tests do not
validate runtime stability. Disable the option again if there is
any instability. Never merge PR #76 before explicit maintainer approval.

The native executable remains unchanged on disk. No global
`ExitProcess` patch, COM vtable replacement or fabricated HRESULT
is used.

legluondunet assisted by ChatGPT (OpenAI)


## Hardware acceptance — stage 1 under Wine: PASS (28 October 2026)

The maintainer **physically unplugged the Sony DualShock 4** from
the main menu and confirmed that **Spider-Man did not terminate** and
the main menus remained navigable using the keyboard afterward.

Supporting first-hand runtime evidence:
`SpiderMan2k1Fix(20261028-133230).log` (432 lines), using
`SpiderMan2k1Fix v1.1.0-9b5229d` on the exact SHA-256-supported
retail `SpideyPC.exe`.

- Startup: `GAMEPAD-UI: config profile=Off` and
  `NATIVE-UNPLUG: experimental guard requested`. Thus this result
  isolates the game-native input handling, without the experimental
  secondary DS4 UI bridge.
- At Flip #120: `NATIVE-UNPLUG: ACTIVE at Flip #120; strict
  original x86 signatures; only DIERR_UNPLUGGED returns
  nonfatally; native rebind NOT implemented`.
- After physical detach, at Flip #480:
  `NATIVE-UNPLUG: suppressed fatal native DIERR_UNPLUGGED=37;
  cleared 32 button states; native device still stale`.
- At Flip #600:
  `NATIVE-UNPLUG: suppressed fatal native DIERR_UNPLUGGED=157;
  cleared 32 button states; native device still stale`.
- Subsequent `QUICK-EXIT-F12: ENGINE-STATE flip=655` confirms
  the process and game loop were still executing after these
  recoverable failures. The maintainer independently verified
  that menu keyboard control remained functional.

**Result: PASS for the limited first-stage acceptance criterion —
main-menu physical controller detach does not terminate the game,
keyboard navigation survives.** This is the first physical validation
of the native error-path guard, not a confirmation that joystick
hot-replug or native `CreateDevice` rebind works. No 3D mid-game
detach, repeated hot-unplug cycles, Windows-native test, or reconnection
has been confirmed by this run.

**Engineering observation:** the recovered `DIERR_UNPLUGGED` branch
continues to be taken once per input update on the now-stale original
device (counter rose from 37 at Flip #480 to 157 at Flip #600).
A future recovery state machine should avoid endless futile
native `Acquire` attempts and should re-enumerate/rebind the
*original game-owned* device safely at a bounded interval.
Do not infer native-controller recovery from the game's continued
rendering; do not blindly replace the game-owned COM object without
understanding its ownership, cooperative level and data format.

Keep `ExperimentalNativeJoystickUnplugGuard` **OFF by default**
until more game states and a genuine replug are verified. Preserve
the PR #76 draft status and require explicit maintainer approval
before merging.

legluondunet assisted by ChatGPT (OpenAI)


## Experimental stage 2 — original-game native DS4 rediscovery and rebind (NOT hardware tested)

Implementation: `native_joystick_reconnect.c` and its bounded policy
`native_joystick_reconnect_policy.h`, in draft PR #76. This is distinct
from the independent `ControllerProfile` / `gamepad_ui_profile.c` UI
bridge: after successful recovery **Spider-Man's own controller** at
`0x006B7A2C` is replaced with a newly acquired game-owned device.

### Verified original executable behavior

Offline SHA-pinned original 2001 PE32 x86 disassembly establishes:

- `[0x006B7A30]` is the original game-owned DirectInput8A factory.
  Original `0x00501890` uses `EnumDevices(DI8DEVCLASS_GAMECTRL,
  callback=0x00501AE0, DIEDFL_ATTACHEDONLY)`.
- `0x00501AE0` creates a device, then reads its native
  `DIDEVCAPS`, recording the original button-count at
  `[0x006B7A58]` and the polled-device capability bit at
  `[0x006B7928]`.
- Original `0x005018BF..` binds the device to its original HWND
  `[0x006B7A60]`, using `DISCL_FOREGROUND|DISCL_NONEXCLUSIVE`,
  and `SetDataFormat` with the **game's own** data-format structure
  at `0x0053D0F4` (24-byte `DIDATAFORMAT`: 80-byte data,
  44 objects, object table `0x005177B0`).
- Native `SetProperty` sets `DIPROP_BUFFERSIZE=16`, absolute
  axis mode, and `DIPROP_RANGE=-1000..1000` for axis offsets
  **`0` (lX)** and **`4` (lY)**, before the original `Acquire` and buffered
  input reads. All of these match the byte-for-byte original code.
- Original cleanup `0x00501440` Unacquires and Releases the
  original joystick pointer. Its reader `0x00501E6D` explicitly
  tolerates a null pointer and returns without calling Poll/Acquire.

### New opt-in state machine

Add to the installed INI:

```ini
[Input]
ControllerProfile=Off
ExperimentalNativeJoystickUnplugGuard=1
ExperimentalNativeJoystickReconnect=1

[Debug]
Log=1
```

Both experimental settings default to `0`. Reconnect **refuses to
initialize unless the stage-1 guard is also configured and successfully
installed**. All work occurs on the original game's Flip/render thread
from Flip #120 onward, **only while the game is foreground**.
No background worker or COM-vtable hook is installed.

1. On the stage-1 guard's first real `DIERR_UNPLUGGED` event,
   atomically clear the original joystick pointer and release its
   already-lost COM object **without re-Unacquiring it**. Clear the
   32 native buffered button bytes; original keyboard remains intact.
   This also avoids endless `Acquire` calls against the old device.
2. Every 2 seconds (wrap-safe `GetTickCount`), scan only **currently
   attached** game controllers with the **game-owned factory**.
   Accept exactly one Sony DS4 product
   `VID=054C, PID=05C4/09CC/0BA0`; ambiguous matches fail closed.
3. Create a fresh candidate through that original factory; reproduce
   the exact original HWND, data format, buffer size, axis mode and
   range settings, and `Acquire` it. On any failure, Release the
   candidate and retain keyboard-only mode.
4. Require all 32 button states to be released and flush the finite
   buffered DirectInput event queue (up to 8 batches of 16).
   If nonneutral/unready, Release and retry after 2 seconds.
5. **Only after fully successful initialization**, compare-and-swap
   the fresh device into the **original game-owned**
   `[0x006B7A2C]` pointer, update original capabilities,
   and log `NATIVE-RECONNECT: REBOUND original game-owned DirectInput
   joystick`. Original native gameplay bindings are preserved;
   no separate synthetic action or default binding reset is used.
6. Every subsequent `DIERR_UNPLUGGED` event initiates the same
   bounded removal/recovery cycle.

The module verifies original x86 image base, original native
`EnumDevices`, `CreateDevice` and null-safe Poll signatures,
and the **original static DIDATAFORMAT bytes**. The overall fix
also requires the game's exact SHA-256. Invalid signatures disable
the module without patching COM memory.

### Remaining validation / limitations

**The first-stage unplug guard is physically validated; the
second-stage native rebind IS NOT.** CI tests cover compiled
configuration and policy only; they cannot prove a detached joystick
is rediscovered, safely released, or usable in a running retail game.

Recommended first test is **main menu only**, with the correct
Windows-x86 Actions build, the three INI settings above, and
the DS4 connected at startup. Verify `NATIVE-UNPLUG: ACTIVE at Flip
#120`, then unplug it once, confirm keyboard continues to work and
observe `NATIVE-RECONNECT: game-owned stale joystick released`.
Replug the same DS4 once and *do not hold buttons while it is
reacquiring*. Look for `NATIVE-RECONNECT: REBOUND...`, and test
menu navigation using the original controller input. Preserve and
submit the new timestamped fix log. If it fails, disable only
`ExperimentalNativeJoystickReconnect`; stage-1 guard still protects
the game. Do not stress-test repeated detach or 3D until this succeeds.

Important risks: the native game owns this device pointer, and its
readers outside the inspected render/input path may have additional
assumptions; physical validation is required on both Wine and native
Windows. We do not promise recovery of another brand of controller.
The independent DS4 UI bridge `ControllerProfile=Auto` has **not**
yet been tested together with native device rebinding. Keep
`ControllerProfile=Off` for the isolated first acceptance run.
**Do not merge the draft PR without maintainer approval.**

legluondunet assisted by ChatGPT (OpenAI)


## Wine hardware test: native rebind works, analog X axis regression found (2026-10-28)

The maintainer tested the initial stage-2 Windows-x86 artifact
`SpiderMan2k1Fix v1.1.0-ca3076f` (release from PR #76), with
`ControllerProfile=Auto` and both
`ExperimentalNativeJoystickUnplugGuard=1` and
`ExperimentalNativeJoystickReconnect=1`.
Submitted runtime evidence: `SpiderMan2k1Fix(20261028-135045).log`.

- Physical DS4 reconnect **works twice**: native joystick `REBOUND`
  after detach with `VID=054C PID=09CC`, `buttons=14`, `polled=0`.
  Log lines report `rebound=1 scan=8` and `rebound=2 scan=14`.
- Original native buttons and directional pad function after reconnect,
  and the game remains alive. The maintainer explicitly reports that
  **the analog joystick no longer responds**. This is a partial PASS
  and analog regression, **not complete hotplug acceptance**.
- Comparing the actual SHA-pinned retail `SpideyPC.exe` x86
  disassembly to our stage-2 `configure_candidate()` found a precise
  mismatch: the original game's first `DIPROP_RANGE` setter at
  `0x005019E0` supplies `dwObj=0` (axis **lX**), and the
  second at `0x00501A42` supplies `dwObj=4` (**lY**). Both set
  `DIPH_BYOFFSET` and `[-1000,+1000]`. The initial stage-2
  implementation incorrectly set **16 (lRy)** and 4, leaving native
  X-axis with its default scaling. This is a supported cause for
  missing or broken analog navigation after hot-rebinding.
- **Follow-up code fix:** `native_joystick_reconnect.c` now sets
  `DIPROP_RANGE` at offsets **0 and 4**, matching the original game.
  `ChatGPT/tests/test_native_joystick_reconnect.py` asserts those
  exact offsets and rejects the incorrect 16. No change to the
  previously physically validated native-unplug guard, native
  COM re-enumeration or button/cross recovery.
- **Current acceptance status:** axis correction code and automated
  tests only. **No user hardware retest of the analog stick yet**.
  Keep the new guard/rebind options OFF by default and PR #76 draft.
  Test original controller analog X/Y in-game after one replug, plus
  buttons and D-pad. If analog still fails, collect the new fix log
  and inspect native axis event values without guessing further.

legluondunet assisted by ChatGPT (OpenAI)


## Regression: controls and analog recover, late main-menu termination (28 October 2026)

The maintainer installed `SpiderMan2k1Fix v1.1.0-40c494f` and
reports **the DS4 reconnected successfully, the analog stick worked
again, and both digital buttons and D-pad worked, but the game later
crashed while testing menu controls**. The accompanying
`SpiderMan2k1Fix(20261028-140017).log` confirms the original retail
EXE SHA-256 and `ControllerProfile=Auto`.

Observed sequence (exact log line numbers):
- 409-410: native DI8 retry and stage-1 unplug guard installed.
- 433-440: secondary DS4 `DIERR_INPUTLOST` and complete release;
  original game-owned stale joystick detached and released.
- 442: a single `DIERR_UNPLUGGED` native fatal path suppressed.
- 455-457: Sony DS4 054C:09CC rediscovered and native
  `REBOUND ... rebound=1 scan=7 buttons=14 polled=0`.
- 458-464: game continues from Flip #840 through Flip #960,
  with regular menu rendering after rebind.
- **465, the last recorded fix message:**
  `GAMEPAD-UI: SYNTHETIC Return (DIK 0x1C) down #7`.
  This event is a synthetic ENTER produced by the independent
  `ControllerProfile=Auto` UI bridge. Earlier six Return events
  and Escape were handled before the unplug. **Temporal proximity
  does not establish causation.** The uploaded fix log does not
  include a fatal SEH exception, native fault address, Windows exit
  API call, or shell exit status; the exact crash mechanism remains
  unknown.

**State: stage-1 unload PASS; stage-2 rebind and analog PASS for one
physical replug; subsequent menu stability FAIL. Full hotplug remains
unvalidated and PR #76 must remain DRAFT/UNMERGED.**

Original retail EXE disassembly also shows the native DirectInput
cleanup routine at `0x00501440` owns additional COM objects
including a force-feedback effect at `[0x006B7A68]`. It is
**possible**, but *not established*, that the game's former
force-feedback interface or a menu-specific input handler retains
state across joystick rebind. Do not blindly Release or NULL out
such native COM objects until a crash location identifies the
actual failing path.

### Non-invasive new diagnostic breadcrumbs

`native_joystick_reconnect.c` now includes a bounded, passive
`SpiderNativeReconnect_LogSyntheticMenuKey`. After an actual native
rebind, `gamepad_ui_profile.c` logs before and after each synthetic
Enter/Escape keydown/keyup (`NATIVE-RECONNECT: POST-REBIND INPUT`),
with the **game-owned joystick pointer**, original FFB effect pointer
(only the value, not dereferenced), rebind number, native unplug
counter and action phase. Up to 40 entries per run, on the same
input thread, gated by the existing experimental rebind option.
**No new hook, COM call, memory modification, or input behavior
change** is introduced in this diagnostic. The log can tell if the
synthetic event completed delivery before a native menu handler
failed, but cannot by itself identify the faulting instruction.

For an actual fatal exception/exit signature on the next reproduction,
use the existing **external** Wine relay tool
`ChatGPT/diagnostics/run_hotplug_exit_relay_wine.sh` to capture
`+seh,+process` and the allowlisted `ExitProcess` family. Preserve
the matching fix log. If Windows process fault information remains
absent, collect relevant Wine coredump/call-stack data before applying
a speculative code hook. A control run with
`ControllerProfile=Off` (keeping the native guard and rebind ON)
can separately test whether the synthetic-menu UI bridge is necessary
for the crash. Do not assume Off disables the original game joystick.

Keep the known-good unpluggable but non-reconnecting configuration
available for regression isolation:

```ini
[Input]
ExperimentalNativeJoystickUnplugGuard=1
ExperimentalNativeJoystickReconnect=0
```

No current evidence warrants disabling the native joystick axis fix
or replacing the game's DirectInput8 factory.


## Extended menu soak: two unplug/replug cycles without crash (28 October 2026)

Maintainer reports the previously observed late menu crash **could not be
reproduced**. New attached fix log:
`SpiderMan2k1Fix(20261028-141227).log`, build
`v1.1.0-1070d30`, SHA-verified supported original EXE,
`ControllerProfile=Auto`, both experimental native hotplug
options active. This is a **positive repeatability test**, not proof
that the earlier crash cannot recur.

The log shows two complete physical detach/rebind cycles:
- First secondary DirectInput loss and release: log lines 428-435;
  native guard suppressed `DIERR_UNPLUGGED` (437); **native rebind
  #1** at line 452, `scan=7 buttons=14 polled=0`.
- Second detach: lines 495-503; native guard suppressed a second
  `DIERR_UNPLUGGED`; **native rebind #2** at line 519,
  `scan=14 buttons=14 polled=0`.
- After both rebinds, post-rebind passive traces show balanced
  `KEYDOWN` + `KEYUP` cycles for synthetic **Enter** and
  **Escape** (lines 457-543), all with stable
  `joystick=08308d5c` for generation 1 and
  `joystick=0835caec` for generation 2. In both generations
  `native-ffb=00000000` (no observed FFB effect pointer).
- Menus continue to at least **Flip #3422** (line 635), over
  3,400 rendered frames at the 30-FPS target. Two separate
  `SPIDEY-CFG: user settings saved; original joystick bindings
  preserved` operations (lines 611-613 and 625-627) complete
  after the second rebind.
- No crash, uncaught exception, DirectInput fault or unexpected exit
  message is present in the supplied fix log. The log ends while
  normal menu activity is in progress; it **does not establish
  a process exit code** or test actual 3D gameplay.
- No FFB effect was seen during this **menu-only** test, so the
  previous hypothesis that an existing force-feedback effect
  caused the late menu crash is unsupported by this run.

**Status:** after two real Wine unplug/replug cycles the original
controller and synthetic menu controls survived an extended menu
session. Axis analog movement had already been user-confirmed working
with the corrected X=0,Y=4 settings in the preceding hardware test.
The single earlier menu crash remains **intermittent/unexplained**,
so do not claim full stability or merge.

### Next low-risk milestone: 3D controller lifetime

No behavior change is warranted yet. To improve diagnostic coverage
for 3D gameplay without adding native memory hooks, the
experimental rebind module now:
- Resets its **40-entry** synthetic Enter/Escape trace budget on
  **each** successfully published native joystick generation, rather
  than exhausting one budget across all future replug cycles.
- Emits a **read-only** `NATIVE-RECONNECT: HEALTH` event on a
  mode transition, on each new rebind generation, and otherwise
  every 300 Flip calls (about 10 seconds at 30 FPS), capped at
  80 messages per process. It logs the inferred
  `MENU`/`3D`/`BINK`/`TRANSITION` mode, game-owned
  joystick pointer and FFB pointer *values* (no dereference),
  rebound number, and native unplug counter. The mode estimate
  uses **only the same existing render flags** already supplied
  to `SpiderGamepadUI_OnFlip`; it is not a new game-state
  mutation or verified original engine mode detector.

Suggested next single-hardware pass after the diagnostic build:
start menu with DS4 connected, enter a 3D level, test analog and
native actions, physically unplug **once in-game**, continue on
keyboard, replug without held buttons, and verify analog movement
and native actions still function. Return to a menu and exit
normally if possible. Save the matching fix log and any Wine
`+seh`/exit trace if an actual crash happens. This sequence
should be attempted once, not as repeated stress tests, while
PR #76 remains draft/unmerged. Revert
`ExperimentalNativeJoystickReconnect=0` to retain the already
validated unplug-guard-only behavior if any 3D instability occurs.
