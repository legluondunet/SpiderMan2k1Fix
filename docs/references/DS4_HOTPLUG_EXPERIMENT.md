# DualShock 4 hotplug experiment (stacked on PR #74)

Status: **experimental**, Windows x86 CI/static verification only. Real game
behaviour with disconnect/reconnect under Wine and Windows has **not**
been verified. This research branch is based on PR #74; it contains
none of the promised Xbox 360, DualShock 2 or rumble support.

## Scope of this first stage

- A single authentic Sony DualShock 4, verified through DirectInput8
  `guidProduct` vendor 054C and products 05C4, 09CC or 0BA0.
- Foreground-only discovery, every 2,000 ms while no verified secondary
  controller is available. Never enumerate inside the custom PAUSED loop,
  background windows or the original game's controller COM objects.
- On `DIERR_INPUTLOST`, do **not** immediately call `Acquire()` on the
  potentially unplugged secondary device. Release the secondary device
  (with checkpoint logs), release any **synthetic** key, and schedule
  bounded rediscovery. Other read errors require three consecutive
  foreground failures before release. A one-off transient error does
  not immediately detach the device.
- Reacquisition uses the same strict unique-device rule as before:
  **exactly one attached game controller and it must be a Sony DS4**.
  Any ambiguous multiple-controller configuration leaves the bridge off.
- After reattachment (and after any foreground loss/read failure) the
  bridge consumes the current physical state until all first twelve
  digital buttons are neutral. A new later press can then generate
  the usual synthetic Return/Escape. No synthetic key is injected by
  the reconnect event itself, avoiding accidental New Game confirmation
  or opening the native 3D Escape menu.
- Game-owned mouse policy `MouseEnabled=0` remains independent of
  controllers and continues through the custom focus PAUSED screen.

## Important limits: not the complete final hotplug feature

- **The original retail game owns a separate controller device.**
  Reacquiring SpiderMan2k1Fix's secondary DirectInput device does **not**
  guarantee that the original game re-enumerates or reacquires its own
  device for movement, jumps and actions. This needs a physical test.
- The PS1 preview's seven **native gameplay bindings** are safely
  initialized through the original game's verified configuration load
  before the first real game entry. If the DS4 was absent at startup,
  that verified bootstrap was skipped: late discovery does **not**
  silently call the original config loader during 3D or rewrite game
  controller tables. In that scenario **synthetic UI input remains disabled** rather than
  silently falling back to the *different* legacy Cross/Options mapping.
  Restart the game with the controller connected to activate PS1 preview.
- Connecting or disconnecting while the **custom Alt+Tab PAUSED screen**
  blocks Flip does not trigger rediscovery until after keyboard wake.
  A failed read during this pause disables further pad polling until
  the wait ends; it does not issue an immediate `Acquire()` on a
  potentially unplugged device.
  Its static gamepad prompt is a snapshot from PAUSED entry, not a live
  device list.
- No claim about DS4 Bluetooth device-instance persistence, USB driver
  behavior, 2+ attached gamepads, true wireless reconnection or
  background hotplug notification. Discovery is periodic rather than
  WM_DEVICECHANGE driven.
- This is **not** Xbox/XInput, DS2/USB adapter, force feedback or rumble.

## Manual acceptance matrix

1. **Baseline:** authentic DS4 present before launch with PS1 preview:
   native movement/jump/attack, Cross in 2D, SHARE native pause,
   keyboard fallback and custom PAUSED wake work.
2. **Reconnect while in 2D menu:** release all buttons, unplug USB;
   no stuck Return or crashes. Replug the *same* DS4, wait at least
   2 seconds, release all buttons, press Cross **anew**. At most one
   menu confirmation must occur.
3. **Reconnect while in 3D:** original-game movement/action must be
   checked independently of secondary synthetic menu input; if native
   input fails after replug, report that limitation explicitly.
4. **Connect after startup with no DS4:** periodic discovery should
   recognize the verified device, but **no synthetic input** may be
   generated and this must **not** be reported as full PS1 native gameplay
   profile activation. Relaunch with the DS4 connected for PS1 preview.
5. **Alt+Tab during and after reconnect:** desktop pointer works,
   PAUSED freeze persists until keyboard or previously acquired DS4
   press/release; game internal mouse remains disabled when configured.
