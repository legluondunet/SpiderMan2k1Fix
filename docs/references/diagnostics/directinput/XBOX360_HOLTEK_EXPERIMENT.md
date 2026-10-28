# Xbox 360-compatible wired gamepad — independent DirectInput profile (experimental)

Status: **source/CI candidate only**, **NOT hardware validated**. This work
depends on the physical DualShock 4 hotplug implementation in draft PR #76;
the Xbox feature lives in its own follow-on draft PR. Neither PR is merged.

## Actual user-owned USB hardware (2026-10-28)

Linux `dmesg` on plug-in:

```text
idVendor=045e, idProduct=028e, bcdDevice=1.14
Product: Controller
Manufacturer: Holtek
input: Microsoft X-Box 360 pad ... driver xpad
input input152: unable to receive magic message: -32
```

This is **Holtek-manufactured Xbox 360-compatible wired hardware
reusing the Microsoft 045E:028E VID/PID**; do not attribute it
to Microsoft-manufactured hardware. The Linux `xpad` driver
recognized a gamepad input device.

The Linux xpad `unable to receive magic message: -32` is from
`xpad_start_input()`: a vendor USB control-transfer read
(`bRequest=0x01, wValue=0x0100`) used by certain third-party
Xbox 360-style devices, which emits a **warning** on failure
and proceeds rather than failing `xpad_start_input`. A Linux
`-EPIPE` means the USB control transfer stalled or its pipe
failed. This message **alone** does not prove button/analog
events are unusable, and is unrelated to the old Spider-Man
`DIERR_UNPLUGGED` exit. We must test actual input under
Wine before deciding whether a kernel workaround is needed.
Driver reference:
https://linux.googlesource.com/linux/kernel/git/torvalds/linux/+/6c790212c588fddeb0d852f2790840753bb604b1/drivers/input/joystick/xpad.c

Wine may expose an XInput-compatible controller through different
DirectInput identities (and potentially legacy wrapper devices).
The Linux USB VID/PID is not proof Wine's DirectInput
`guidProduct.Data1` matches it. Detect the **reported**
DirectInput product GUID and the single native gamepad count in
the timestamped fix log before asserting compatibility.

## Scoped feature design

New portable source `controller_device_identity.h` classifies
Sony DualShock 4 exactly as before and wired Xbox 360-compatible
`045E:028E`. USB wireless receiver `045E:0719` is **deliberately
excluded**: the receiver's own USB identity is not sufficient to
assert a controller is available. No generic Logitech or unknown
gamepad is mislabeled as Xbox.

`[Input] ControllerProfile=Xbox360` is **explicit opt-in**.
`Auto` remains Sony DS4-only to preserve its physically verified
PS1 UI profile and early virtual Spidey.cfg mapping. No Xbox
profile applies the PS1 seven-button patch, Sony SHARE pause, or
native Start unbinding.

The Xbox profile:
- Enumerates attached DirectInput8 `DI8DEVCLASS_GAMECTRL`.
  Requires **exactly one** physical candidate. No type selection
  when multiple gamepads are attached.
- Opens a **secondary** DirectInput8 device using native
  `c_dfDIJoystick2` and `DISCL_FOREGROUND|DISCL_NONEXCLUSIVE`.
- Uses A (provisional DirectInput BUTTON0) to send a single
  Enter edge and B (provisional BUTTON1) to send Escape only
  in menus, Bink or native pause menu; never fabricates 3D
  movement/action input or uses PS1's CROSS mapping.
  **Button indices require actual user hardware validation**.
- Original Spider-Man gameplay owns analog, D-pad, other buttons,
  camera mapping and in-game configurations; nothing is imposed
  on the seven native saved joystick action bindings.
- Retains the existing neutral-button hotplug gate, focus-loss
  release and scan interval. No extra poll thread or Wine
  registry manipulation.

The *existing* original-game hotplug options are **both OFF by default**:

```ini
[Input]
ControllerProfile=Xbox360
ControllerPS1Preview=0
ExperimentalNativeJoystickUnplugGuard=1
ExperimentalNativeJoystickReconnect=1

[Debug]
Log=1
```

The native reconnect now uses the game's existing DirectInput8A
factory, original x86 data format, native +/-1000 lX/lY axis range
and original pointer lifetime. It selects Xbox devices **only if**
`ControllerProfile=Xbox360` was explicitly selected. In
`ControllerProfile=Auto`, `Off` or `DualShock4`,
native hotplug continues to select DS4 only, as in tested PR #76.
The existing original x86 SHA/signature guard and
`DIERR_UNPLUGGED` interception are unchanged.

## Hardware acceptance to perform

First use this hardware with **one** gamepad attached. Keep
`ControllerPS1Preview=0` and the original seven native
bindings unchanged. Verify the game starts, reaches the menu
and log includes `GAMEPAD-UI: DISCOVERY gamepads=1 ... Xbox360=1
last-product=045E:028E`, then
`GAMEPAD-UI: PROFILE Xbox360 CANDIDATE`.

Check joystick X/Y analog, D-pad and original native seven
gameplay buttons **before** unplugging; then observe A/B
in menus. Provisional A/B mapping must be reviewed if
Wine exposes different DirectInput button indices. Only after
normal input is confirmed, unplug once, confirm the game
does not terminate, replug with no held buttons, verify
`NATIVE-RECONNECT: REBOUND ... Xbox360-compatible VID=045E PID=028E`
and all original game controls. Confirm a normal exit code.

**Do not infer success from successful compilation or the Linux
kernel's successful input-device registration.** If Wine never
shows 045E:028E as a DirectInput game controller, record the
discovered GUID/name and native game behavior and consider a
separate XInput-to-DirectInput compatibility strategy rather
than guessing or spoofing VID/PID.

All changes remain in a follow-on **draft PR**. No
merges without explicit maintainer approval.

legluondunet assisted by ChatGPT (OpenAI)
