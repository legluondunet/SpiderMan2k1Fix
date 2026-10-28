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


## Second physical Xbox 360 test: sticks/D-pad OK, native gameplay mapping misaligned

The maintainer reports the follow-up A/B-bridge experiment with the
Holtek 045E:028E pad:

- **Digital D-pad works** in menus.
- **Analog stick works** in menus.
- **Share/Back button does not perform the expected action**.
- **Options/Start triggers the native action associated with
  R2 on the PS1-themed layout**, instead of opening the in-game menu.
- Physical **Y** performs the game's native **Jump** action,
  while the desired face button is physical **A**.
- Physical **Y** confirms menus rather than physical **A**.
- No Xbox button opens the **in-game pause/menu**.
- **Important new evidence:** the game's native
  **Joystick Configuration menu is editable** on this device.

The underlying game stores **seven user-editable action bindings**
in the virtual 252-byte Spidey.cfg (the named `[Joystick]` keys
are authoritative). The factory game mappings are Jump=BUTTON3,
Kick=BUTTON4, Punch=BUTTON0, Web=BUTTON1, Aim=BUTTON6,
Zipline=BUTTON9, Swing=BUTTON7. A physical button triggering Jump
or Swing is consistent with these original mappings, but the
physical Y/A/Back/Start **DirectInput indices are not yet proven
by a new raw button-edge log**. Do not assume that the Holtek
controller exposes the standard official Xbox order.

### Correct interpretation: editable Joystick Configuration is a bug

The maintainer explicitly clarified that **the editable native
Joystick Configuration menu is a regression to fix**, not a request
to preserve in-game binding editing. The prior recommendation to
remap Jump/Start/Back in that menu was mistaken and is superseded.

The initial Xbox profile handled A/B menu keys but did **not**
install the same native Joystick-only read-only patch used by the
Sony PS1 profile. The actual 2026-10-28 Xbox log confirms profile
Auto+Xbox360 and original in-game joystick settings saved after
input interactions: the settings menu remained editable.

**Fix implemented in PR #77:**
- Share the **exact same six original x86 signature-checked**
  patches from `ps1_joystick_readonly_menu.c`. They bypass
  native capture/restore/rebinding only inside the **Joystick**
  Configuration menu; **Keyboard** is still editable.
- The patch becomes automatic after the original config bootstrap
  when a single Xbox360-compatible DirectInput controller has
  been verified, and is also attempted once if the Xbox is
  newly detected later in a foreground frame.
- Do **not** install twice or reinterpret Sony PS1 labels as
  Xbox labels: the name formatter uses confirmed A/B physical
  DirectInput indices (BUTTON0/BUTTON1) and a numeric fallback
  for all unverified Holtek physical indices.
- This feature does **not** itself modify the user's stored
  `[Joystick]` bindings, change a game action, or repair the
  bad Xbox button layout. Joystick read-only and gameplay
  mapping are two separate acceptance criteria. The previously
  validated Sony DS4 readonly path remains opt-in.
- A strict original EXE SHA and six original code signatures
  are still required; a signature mismatch refuses installation,
  leaving the original menu untouched and writing an explicit
  `GAMEPAD-XBOX360: JOYSTICK-READONLY REFUSED` log message.

**Still pending:** fix the Xbox gameplay action layout
(A should Jump, not Y; Back/Start should not trigger unrelated
actions), with an explicit and hardware-validated mapping. The
new read-only mechanism alone does not solve these assignments.
Genuine Xbox trigger axes and vibration also remain unverified.

### Hardware validation after the Xbox read-only fix

On the next instrumented Windows-x86 build of PR #77:

1. Launch with only the wired Holtek Xbox controller
   (045E:028E); `ControllerProfile=Auto`, Debug Log=1.
2. Confirm
   `GAMEPAD-XBOX360: JOYSTICK-READONLY ACTIVE` appears.