6. **Ambiguous devices:** two simultaneously connected gamepads must
   **not** activate a random Sony profile. No crash, no false
   synthetic actions; unplug the extra device and rescan.
7. **Bluetooth**, if available: distinguish host pairing/disconnect
   behavior from the game's own DirectInput enumeration; do not
   assume VID/PID/instance GUID remain identical.
8. **MouseEnabled=1** remains fully usable for menu clicks throughout.

A temporary debug `[Debug] Log=1` is useful. Watch for
`GAMEPAD-UI: DISCOVERY`, `GAMEPAD-HOTPLUG: lost secondary DS4`,
and `GAMEPAD-UI: PROFILE DualShock4 ACTIVE`. Repeated absence logs
are throttled while discovery continues.

## Safety and architecture

`ps1_ds4_hotplug.h` is an isolated portable C99 state machine
containing only timing, read-failure and neutral-button rules.
`gamepad_ui_profile.c` alone owns the secondary DirectInput device.
Its existing shared native gameplay mapping remains untouched.

No original `SpideyPC.exe` executable or audio files are modified.
The original-game profile bootstrap stays startup-only, and no
`VirtualProtect`/COM vtable/keyboard hook was added. Keep PR #74 and
its prerequisite PR #73 unmerged until their separate validation;
hotplug remains a stacked draft review candidate.

legluondunet assisted by ChatGPT (OpenAI)


## Disconnect crash investigation — 28 October 2026

The maintainer reported an immediate game crash after unplugging the Sony
`054C:09CC` DS4 during an experimental Wine run. Evidence:
`SpiderMan2k1Fix(20261028-122046).log`.

- Exact supported `SpideyPC.exe` SHA-256 and early PS1 bootstrap confirmed.
- Last trace: `GAMEPAD-UI: independent DirectInput read hr=0x8007001E; no Return emitted`.
- **No reconnection, release checkpoint or second failed read was logged.**
  The code at that revision called `IDirectInputDevice8_Acquire(device)`
  directly after logging the failed `GetDeviceState`; the log cannot
  distinguish a fault inside that call from a subsequent fault inside
  the original game's independently owned DirectInput device.
- Candidate safety change: on `DIERR_INPUTLOST`, **do not Acquire
  or Unacquire the already-lost secondary device**. Perform a single COM
  `Release` with logged checkpoints, then retry discovery after 2 seconds.
  The game-owned controller is untouched. On other failures, defer
  acquisition and require repeated errors before dropping.
- The custom PAUSED wake loop also avoids calling `Acquire` on a
  failed secondary read. Keyboard wake remains available.
- **Still experimental**: this does not claim the original game's
  native controller is hotplug-safe. The next physical run needs to
  determine whether the game survives the unplug; if it still crashes,
  capture a Wine `+seh` exception/backtrace to determine whether the
  game-native controller code or secondary COM release is the faulting
  site. Do not attempt unsafe native device-memory writes or broad
  exception swallowing without that evidence.


## Second immediate-unplug crash: native path still unresolved (28 October 2026)

Maintainer explicitly confirmed **the game again crashed immediately when physically
unplugging the DS4** after installing the defensive PR #76 build. The
`SpiderMan2k1Fix(20261028-123031).log` startup version
`v1.1.0-5799185` is **the correct GitHub Actions PR merge build** from run
`40687603987`; it is not an obsolete build. Its output shows clean
bootstrap and normal foreground rendering through Flip #480, but **no
secondary DirectInput error or GAMEPAD-HOTPLUG release checkpoint** before
the abrupt end. `hotplug-seh.log` contains Wine `RtlUnwindEx` traces,
including code `80000026`, but no attributable fatal exception, fault
address, Windows module name or backtrace. This capture does **not**
exonerate the game, patch or Wine. In particular it gives no proof that
the secondary device release fix can execute before the crash.

**Blocking acceptance condition:** the game must survive an immediate
controller detach before we can claim any hotplug support.

Do not add a guessed native joystick pointer patch, COM-vtable override,
or broad exception swallow based on this evidence: this project has
previously observed native COM-vtable hooks break `CreateDevice`.

### External Wine exit and driver diagnostics (read-only)

