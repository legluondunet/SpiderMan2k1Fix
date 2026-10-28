# Xbox 360-compatible controller support — experimental draft

## First hardware candidate (Linux xpad -> Wine DirectInput)

The maintainer's connected wired controller enumerates on Linux as:

```text
idVendor=045e idProduct=028e bcdDevice=1.14
Manufacturer: Holtek
Product: Controller
input: Microsoft X-Box 360 pad
usbcore: registered new interface driver xpad
input: unable to receive magic message: -32
```

The USB VID/PID is the common wired Xbox 360 identifier, but the
reported manufacturer **Holtek** means this particular device is an
Xbox-360-compatible third-party controller. We must not assume all
official Xbox hardware behaviors, button layouts or rumble semantics.

The Linux `xpad` messages with `-32` are USB-level transfer errors
(`EPIPE` on Linux). They may be harmless failed optional vendor
requests or indicate a device/protocol quirk; this log alone does
**not** prove lost game input. Do not suppress `xpad` warnings
or change Linux system drivers as part of the game's Windows DLL fix.

## What is actually implemented in this first slice

An isolated Xbox 360-compatible native DirectInput **reconnect**
extension on top of the already Wine-validated Sony DS4 stage-1
`DIERR_UNPLUGGED` guard and stage-2 game-owned rebind:

- The game's `IDirectInput8A::EnumDevices` scan accepts its existing
  Sony DS4 identities and a strict list of Microsoft Xbox 360
  `VID=045E` PIDs `028E` (this wired Holtek sample),
  `028F` and `02A1`. All unknown or ambiguous candidates are
  refused. In particular, `045E:0719` (wireless receiver interface)
  is not blindly treated as a gamepad.
- It uses the **original** game-owned DirectInput factory and
  original native `DIJOYSTATE` data format, `lX/lY` analog ranges
  and neutral-button handling. No new COM-vtable hooks, no XInput
  loading, and no changes to the existing validated Sony DS4 path.
- The selection is limited to **one total supported candidate**
  (DS4 *or* Xbox 360). When both are attached, automatic rebind
  refuses to guess which belongs to the game.
- Native recovery remains *opt-in*:
  `ExperimentalNativeJoystickUnplugGuard=1` and
  `ExperimentalNativeJoystickReconnect=1`.
- The separate Sony-PS1-themed `ControllerProfile=Auto` layer
  remains Sony DS4 only; no Xbox360 synthetic Enter/Escape, SHARE,
  OPTIONS or PS1 button overrides have been introduced. This
  intentionally avoids emitting Sony-specific keyboard events from
  arbitrary Xbox gamepads.

**Important limitation:** Native Xbox 360/XInput controllers are not
guaranteed to expose the expected DirectInput device through Wine
or Windows drivers. The Linux USB identity doesn't confirm the
Windows DirectInput GUID product identity. Also, XInput and
DirectInput differ in their trigger representation (and rumble);
this implementation does **not** claim full XInput parity,
proper LT/RT independence, vibration, or Xbox-style menu mapping.

## First hardware experiment under Wine

Use the test build of this draft PR; install the matched x86 DLL
pair; disconnect the Sony DS4 for an isolated Xbox test. Keep all
other existing game settings but use:

```ini
[Input]
ControllerProfile=Off
ExperimentalNativeJoystickUnplugGuard=1
ExperimentalNativeJoystickReconnect=1

[Debug]
Log=1
```

Connect the Holtek Xbox 360-compatible controller **before game
startup**. First verify native joystick movement, ABXY buttons,
D-pad and both analog sticks without unplugging. If these inputs
do not work initially, collect a Wine DirectInput enumeration trace
before attempting any unplug: USB recognition alone is insufficient.
If baseline controls work, physically unplug **once in the main
menu**, verify keyboard control persists, then replug with buttons
released, and look for:

```text
NATIVE-RECONNECT: REBOUND original game-owned DirectInput joystick Xbox360-compatible VID=045E PID=028E
```

The exact button numbers and any differences between left/right
triggers need real hardware observation before defining an Xbox
`ControllerProfile` and mapping menu confirmation/pause. Save
the matching timestamped `SpiderMan2k1Fix(...).log`; do **not**
declare functional success until real analog/digital input and
a clean exit are confirmed.

The new Xbox functionality has **no hardware validation yet**.
Keep its pull request in DRAFT and stacked on draft PR #76.
No merge without explicit maintainer approval.

legluondunet assisted by ChatGPT (OpenAI)