3. Open **Joystick Configuration**. The menu must still navigate
   and display the current bindings, but selecting a binding
   must **not** enter capture mode, assign a new button, or
   restore defaults. **Keyboard Configuration must still
   accept and persist edits**. No crash on Back/Escape.
4. Reopen Joystick Configuration, confirm the seven original
   action assignments did not change, and share the timestamped
   fix log. Check that the already working analog axes,
   D-pad, main-menu A/B and game-native hotplug still work.

If the native signature patch refuses, do **not** interpret
an editable menu as success: the log must be reviewed before
changing any native patch sites.

The user has **not yet validated the Xbox-only read-only
menu fix physically**. PR #77 remains DRAFT and UNMERGED.
No merge without explicit approval.

legluondunet assisted by ChatGPT (OpenAI)


## Xbox readonly menu validated in Wine — 2026-10-28 15:28:53

The maintainer uploaded a screenshot of the original **Joystick Configuration**
screen and explicitly confirmed that it is now locked with the experimental
Holtek Xbox controller. The same run used binary `v1.1.0-11c58c6` and
recorded:

- `gamepads=1 SonyDualShock4=0 Xbox360=1`, VID/PID `045E:028E`,
  `ControllerProfile=Auto`.
- `GAMEPAD-XBOX360: JOYSTICK-READONLY ACTIVE`, after the original game's
  virtual configuration read was confirmed.
- Native game setting writes occurred, but the screenshot and verbal
  confirmation establish **non-editable Joystick UI**, not a byte-for-byte
  persistence comparison. Keyboard Configuration editability remains to be
  explicitly checked.
- The screenshot shows JUMP=B, PUNCH=A, WEB=NONE, AIM=BUTTON6,
  ZIPLINE=BUTTON9, SWING=BUTTON7. These are observed associations, **not**
  a correct or validated Xbox gameplay layout.

The same runtime log reports
`PAUSE BLOCKED physical BUTTON7: native [Joystick] Web=0x00004000`.
This exposed a parser bug: 0x4000 is the original game's **NONE/unbound**
sentinel and must not itself conflict. A subsequent narrow correction allows
that sentinel while rejecting all other unknown out-of-range values.
**Crucially, Swing=BUTTON7 still conflicts with candidate Start=BUTTON7,
and Aim=BUTTON6 still conflicts with candidate Back=BUTTON6.** Thus this
parser correction alone does not make 3D Pause functional and is not a
reason to bypass the conflict guard. A safe reversible Xbox gameplay preset
needs a separately evidenced physical mapping, preserving saved user values.

The read-only joystick menu is **user-validated on this Wine setup**;
the follow-up parser correction is **not yet user-validated**. Keep this
entire draft PR unmerged until explicit approval.


## New independent Xbox gameplay prototype (opt-in, not user validated)

Set `[Input] ControllerXbox360Preview=1` only for a controlled first
trial on the single Holtek 045E:028E controller. Default remains **0**.
This is separate from the already user-validated native read-only Joystick UI.

The game-owned, original 252-byte configuration is temporarily overlaid
*before its authenticated original CRT reload*, following an early unique
controller discovery and a successful strict original-x86 Joystick lock.
It refuses startup when the HWND, early DirectInput acquisition, native
read-only patch, or game CRT read verification fail. Late hotplug never
installs this gameplay mapping. With the setting Off, gameplay is unchanged.

**Experimental seven-action digital map:** Jump=A/BUTTON0 (observed),
Kick=B/BUTTON1 (observed index, untested action), Punch=BUTTON2,
Web=BUTTON3, Aim=BUTTON4, Zipline=BUTTON5, Swing=BUTTON9.
**Do not claim BUTTON2/3/4/5/9 physical labels or XInput trigger parity**:
those assignments are only candidates pending a real full-controller test.
The profile releases both BACK/BUTTON6 and START/BUTTON7 from all seven
native gameplay actions, making synthetic ESC available for in-game
Pause; A/B also navigate the paused native menu. Digital BUTTON9 is a
provisional Swing fallback, not a right-trigger emulation.