A separate diagnostic launcher is included at
`ChatGPT/diagnostics/run_hotplug_crash_wine.sh`. It does not change
SpiderMan2k1Fix, game binaries, hooks or INI. Run from a checkout of
PR #76, supplying the **actual game folder**:

```bash
bash ChatGPT/diagnostics/run_hotplug_crash_wine.sh '/path/to/Spider-Man'
```

- It launches the executable with `WINEDEBUG=+seh,+dinput` and existing
  `WINEPREFIX` (if set), respecting an existing `WINEDLLOVERRIDES`
  or defaulting to `winmm=n,b`.
- It captures a Wine log and a separate `WINE_EXIT_CODE` status file
  under `hotplug-crash-diagnostics/`, then returns that exit status.
- If debug volume is excessive, set `HOTPLUG_WINEDEBUG=+seh`.
- An exit code such as `139` is only **consistent with**, not proof
  of, a host SIGSEGV. A clean `0` does not prove gameplay succeeded;
  a game can deliberately terminate during device teardown.
- The user should also retain the timestamped SpiderMan2k1Fix log.
  If the termination is unhandled by Wine SEH, inspect matching kernel
  or systemd coredump data separately, with consent and relevant privacy
  precautions. Do not assume Wine `RtlUnwindEx code=80000026` indicates
  a fault.

**Next technical milestone:** determine whether the exit is a deliberate
native-game exit on device disappearance, an unhandled Windows exception
in a native controller read, or a host/Wine signal. Only then design
a guarded controller device lifetime bridge (if needed); the secondary
DirectInput8 polling alone is not sufficient.


## Third physical unplug: confirmed native-game re-acquire and exit code 9

Evidence from maintainer-supplied `SpiderMan2k1Fix(20261028-124112).log`,
`hotplug-dinput.log` (**313,652 lines**) and `hotplug-exit.txt`.
These are **logs of runtime observations**, not a verified decompilation
or source-level cause.

The exact same Sony DS4 instance
`{40b2cbcc-d2cb-11f1-8001-444553540000}`
(`VID_054C&PID_09CC`) is opened by **two independent DirectInput8 factories**:

| Owner | DInput factory | Device COM object | Evidence in Wine log |
|---|---|---|---|
| SpiderMan2k1Fix's secondary UI bridge | `00115AC0` | `0327A750` | Factory enumeration line 584; `CreateDevice` at line 599; its `Release`/destroy at 313630-313638 |
| Original game's native joystick | `03273BB0` | `03316480` | `CreateDevice` line 4624 writes pointer at **`0x006B7A2C`**, a retail-game global; its `Acquire` at 4774 and **313643-313644** |

At the actual unplug Wine logs **two distinct** failed HID reads:

```text
313621  0140:warn:dinput:hid_joystick_read GetOverlappedResult/ReadFile failed, error 1167
313622  0140:trace:dinput:dinput_device_internal_unacquire iface 0327A750.
313624  0140:warn:dinput:hid_joystick_read GetOverlappedResult/ReadFile failed, error 1167
313625  0140:trace:dinput:dinput_device_internal_unacquire iface 03316480.
```

Win32 error **1167** is `ERROR_DEVICE_NOT_CONNECTED`. Both
DirectInput device objects cease being acquired.

The patch's own log completes **all its shutdown breadcrumbs** in sequence
(lines 438-442): `GetDeviceState` returns `DIERR_INPUTLOST
0x8007001E`; `Release BEGIN`; `secondary stale device Release COMPLETE`;
`secondary device release COMPLETE; rescan deferred 2s`.
Wine independently corroborates the old secondary object
`0327A750` was released/destroyed (lines 313630-313638).

**After that**, the original game continues trying to poll its *separate*
`03316480` joystick:

```text
313642  012c:trace:dinput:dinput_device_a_Poll impl 03316480.
313643  012c:trace:dinput:dinput_device_a_Acquire impl 03316480.
313644  012c:trace:dinput:dinput_device_Acquire iface 03316480.
313645  012c:trace:dinput:dinput8_a_Release impl 03273BB0.
313648  012c:trace:dinput:DllMain inst 781E0000, reason 0, reserved 00000001.
```

