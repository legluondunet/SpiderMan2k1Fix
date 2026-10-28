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