The original player's seven [Joystick] assignments are snapshotted
before the overlay and restored byte-for-byte both in the persisted
[Internal] RawHex and each named [Joystick] entry on native settings
writes. The overlay is reasserted on virtual configuration writes.
Disable the option and restart to return to the player's prior layout.
No on-disk EXE changes, no XInput driver or native DirectInput vtable
hooks. As with the Sony profile, no live profile swap is attempted
when the pad is unplugged; restart after switching controller types.

For the **first Xbox-only test**, preserve the existing options and use:

```ini
[Input]
ControllerProfile=Auto
ControllerXbox360Preview=1
[Debug]
Log=1
```

Expect `GAMEPAD-XBOX360: JOYSTICK-READONLY ACTIVE`,
`GAMEPAD-XBOX360: PREVIEW EARLY PREFLIGHT OK`,
`SPIDEY-CFG: XBOX360-PREVIEW action=Jump ... temporary=BUTTON0`,
`SPIDEY-CFG: BOOTSTRAP ... [READ CONFIRMED]`, and
`GAMEPAD-XBOX360: PREVIEW ACTIVE`. Test A Jump, independent B Kick,
the six other digital actions, START/Back in gameplay and the
paused menu, entering/exiting native Joystick Configuration without
editing it, keyboard remapping, and at least one game settings save.
Finally disable preview and compare the seven original [Joystick]
values + [Internal] RawHex against a pre-test backup. Report the
full timestamped log. Abort the test and use the last validated
build if unintended actions/crashes occur.

**Caution:** No physical gameplay test has validated this new optional
profile. It is deliberately NOT the automatic Xbox default; no PR merge
or release without explicit maintainer approval.

legluondunet assisted by ChatGPT (OpenAI)


## Maintainer requested Xbox action parity with the existing Sony PS1 mapping

The desired Xbox digital action layout explicitly follows the existing
Sony DS4-to-PS1 profile:

| Game action | Sony PS1-style physical control | Xbox requested physical control | Current Xbox preview |
| --- | --- | --- | --- |
| Jump | Cross | A | BUTTON0 (A observed) |
| Kick | Circle | B | BUTTON1 (B observed) |
| Punch | Square | X | BUTTON2 (physical Xbox X not yet user-verified) |
| Web quick shot | Triangle | Y | BUTTON3 (physical Xbox Y not yet user-verified) |
| Aim | L1 | LB | BUTTON4 (LB not yet user-verified) |
| Zipline | R1 | RB | BUTTON5 (RB not yet user-verified) |
| Swing in flight | R2 | RT | BUTTON9 digital fallback ONLY, **NOT RT** |
| Pause | Share | Start | BUTTON7 synthetic Escape, provided no native action conflict |

The current Xbox preview already implements the first six action indices
in this order, but only A and B have been physically observed in the
user-supplied Holtek logs. No code should label the current BUTTON9
fallback as RT, nor advertise an actually working RT bridge.

**Why RT needs a separate physical measurement:** Under XUSB DirectInput
Microsoft's normal mapping combines LT/RT into one Z axis, whereas Wine
joystick overrides, different firmware or adapters can expose separate
axes. An old DirectInput game accepting only BUTTON-number action
assignments cannot bind a continuous RT axis by simply renaming BUTTON9.

A NEW passive opt-in diagnostic, `[Debug] Xbox360TriggerProbe=1`,
records up to 48 significant DIJOYSTATE2 raw axis changes in foreground 3D
with no game-input changes. It logs `GAMEPAD-XBOX360: TRIGGER-PROBE`
and raw `lZ`, `lRx/lRy/lRz`, `slider0/slider1`. Its default is zero.