`DllMain reason 0` is `DLL_PROCESS_DETACH`, and the nonzero
`reserved` is consistent with process-termination DLL detach. The
separately captured shell result is **`WINE_EXIT_CODE=9`** (an
application/Wine process exit status, *not* shell `128+signal`).
No `0xc0000005`, fatal SEH dispatch, stack trace or Unix signal
was identified in the submitted trace. The last `Acquire` call does
**not** prove that it itself failed or crashed; the game may subsequently
choose to shut down on an error status or a device-invalid state.
The exact branch issuing the exit status is **still unlocated**.

**Substantive new conclusion:** the patch's *secondary* disconnect cleanup
is no longer a likely immediate fault site; the original game's native
DirectInput path is still running on the detached joystick and the process
then exits. A fix cannot be complete until native-game device lifetime is
understood. The current 2-second re-enumeration in the *secondary* bridge
cannot run because the game has already terminated.

Next safe engineering step: obtain an exact retail-EXE offline disassembly
of the **native joystick Poll/Acquire failure branch and the call path
returning status 9**, with signature validation before any future patch.
Compare against a control trial with our DS4 secondary bridge disabled to
attribute whether the exit predates our patch. Do **not** broadly suppress
`ExitProcess`, hook arbitrary COM vtables, or return invented success
HRESULTs without identifying the exact native call site. The acceptance
blocker remains: **unplug must not kill the game**, then test original
game movement after new DS4 acquisition.


## Isolation test: crash persists with ControllerProfile=Off

Maintainer explicitly reports that **unplugging the DS4 still immediately
terminates the game with `[Input] ControllerProfile=Off`**. The
implementation verifies that `Off` bypasses the SpiderMan2k1Fix
secondary `DirectInput8Create` / `CreateDevice` path; PS1 native
preview bootstrap also returns early. This substantially narrows the
fault to the *retail game's own* DirectInput joystick and/or Wine's
HID/DirectInput handling. It does **not** completely isolate an unpatched
retail executable because independent display, focus, mouse and
keyboard patches remain enabled. The maintainer has not yet supplied a
fresh `profile=Off` fix log with an explicitly verified exit status,
so treat the test as a user-reported comparison rather than a source-
identified exit-path proof.

The last full `+dinput,+seh` trace under `ControllerProfile=Auto`
did establish native joystick object `03316480` performing `Poll`
and `Acquire` after USB unplug, followed by process teardown with
exit code `9`. `Acquire` may be *preceding* deliberate game shutdown,
not itself crashing. **Do not introduce a native COM-vtable replacement
or pretend this native device has reacquired on failure.**

### Focused process-exit API trace

New external script:
`ChatGPT/diagnostics/run_hotplug_exit_relay_wine.sh`.

Run from a checkout of this PR, with `ControllerProfile=Off` in the
game's INI and no other Wine program using the same Wine prefix:

```bash
bash ChatGPT/diagnostics/run_hotplug_exit_relay_wine.sh '/path/to/Spider-Man'
```

It temporarily enables Wine's documented `+relay,+process,+seh`
channels with a **specific allowlist** of process-exit API entrypoints
(`kernel32.ExitProcess`, `ntdll.RtlExitUserProcess`,
`ntdll.NtTerminateProcess`, CRT `exit`, etc.), instead of writing a
300,000-line joystick trace. It saves a previous `REG_SZ` value of
`HKCU\\Software\\Wine\\Debug\\RelayInclude` and restores it on
normal return, signal-driven shell trap or nonzero exit. For safety,
it refuses to overwrite a non-`REG_SZ` value. Like any external tool,
abrupt SIGKILL of the launcher or power loss can prevent restoration;
inspect the registry before running another Wine application if that
happens. Do not run other Wine processes within the same prefix
concurrently because the registry change applies to that prefix.

Upload the three diagnostic files it writes (full filtered relay log,
short exit-call summary, process-status file), plus the corresponding
`SpiderMan2k1Fix(...).log`. If a call such as `ExitProcess(00000009)`
is captured from the game process, that will help locate a specific
termination path; if no call is present, remember Wine's relay does not
intercept all internal/forwarded functions, so **absence alone does not
prove an OS signal or rule out an orderly termination**.

The ultimate task remains to locate the original retail joystick
failure path (and whether it exits on `Poll`/`Acquire` failure),
then add a narrowly signature-checked remedy **only if justified by
the captured call-site**. The hotplug feature remains blocked until
the game survives a disconnect. No new gameplay patch is proposed yet.
