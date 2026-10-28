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
- The independent `ControllerProfile=Auto` UI bridge now recognizes
  **one verified Xbox 360-compatible DirectInput device**, alongside
  the existing Sony-only profile. With Xbox selected, it uses the
  commonly observed DirectInput **A=BUTTON0 -> Enter**,
  **B=BUTTON1 -> Escape**, only in native 2D menus/Bink or
  known non-gameplay pause context. Buttons remain native in 3D.
  `ControllerProfile=Xbox360` can explicitly require Xbox hardware.
  The Sony PS1 preview is forcibly **excluded for Xbox**, even if
  the existing INI still has `ControllerPS1Preview=1`.
- **Important:** Holtek's actual physical A/B button indices have
  not yet been measured. The secondary Xbox interface logs the
  first 32 button transitions as `GAMEPAD-XBOX360: BUTTON-EDGE`,
  with the raw DirectInput held/changed masks; if A/B uses different
  indices we will refine the mapping using those real values. No
  Xbox START, Back, LT/RT or PS1-style override is synthesized.

**Important limitation:** Native Xbox 360/XInput controllers are not
guaranteed to expose the expected DirectInput device through Wine
or Windows drivers. The Linux USB identity doesn't confirm the
Windows DirectInput GUID product identity. Also, XInput and
DirectInput differ in their trigger representation (and rumble);
this implementation does **not** claim full XInput parity,
proper LT/RT independence or vibration. The current A/B menu bridge\nis experimental and its exact Holtek layout is not yet verified.

## First hardware experiment under Wine

Use the test build of this draft PR; install the matched x86 DLL
pair; disconnect the Sony DS4 for an isolated Xbox test. Keep all
other existing game settings but use:

```ini
[Input]
ControllerProfile=Auto
ExperimentalNativeJoystickUnplugGuard=1
ExperimentalNativeJoystickReconnect=1

[Debug]
Log=1
```

For isolation, `ControllerProfile=Xbox360` is also accepted and
requires an Xbox identity, whereas `Off` disables only synthetic
menu keys and leaves the original game's joystick input intact.
The existing `ControllerPS1Preview=1` setting, if present, is
ignored for Xbox and must never write Sony PS1 gameplay mappings.

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

### Actual first test and result (2026-10-28)

The maintainer tested `SpiderMan2k1Fix v1.1.0-2a1b2a1` and
submitted `SpiderMan2k1Fix(20261028-144306).log`. The
`GAMEPAD-UI` discovery reported `gamepads=1`,
`SonyDualShock4=0`, `last-product=045E:028E` and
`last-name=Controller (XBOX 360 For Windows)`. The *old*
Sony-only Auto bridge then rejected it: `no unique verified
DualShock4; auto profile inactive`. As the maintainer
observed, **the analog stick moved the native menus, but no
other button worked** there. This directly motivated the
Xbox-specific Auto and A/B menu bridge. The same test also
recorded the native controller **rebound=1**, with `buttons=10`
and `polled=0`, without any new crash documented.

For the next hardware test, confirm the log contains
`GAMEPAD-UI: PROFILE Xbox360 EXPERIMENTAL ACTIVE`.
Press **A and B individually** in a main menu, and verify Enter
and Escape as well as button masks. For example A=BUTTON0
would be `held=0x001`, B=BUTTON1 `held=0x002`.
This device may report other mappings; the masks are
measurements, **not** guaranteed mappings.
If the masks differ, use the recorded transitions to fix the
UI mapping before attempting more complicated combinations.
After A/B is verified, test other ABXY buttons, Start, D-pad,
triggers and both sticks in a 3D level and check the original
game's native `[Joystick]` bindings if needed.

Save the matching timestamped `SpiderMan2k1Fix(...).log`;
**do not claim Xbox menu button support has been physically
validated until A/B works**.

The **Xbox native VID/PID discovery and one hot-rebind**
have been physically observed under Wine; **Xbox A/B menu
mapping and full gameplay controls remain unvalidated**.
Keep its pull request in DRAFT and stacked on draft PR #76.
No merge without explicit maintainer approval.

legluondunet assisted by ChatGPT (OpenAI)