To establish correct physical mapping, start a new game with only the
Xbox controller, `ControllerXbox360Preview=1`, `Log=1` and
`Xbox360TriggerProbe=1`. Stand still; release all buttons, press RT
alone slowly to full travel and release, then repeat with LT alone,
then press X/Y/LB/RB individually. Review the time-ordered log for axis
baseline and changes before implementing a safe held-action conversion
for RT. When finished, disable the diagnostic. The existing Xbox
digital Swing fallback remains provisional and should be replaced only
after the genuine trigger is measured and tested.

This stage implements **diagnosis, NOT working physical RT Swing**.
The branch is experimental, draft/unmerged, and Sony PS1 behavior is
unchanged.


## Physical Xbox 360 Holtek RT/LT report 2026-10-28 16:17:51

Uploaded real Wine log `SpiderMan2k1Fix(20261028-161751).log`
(binary `v1.1.0-794558a`). Startup proves exact supported EXE,
one Xbox `045E:028E`, strict `JOYSTICK-READONLY ACTIVE`,
`PREVIEW EARLY PREFLIGHT OK`, seven saved-to-RAM bindings,
authentic game CRT `[READ CONFIRMED]`, and `PREVIEW ACTIVE`.
Multiple `GAMEPAD-XBOX360: 3D MENU Back/Start -> Escape` entries
and native pause-ribbon draws follow, indicating Pause works through
the synthetic Escape bridge. This log alone does not prove that all
six mapped digital action labels match the physical pad.

The 14 passive trigger samples unequivocally show a shared `lZ`
axis with rest=32768; low=128, high=65408, and intermediate values.
`lRz=0` and both slider entries remain 0. Ordered groups:
`32768 -> 128 -> 17024 -> 32768 -> 43520 -> 65408 ->
51072 -> 32768`, then again two low/high full strokes.
The maintainer had been instructed to press RT first and LT second.
Therefore **low lZ=RT, high lZ=LT is the present working hypothesis,
not independently labeled device evidence**. Ensure a physical
user confirmation before promoting it to default.

The branch now includes a **separately opt-in** experiment:
`[Input] ControllerXbox360RTSwing=1` (default 0). It requires an
already activated unique Xbox native-game RAM preview and executable
original CRT load. Negative lZ below 22000 causes *one* scancode
KEYDOWN for the actual native Swing [Keyboard] DIK value (at config
offset `0x3c`), held until axis >=28000, focus loss, input loss,
2D/Bink/pause, or controller removal; all these cause one KEYUP.
The user's Swing scan is fetched from the virtual original game config
and rejected if invalid, ambiguous with any other key action or
dangerous/extended. No keyboard assignments or on-disk INI/raw saves
are changed by this experiment. The analog axes are still read
nonexclusively with `c_dfDIJoystick2`. This does **not** hook the
game's native DirectInput COM interfaces nor claim to work yet:
the game's 3D DirectInput keyboard response to held `SendInput`
requires user verification under Wine.

`BUTTON9` remains a separately working/unknown digital Swing fallback
for this first hardware proof, so the player is not left without
Swing if the trigger keyboard injection is refused. Remove this
fallback only after RT is confirmed on the physical pad.

Recommended isolated next test:

```ini
[Input]
ControllerProfile=Auto
ControllerXbox360Preview=1
ControllerXbox360RTSwing=1

[Debug]
Log=1
Xbox360TriggerProbe=1
```

Enter 3D and press RT alone, release, then LT alone, release.
Check `GAMEPAD-XBOX360: RT-SWING DOWN lZ=...` and
`RT-SWING UP`, and observe actual held Swing in-game;
LT must not trigger Swing. Next, while holding RT, press Start
and verify the keyup occurs before Pause. Confirm Keyboard remains
editable, Joystick remains locked, personal INI/RawHex unchanged.
Turn RTSwing option and trigger probe OFF after the experiment.
Keep a last user-validated DLL backup.

*This bridge is not yet user-validated and this PR remains DRAFT /
unmerged; no release without explicit approval.*
