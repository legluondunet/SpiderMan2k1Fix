# SDL3 → game-owned DirectInput: experimental one-player controller owner

**STATUS (28 October 2026): DRAFT, not a release. Unverified with physical
multi-controller gameplay.** Stacked on SDL3 read-only discovery PR #79.
Do NOT merge until both Windows 11 and Wine physical validation.

## Motivation / evidence

Two hardware logs from the maintainer isolate the issue:
- **DualShock 4 + Xbox 360 at startup**: DirectInput UI reports
  `gamepads=2`, rejects the ambiguous legacy unique-device requirement,
  and has no `GAMEPAD-UI: SYNTHETIC` buttons. SDL3 correctly discovers
  both, selects Xbox via Start and can hand off selection to Sony.
- **Xbox 360 alone**: DirectInput bridge chooses the single Xbox
  `045E:028E`. Physical A/B correctly produce synthetic Return/Escape
  menu keys, while SDL3 auto-selects the sole gamepad.

The missing component is the connection between SDL3's **logical instance
selection**, the game's actual **single DirectInput8 joystick pointer**,
and the separate **2D/Bink secondary UI bridge**.

## Enabling a controlled trial

Retain a known-good backup of both `winmm.dll` and
`SpiderMan2k1Fix.dll`, use the verified original `SpideyPC.exe` and
place the **official Windows x86 SDL3.dll** beside the executable.
Update only the paired DLLs from this draft PR's successful CI artifact.

Add / confirm these hidden keys in `SpiderMan2k1Fix.ini`:

```ini
[Input]
ControllerProfile=Auto
ExperimentalSDL3Probe=1
ExperimentalSDL3NativeOwner=1
ExperimentalNativeJoystickUnplugGuard=1
ExperimentalNativeJoystickReconnect=1
ExperimentalNativeJoystickMappingSwap=1

[Debug]
Log=1
```

None of these native-owner options are enabled by default. If any of the
three native UnplugGuard/Reconnect/MappingSwap options is absent/off, the
SDL3 ownership experiment **refuses activation**, leaving the prior passive
SDL3 probe and legacy controls. If SDL3.dll is absent/unloadable or an
unsupported EXE is used, ownership does not activate. Do not use a 64-bit
SDL3 DLL in this 32-bit game.

## Safety model

* SDL3 3.2+ loads dynamically, on the original game Flip thread only.
  The selected **SDL3 instance ID** is used for the logical handoff.
* SDL3 ID is not a DirectInput instance GUID. The bridge first uses SDL3
  VID/PID to find **exactly one** compatible DirectInput device by the
  selected product ID, then uses the actual **DirectInput instance GUID**
  for rebind. **Duplicate identical VID/PID devices are unsupported**,
  even when SDL3 can distinguish their instance IDs. No guessing by index,
  name, port number, enumeration order, or product-type count.
* The game-owned DirectInput8 factory, exact original 80-byte DIJOYSTATE
  format, native +/-1000 lX/lY axis configuration, buffer size 16 and
  null-safe game-owned joystick pointer `0x006B7A2C` are reused.
  Unknown x86 code layout or missing native unplug guard refuses the swap.
* An unclaimed multi-pad session silences the original game's
  arbitrary initial joystick. When Start selects a new gamepad, the
  previous native device and 32 buffered button bytes are released.
  New native joystick publication requires **all physical buttons
  released**, neutral reads and fully drained buffered events. No
  synthetic success for failed DirectInput reads.
* A live game-owned force-feedback effect prevents detach/swap.
  The game-owned COM vtable, keyboard/mouse devices, original EXE on disk,
  saved INI and SDL3 itself are not patched.
* The seven gameplay joystick actions at the SHA-verified runtime table
  can change to the selected **canonical Sony/Xbox** profile, but only
  when the existing action table exactly matches Sony, Xbox or the
  original retail factory mapping. Customized player mappings are
  preserved; this is a possible limitation if the custom bindings are
  incompatible across controllers.
* The separate DirectInput menu/Bink adapter selects exactly the same
  VID/PID as SDL3 and does not activate until the *original game-owned*
  native device reports that product. On a handoff it drops the old
  secondary DirectInput device and synthetic key; there is no two-pad
  key or axis summing. Sony stays Cross-only for confirmation, and
  Sony SHARE / Xbox Back may synthesize 3D Escape only under the
  verified canonical native mapping and conflict checks. The existing
  Sony/Xbox native Quit-to-menu inherited-confirmation guards also
  accept these strictly verified SDL3 native profiles.
* Focus loss stops foreground polling, **preserves current selection**
  but disarms all Start switches; after Alt+Tab a held Start is ignored
  until released and freshly pressed. The original pause and keyboard
  wake logic remain in control. No new worker threads or SDL3 audio/video.

## Physical acceptance protocol (Windows 11 first, Wine second)

1. Start with BOTH Sony DS4 `054C:09CC` and Xbox 360 `045E:028E`.
   After SDL3 enumerates, selection should be 0 and the native arbitrary
   joystick should be detached. Keyboard still navigates.
2. Release both Start buttons. Press Xbox Start, then **release**.
   Expect `SDL3-PROBE: START selected logical id=...`, then
   `SDL3-OWNER: REBOUND` or `SDL3-OWNER: ALREADY-NATIVE`, and
   `SDL3-OWNER-UI: ACTIVE` for Xbox, without repeated native Acquire.
3. Xbox: check LEFT stick, A=Return, B=Escape in menus and during
   Bink, and check the **read-only Joystick** configuration without
   modifying it. Enter 3D and check the seven native actions, left
   analog, Back/pause and RT swing (when separately enabled).
4. Release Sony Start, press Sony Start, then **release**. Expect an
   SDL selection handoff, one old-owner release, neutral candidate
   initialization, native device GUID change, and UI profile Sony.
   Test Cross/Circle in menus and all native actions in 3D.
5. Switch back to Xbox without unplugging. Repeat 10 cycles, alternating
   menu and 3D, and ensure no held buttons, no accidental synthetic key,
   no double actions and no unexpected native crash.
6. Test unplug/replug of selected and nonselected devices; ALT+TAB in
   menu, Bink and 3D; foreground keyboard, Pause-wake and keyboard
   controls must remain intact.
7. Negative controls: no SDL3.dll, no selected device, unknown/virtual
   VID/PID, two pads with identical VID/PID, device lost during Acquire,
   user-custom joystick mapping and force-feedback effect present.

**Stop testing** on a crash or keyboard/input regression. Restart with
`ExperimentalSDL3NativeOwner=0` (or remove it) to return to the
known-good DirectInput behaviour, and provide the complete new log.


## Optional passive 3D acceptance trace

The dual-pad owner swap has now been observed on **actual hardware** in
`SpiderMan2k1Fix(20261028-214736).log` (DS4 → Xbox → DS4, menu/Bink).
The sample never enters 3D, so that log does **not** validate 3D analog
movement, attacks or mid-game Start handoff. The old run also repaired
three INI validation errors, saving the preceding configuration to
`SpiderMan2k1Fix.ini.old`. Identify those separately before assuming
what was invalid.

For the next physical **3D** run, you may turn on:

```ini
[Debug]
Log=1
SDL3GameplayTrace=1
```

Keep all owner opt-ins as listed above. This new diagnostic defaults OFF
and is only accepted when the SDL3 owner experiment is requested.
It samples the **already-read independent secondary** DirectInput joystick
state from the normal UI bridge, only in the 3D game context while the
selected physical ID matches the original game's verified native owner.
It does **not** call native `GetDeviceState`/`GetDeviceData`,
poll again, inject keys, change bindings or determine whether the game
actually acted on a physical press. Each entry reports selected SDL3 ID,
known device profile, all first 16 raw button states, raw `lX/lY/lZ`
and POV. Button edges are logged immediately, axes on significant
changes no more frequently than 2 seconds, and logging stops after
64 lines per process.

**Proposed test order:** Keep both pads plugged in. In the main menu,
select Xbox via fresh Start, open actual 3D gameplay, move the left
stick, use A/B/X/Y/LB/RB and pause/back, then press/release DS4 Start
without unplugging and repeat with stick, Cross/Circle/Square/Triangle,
L1/R1 and SHARE. Switch back to Xbox once more, use each pad for at
least one 3D action and test 3D → main menu → 3D. If the native game
does not respond, the trace helps tell apart **physical secondary
input** from **native DirectInput ownership**; it cannot by itself
prove a native action was consumed. Do not confuse raw `lZ` with RT
until the separate driver-specific trigger test is validated.

**Do not merge or release on the basis of a successful log alone.**
The maintainer must verify visible game response and stability, and
test both Windows and Wine before considering acceptance.

### Xbox 360 RT / stick-click correction (Windows 11 evidence, 31 Oct 2026)

The maintainer's actual Windows 11 log `SpiderMan2k1Fix(20261031-171330).log`
confirms selected Holtek `045E:028E` DirectInput device with **10 buttons**,
active SDL3 owner and `SDL3-3D-TRACE` entries that distinguish:
- **RT**: SDL3 RT `0..32767`, DirectInput `lZ` decreases
  from `32767` (neutral) to `128` (full). No DirectInput digital bit
  becomes pressed for the trigger.
- **LT**: SDL3 LT `0..32767`, DirectInput `lZ` increases to `65408`.
- **Stick click**: DirectInput physical `BUTTON9` (`held=0x200`)
  appears in 3D. The former Xbox seven-action mapping had Swing=BUTTON9.
- At test startup `GAMEPAD-XBOX360: RT-SWING OFF (default)`; that bridge
  could not drive Swing despite correct RT input detection.

**New guarded experimental behavior:**
- With the already-required `ExperimentalSDL3Probe=1`,
  `ExperimentalSDL3NativeOwner=1`,
  `ExperimentalSDL3GameplayBindings=1`, and the three native
  owner/hotplug options all `=1`, the RT-to-live-native-keyboard
  Swing bridge defaults **ON** for verified Xbox ownership. This
  does not turn on the legacy one-pad `ControllerXbox360RTSwing=0`.
  Optional `[Input] ExperimentalSDL3XboxRTSwing=0` turns off the
  new SDL3 automatic RT behavior (legacy opt-in remains separate).
- For **exactly** native Xbox VID:PID `045E:028E` with the game-owned
  driver reporting **exactly 10 DirectInput buttons**, native
  Swing=BUTTON9 is replaced by the original game's known UNUSED
  action-marker `0x4000` in the seven-action table. This avoids
  stick-click Swing; it **does not guess** an RT DirectInput button.
  All other Xbox models and drivers keep their previous fallback.
- The RT bridge takes the selected SDL3 normalized axis or the
  observed negative DirectInput lZ, checks canonical ownership,
  reads the verified native Swing keyboard scan, and sends a
  held press/release only in active foreground gameplay.
  Existing focus/selection/hotplug/pause key-up guards remain.
- The old Xbox BUTTON9 fallback is still recognized for legacy
  profile identity and is not rewritten when SDL3 is off.

**Test before merge** with Windows DS4+Xbox: enter 3D with Xbox selected,
press left-stick click **alone** (must not swing), then RT **alone**
(must swing while held), then LT (must not swing). Release RT and
press Left/Right/Start without stuck Swing. Switch Xbox→DS4→Xbox,
unplug/replug each and Alt+Tab. Confirm no accidental keyboard Swing
after selection, focus or pause changes. Repeat Wine/Proton tests.
Inspect `GAMEPAD-XBOX360: SDL3 RT-SWING ENABLED`,
`NATIVE-SWAP: APPLIED`, and `RT-SWING DOWN/UP` logs. If keyboard
injection is refused, capture `RT-SWING REFUSED` and verify the
original live keyboard Swing binding.

**This is a new candidate, not a hardware-verified correction.**

### Xbox RT → Swing compatibility check

The maintainer has now reported **successful 3D gamepad switching and physical
disconnect/reconnect** with both pads, but **Xbox swing on RT has regressed**.
The earlier verified Holtek DirectInput profile reported `lZ=32768`
neutral, `lZ=128` on RT, `lZ=65408` on LT, and triggered a held
keyboard Swing action (`DIK 0x22`). Native owner hot-swap may expose a
different DirectInput axis reading.

The experimental RT bridge remains opt-in via
`[Input] ControllerXbox360RTSwing=1`. When both SDL3 native owner and
the live original Xbox 3D mapping are strictly verified, Swing now accepts
either the earlier negative DirectInput `lZ` signal or the SDL3
**separate normalized RT** (`0..32767`, press at `11000`, release
below `5000`). A positive LT-only legacy axis, SDL3's LT axis, an
unselected pad, or an invalid SDL RT value never triggers Swing.
The keyboard scan code is still derived from the original live
keyboard mapping and checked against unsafe/duplicate bindings before
any injection. The button-9 / R3 native Swing fallback is not modified.

New bounded log lines `GAMEPAD-XBOX360: RT-SWING SOURCE`,
`RT-SWING BLOCKED`, `RT-SWING REFUSED`, and
`RT-SWING DOWN/UP` will distinguish physical trigger recognition from
live mapping refusal. With the Xbox selected in actual 3D, try RT while
watching Spider-Man. Then, if Swing fails, supply the log to diagnose
native mapping ownership versus the keyboard scan rather than
inventing a replacement key.

This fixes a plausible driver-dependent input path, not a proven root
cause; physical verification is still required.

## Known limitations

This is an experimental **native DirectInput ownership** bridge, NOT a
full normalized SDL3-to-DIJOYSTATE virtual gamepad. Device compatibility
still depends on the OS/Wine DirectInput driver exposing the selected
controller. Identical products cannot be distinguished via VID/PID.
The original game native reader is not replaced by SDL3 polling, and
input latency/3D functional correctness must still be measured in the
physical hardware trial. CI compilation and test pass do not establish
live compatibility.

legluondunet assisted by ChatGPT (OpenAI)

## Experimental DS4-only live 3D gameplay profile

**DRAFT / NOT YET HARDWARE-VALIDATED (31 October 2026).** The SDL3
setup UI and two-column focus are validated with the maintainer's DS4
on Windows 11; that does **not** validate this new runtime engine-input
mapping. The existing game-native reader, analog stick axes, 2D menus,
Bink and original config file remain separate.

The live bridge is **OFF by default**. To test it, keep the existing
selected-native-owner options enabled and add only the final new key:

```ini
[Input]
ExperimentalSDL3Probe=1
ExperimentalSDL3NativeOwner=1
ExperimentalNativeJoystickUnplugGuard=1
ExperimentalNativeJoystickReconnect=1
ExperimentalNativeJoystickMappingSwap=1
ExperimentalSDL3GameplayBindings=1

[Debug]
Log=1
```

The implementation activates for **exactly one** Sony DS4
(054C:05C4, 054C:09CC or 054C:0BA0) that the retail game has already
selected as its *verified* DirectInput device, with the original known
canonical Sony seven-action table and a completely neutral SDL3
physical sample at the 3D Flip boundary. Only the seven known native
gameplay action DWORDs in the authenticated 14-slot RAM table at
`0x568408` are changed; each change is read back and is only allowed
on a verified `PAGE_READWRITE` data page. The eighth saved semantic
action, **Pause**, uses a fresh SDL3 press to request one guarded native
Escape keypulse via the existing gamepad UI key owner. No original
joystick COM vtables, analog axes, keyboard mapping, executable on disk,
or `SpiderMan2k1Fix.ini` contents are rewritten by this action bridge.

Verified physical DS4 conversion: Cross → native BUTTON1, Circle →
BUTTON2, Square → BUTTON0, Triangle → BUTTON3, Share → BUTTON8,
L1 → BUTTON4, R1 → BUTTON5, and R2 → BUTTON7. **L2 has no independently
verified native button index**, and D-pad POV, stick clicks, touchpad
and extra buttons also cannot be assumed to be `rgbButtons`.
Any unsupported binding assigned to any of the seven core actions
refuses the **entire profile** atomically, leaving the original game
controller mapping in place. Xbox mappings also remain unchanged until
their driver-specific trigger behavior is established.

The previous seven-action table is snapshotted before a successful
application. When leaving gameplay 3D (including menu/Bink/native pause),
losing foreground, changing pad identity, attaching a second pad,
or editing the profile, the module restores the original values if
the live table still exactly matches what it last wrote.
**The selected saved Pause binding remains the sole active 3D/native
pause key owner:** on native Pause, all seven game action DWORDs revert
to normal, but the physical Pause press/release history is preserved so
only a new press may resume gameplay. The old DS4 SHARE and OPTIONS
synthetic Escape paths are suppressed while this verified SDL3 owner
is active, including while the game is natively paused. Menus, Bink,
Alt+Tab, identity/focus changes and unsupported profiles still disarm
the live Pause state. Keyboard Escape is always retained. The extra
`PreNativeOnFlip` gate restores the mapping *before* any native pad
rebind. If someone else changes the table unexpectedly, the module
refuses an unsafe overwrite and logs that event.

For first acceptance, back up the DLLs, use the default saved DS4
profile, enter gameplay and check normal 3D movement, R2 Swing and
Share Pause. Return to the menu, then use the SDL3 assistant to **swap
Jump (Cross) with Punch (Square)**, release all buttons, re-enter 3D,
and verify both physical actions swap in gameplay. Return to menus
and check Cross/A and Circle/B still Confirm/Back normally. Alt+Tab,
the custom PAUSED screen, and focus-recovery should remain unchanged.
Check `SDL3-3D-LIVE: APPLIED`,
`SDL3-3D-LIVE: original seven DS4 action bindings RESTORED`, and
explicit `REFUSED` warnings in the log. Remapping Jump may also
change the physical key used by the original 3D cinematic skip
path, which is still game-native. Revert
`ExperimentalSDL3GameplayBindings=0` if the runtime behavior differs
from expectations. **Do not merge PR #80 until Windows and Wine
gameplay testing** confirms owner, pause, triggers and safe rollback.

## DualShock 3 (054C:0268) — detected, native support NOT enabled (31 Oct 2026)

**User Windows 11 log**: `SpiderMan2k1Fix(20261031-210857).log`,
application `v1.1.0-4c5c7d9`; user confirms customized Xbox360
profile **works** in 3D. PS3 controller **does not work** during the
same session. This is **not** a supported DS3 gameplay build.

PS3 VID:PID **`054C:0268`** is enumerated by Windows
DirectInput (`PLAYSTATION(R)3 Controller`) and SDL3 (`PS3 Controller`).
SDL3 attaches PS3 at startup (id=1), detaches it at flip ~720, later
attaches id=4 at flip ~3658, detaches at ~4200, and re-attaches id=5
at ~4280. User had Xbox active, so `selected-id=2` stayed Xbox
through those events. There is **no `START selected logical ... 054C:0268`**
event and no `SDL3-PS3-PROBE` physical-button trace in the old build,
so **we cannot claim whether SDL3 ever observed the PS3 Start press**.
Native code explicitly recognizes only DS4/Xbox360:
`spider_reconnect_kind(0x054c,0x0268)==SPIDER_RECONNECT_NONE`.
Do not infer DS3 physical DirectInput indices from DS4 or Xbox layouts.

A **read-only diagnostic candidate** is added to this draft branch:
- For **only** SDL3 `054C:0268`, log up to **48** changes of its SDL3
  normalized 21-button mask and the Start bit. No native Acquire,
  DirectInput hook, configuration write, input forwarding or driver
  substitution. Log prefix `SDL3-PS3-PROBE`.
- A fresh Start on an **unsupported** SDL3 model now explicitly logs
  `SDL3-OWNER: START REFUSED unsupported native controller` and
  preserves the last selected controller, instead of detaching the
  previously verified Xbox/DS4 owner. The controller-setup-modal
  deferred Start route applies the same native model gate.
- Sony DS4 and Xbox360 runtime mappings, including customized
  Xbox six-button mappings and RT Swing, remain unchanged.

**Next Windows test**: Xbox and DS3 connected, gameplay foreground,
press/release DS3 Start and Cross, Square, Circle, Triangle, L1/R1,
and L2/R2 once each; retain the complete log. Determine if SDL3
produces normalized Start and face-button edges, and whether the DS3
disconnects unexpectedly. For actual native DS3 gameplay support,
separately verify exact original-game DirectInput button indices,
axis ranges, trigger representation and Pause semantics on this
specific Windows driver before authorizing any new native mapping.
**No DS3 native gameplay is enabled by this probe**, and no merge
until physical validation.

## Xbox 360 custom 3D mappings — B/X and Y/A swap (31 Oct 2026, candidate)

**User Windows 11 hardware result**: `SpiderMan2k1Fix(20261031-205202).log`
(version `v1.1.0-55f3b3f`). The user swapped **Xbox B↔X,
Y↔A**, and **DS4 Triangle↔Cross, Circle↔Square**. In playable 3D the
custom DS4 mappings worked, but after pressing Xbox Start the Xbox
returned to its factory face-button layout. This is a **confirmed
functional gap**, not evidence of a failed INI read/write.

The log showed `SDL3-PROFILE: GAME model=045E-028E LOADED`,
`SDL3-PROBE: START selected logical id=2`, `NATIVE-SWAP:
APPLIED ... DS4->Xbox360`, then `SDL3-3D-LIVE: REFUSED unsupported
SDL3 binding/driver; only verified Sony DS4 ...`. The Xbox native
seven-button table was canonical but **hardcoded**, and the SDL3 semantic
profile's customized face mappings were **never applied to gameplay**.

**Candidate fix, Windows re-test REQUIRED**:
- Extend the guarded `sdl3_gameplay_live.c` native 3D mapping from
  Sony DS4 only to the exact previously verified **wired Xbox
  `045E:028E`**. Translate SDL A/B/X/Y to the observed DirectInput
  physical BUTTON0/1/2/3 and LB/RB to BUTTON4/5. For the user's swaps,
  the native seven-action table should be exactly
  `3,2,1,0,4,5,0x4000` (Jump/Kick/Punch/Web/Aim/Zipline/Swing).
- **Swing remains RT only**: its native BUTTON index stays **UNBOUND
  `0x4000`**, and the existing keyboard RT-Swing path is permitted when
  the exact installed custom Xbox map is verified on the current,
  unique selected game-owned DirectInput device. Without this
  additional check, the previous `CanonicalXboxMapping` guard
  would disable Swing as soon as a face mapping is customized.
- An Xbox profile with Swing reassigned away from RT, or any unverified
  LT/D-pad/POV/noncanonical physical button is **atomically refused**
  by this *native* mapping bridge. These bindings remain visible in
  the UI but are not yet supported by the 3D renderer. Do not silently
  map RT to stick click or claim arbitrary trigger support.
- Keep exactly the earlier DS4 behavior, private PAUSE scope and
  verified owner/foreground/neutral-first prerequisites. Never
  change the original Spidey.cfg or saved INI, hook COM vtables,
  inject gameplay face-button keys, or patch executable code for this.
- Snapshot the exact canonical Xbox seven-word source table before
  temporary changes. Roll back before gamepad owner swap, focus loss
  or leaving 3D, preserving the native reconnect's canonical-entry
  preconditions; reject unexpected external table mutation.
- Portable C99 test verifies the precise Xbox B/X, Y/A permutations,
  default Xbox table, strict product match, negative LT/POV
  cases, and unchanged Sony canonical mapping. Do not infer hardware
  validation solely from these tests.

**Acceptance test**: Xbox/DS4 both connected; set both customized
profiles as above → start playable 3D with DS4 (verify its buttons) →
Xbox Start, wait until neutral, press Xbox A/B/X/Y (all custom actions),
then Xbox RT (Swing still works), then DS4 Start (old DS4 mapping
restored correctly) → repeat with Alt+Tab once. Check logs
`SDL3-3D-LIVE: APPLIED verified Xbox360 six-button SDL3 custom mapping`,
`RT-SWING DOWN/UP` on Xbox and clean
`SDL3-3D-LIVE: original seven selected-pad action bindings RESTORED`
before `NATIVE-SWAP`. Reopen profile UI and restart the game to
confirm each model's custom mappings still persist. No merge into
`main` until explicit hardware validation.

## Windows 11 hardware matrix: Alt+Tab / hotplug good, same-row Start pending (31 Oct 2026)

**User evidence**: `SpiderMan2k1Fix(20261031-184751).log`, DLL
`v1.1.0-002dea5`. Confirmed by hardware feedback:
- SDL3 panel **Alt+Tab works**; no stuck controls on return;
- unplug/replug within panel, then change control **works**;
- Xbox RT → Swing **works**;
- **NOT OK:** Xbox game owner → L1/R1 browse DS4 profile → Start
  on displayed DS4 → close panel → DS4 Circle does not exit original
  Joystick menu; Xbox B still required. An Alt+Tab occurred in the session.

**Evidence and uncertainty:** the log records SDL3 instance 1 (Sony DS4)
detaching, then a replacement Sony instance **3** attaching. It records
two modal opens and normal closes but **no**
`SDL3-SETUP: MODAL-START`, `SETUP RETURN PENDING`, `COMMITTED`,
or `SDL3-OWNER REBOUND` towards DS4; Xbox instance 2 stayed the game
owner. The log did not previously record either manual L1/R1 browsing
or rejected raw Start presses, so it cannot prove whether the exact
Start edge was missed because of the neutral-arming gate, occurred
outside foreground, or wasn't registered for another reason.

**Targeted candidate (CI and hardware test required):**
- During `refresh_pads`, `SetupRead` already samples every SDL3
  controller. If a *newly discovered* or reconnected instance was
  actually observed with Start **released**, use that fresh neutral
  evidence to arm its modal Start edge **before the next 35 ms timer
  tick**. Never arm a controller with Start already held; focus-loss
  still requires released observation before a new Start takes effect.
- Add bounded diagnostic logging (**max 32 records per modal**) for
  `SDL3-SETUP: PROFILE-BROWSE L1/R1`,
  `SDL3-SETUP: START-SAMPLE` (armed / previous / capture / close),
  and `SDL3-SETUP: START-REFUSED simultaneous claims`. No global
  keyboard input, DirectInput rebind, or game action is added.
- Regression C99 tests cover newly attached DS4 `id=3`, first
  release→Start edge, rejected held Start, and Alt+Tab invalidation.

**Required hardware test**: Xbox selected by Start in game → open setup
(Xbox shown) → press R1 once (DS4 shown); fully release Start on both
pads → press DS4 Start once (same displayed DS4) → close by DS4 Circle
→ press DS4 Circle *again* in the original Joystick menu. Expected
`PROFILE-BROWSE`, `START-SAMPLE id=3 armed=1 previous=0`,
`MODAL-START ... selected-id=3`, `SETUP RETURN PENDING`,
`COMMITTED id=3`, `SDL3-OWNER REBOUND id=3`. If rejected,
the new logs will tell exactly which gate failed. Also test opposite
direction, focus loss while setup stays open, and one hotplug cycle.
This is **not yet a verified hardware fix**; do not merge the draft PR.

## SDL3 setup modal: close with controller transfers ownership (31 Oct 2026)

**User UX finding after successful handoffs**: with Xbox owning the original
game, L1/R1 displays the DS4 profile in the SDL3 setup dialog. Pressing
DS4 Circle closes that dialog, but the *underlying original Joystick menu*
still listens to Xbox, so DS4 Circle stops working. A second DS4 Start
in the original game restores controls, but this is an undiscoverable
extra step. The `SpiderMan2k1Fix(20261031-200734).log` confirms
`PROFILE-BROWSE L1/R1 display-id=1` and a normal B modal Close
**without** any pending owner request; then a later normal gameplay
`START selected logical id=1` activates the DS4 and physical Circle
is delivered to the original menu.

**New intentionally simple interaction rule**:
- L1/R1 only browses the displayed controller's profile while the modal
  is open; no native game state changes while it is active.
- **Physical Circle/B on the displayed controller** closes the modal and
  simultaneously marks that same displayed SDL instance (VID:PID) as
  the next game owner. **Cross/A on the panel's focused Close button**
  behaves identically. No extra Start press is required.
- The existing Start inside the modal also remains available and
  continues to request the displayed controller when the modal closes.
- Keyboard Escape, title-bar X and mouse/Win32 Close simply close the
  modal and **preserve the previous game owner**.
- Every handoff (both pad Close and modal Start) happens **only after**
  modal destruction and safe focus restore, followed by **the next
  foreground Flip**; it must revalidate SDL instance presence and
  unique VID:PID before native game-owned DirectInput reconciliation.
  Stale/disconnected or ambiguous controller identities fail closed.
- Existing modal Circle/B release-before-close remains mandatory so
  the game never inherits the closing keypress as a menu action.
  A **new Circle/B press** after return exits the game's original
  Joystick menu. Focus loss, hotplug and RT→Swing unchanged.
- New log `SDL3-SETUP: PAD-CLOSE GAME-OWNER requested selected-id=...`
  followed by `SDL3-PROBE: SETUP RETURN controller-initiated owner
  handoff PENDING`, `COMMITTED verified modal owner selected-id=...`
  and `SDL3-OWNER: REBOUND` (or explicit `REFUSED`).

**Windows acceptance needed**: Xbox active → open setup → R1 to DS4
→ Circle DS4 closes setup → Circle DS4 *again* exits the ORIGINAL
menu; no Start anywhere in this sequence. Repeat DS4→Xbox, verify
L1/R1 browsing followed by Escape/mouse Close does NOT transfer owner,
and exercise Alt+Tab/hotplug during a modal session. The old reported
crash must not reappear. This is a candidate, **not yet validated on
physical hardware**.

## SDL3 setup modal: Start handoff, then verified gameplay ownership (31 Oct 2026)

**Windows hardware validation**: Start now selects between DS4 and Xbox360
inside the SDL3 configuration modal and both models can configure their own
bindings. The log `SpiderMan2k1Fix(20261031-181858).log` confirms
`SDL3-SETUP: MODAL-START switched setup-only selected-id=1` (DS4), but
**after closing the modal**, `SDL3-OWNER-UI: ACTIVE selected-id=2` (Xbox)
persisted. Thus the last Start-selected DS4's Circle could close the
SDL3 modal but *not* the original game's Joystick page; Xbox B alone
could leave that page. This mismatch is an ownership synchronization bug,
not a bad DS4 Circle mapping.

**Hardware test PASSED for the two-way modal handoff**:
`SpiderMan2k1Fix(20261031-183300).log` shows
`SETUP RETURN PENDING` → `COMMITTED` →
`SDL3-OWNER REBOUND` → `SDL3-OWNER-UI ACTIVE` for both
DS4→Xbox (`selected-id=2`) and Xbox→DS4 (`selected-id=1`).
The original menu receives physical B/Circle as `DIK 0x01` after each
transfer. The user explicitly confirmed **no crash in that test**.
This validates that exact scenario, not all Alt+Tab/hotplug combinations.

**Remaining safeguards — new candidate, physical retest pending**:
A follow-up code audit found that the modal chooser was using the
gameplay `selection_start` API, which returns `0` when Start is pressed
on the **already displayed** pad. After browsing a different profile
with L1/R1, Start on that displayed model therefore could not explicitly
request game ownership upon close. The modal now uses the separate
`selection_start_edge` policy to recognize a genuine release→press
even when its displayed instance does not change. Ordinary gameplay
`selection_start` retains its previous behavior (only report actual
owner changes). The modal consumes *all* sampled edge states and restores
its selected identity on multiple simultaneous Start requests, capture,
or pending close. No game-owned DirectInput changes occur inside the modal.

**Existing behavior validated by Windows**:
- While the modal exists, fresh Start on either controller only changes
  the **displayed SDL3 setup profile**. Never mutate the game's original
  DirectInput device from an open modal; the game Flip is blocked.
- Remember only the last explicit modal **Start** choice (SDL instance ID
  and VID:PID). Merely browsing a model with L1/R1 or the Windows
  combo box does not request game ownership.
- Once the modal is destroyed, game focus has been safely restored and
  no WM_QUIT/foreign focus takeover occurred, queue that explicit
  Start choice **for the next foreground game Flip**. Before applying,
  verify the SDL instance is still connected and its VID:PID is uniquely
  represented among opened mapped devices. If stale/ambiguous, refuse
  and preserve the old game owner.
- The existing game-owned native reconnection code, running after the
  SDL3 selection on the game thread, alone performs the subsequent
  DirectInput detach/rebind under its existing single-owner safeguards.
  No extra COM calls, hooks or keyboard injection were introduced.
- Previously held Start input is disarmed on modal exit and can never
  produce a duplicate spontaneous game takeover.
- Modal Circle/B closes only the SDL3 window, **then a fresh physical
  Circle/B on the newly selected pad** should back out of the original
  game's Joystick page after native ownership reconciliation.
- Logs: `SDL3-PROBE: SETUP RETURN explicit modal Start owner handoff PENDING`,
  then `SDL3-PROBE: SETUP RETURN COMMITTED fresh modal Start ...` and
  `SDL3-OWNER: REBOUND` (or `REFUSED`). A plain L1/R1 browse or a
  closed window with external foreground must not initiate transfer.

**Hardware regression matrix**: Xbox selected in original game via Start →
open modal (Xbox visible) → fresh DS4 Start (DS4 visible) → Circle closes
modal → **new** DS4 Circle closes original game's Joystick menu without
touching Xbox. **The preceding two-way Start handoffs now passed on
Windows.** Remaining matrix: browse by L1/R1 without Start (must NOT
change game owner), then press Start on the **currently displayed**
profile (must explicitly request it); test both pads pressing Start at
the same time (must not pick an arbitrary winner), hold Start while
closing, disconnect while the modal is open, repeat open/close,
Alt+Tab out and back, custom per-model bindings surviving restart,
RT→Swing on Xbox in 3D, and the game after an unplug/replug.
Pass is required before merging this draft.

**Crash follow-up**: the previous reported crash after
closing the modal did not reproduce in the successful 18:33 Windows
handoff test. The earlier crash had no recorded exception address in
`SpiderMan2k1Fix(20261031-175547).log`. Do not claim it fixed unless
physical retest confirms it; collect Windows Application Error Event 1000
and an exception dump on reproduction. Never merge this draft solely
on CI results.

## Multiple different gamepads — one verified native 3D owner (draft)

The original experimental live 3D mapping incorrectly required **exactly
one** SDL-mapped controller connected, even when SDL3 had deliberately
selected one Sony DS4 and the native DirectInput8 owner verified that device
while a different-model Xbox gamepad was merely a spectator.

The revised policy accepts **several distinct VID:PID models** connected
at the same time, but only one selected, verified game-owned native
controller. A Sony DS4 live 3D profile is permitted when:

- `ExperimentalSDL3GameplayBindings=1` and all existing SDL3/native
  owner opt-ins are enabled;
- the selected SDL3 gamepad is the Sony DS4 instance, and **exactly one
  opened SDL-mapped gamepad has that VID:PID**;
- `SpiderNativeReconnect_SDL3SelectionReady` confirms the currently
  game-owned DirectInput device has that selected model's VID:PID;
- the seven-action source native table is canonical; the saved profile
  contains only physically verified Sony buttons and R2;
- the game has foreground 3D ownership, original FFB effect is absent,
  and all physical controls have returned to neutral.

On another controller's **fresh Start** selection, the live DS4 RAM
mapping is restored **before** native DirectInput handoff. Live 3D
remapping is still DS4-only; an Xbox controller with its own native
profile uses the original native mapping and does **not** inherit DS4
bindings. When two controllers share the same VID:PID, the selected
identity is **ambiguous**: fail closed, do not map or claim the game-owned
device on product ID alone. The keyboard remains available.

**This is a source- and CI-tested candidate, not hardware acceptance.**
On Windows 11, physically connect one Sony DS4 plus one differently
identified Xbox360-compatible gamepad, select DS4 with fresh Start,
enter 3D, verify Jump/Punch remaps and Pause; select Xbox with Start,
verify no Sony action leaks; switch back to DS4, repeat, then unplug
either pad. Verify immediate restore on handoff, no stuck input, and
return to 2D. Repeat under Wine. Do not merge before these tests.

## SDL3 3D L2 / D-pad raw-input correlation test (experimental, read-only)

The next native gameplay migration step requires evidence of Sony DS4 L2
and D-pad physical DirectInput representation. Guessing `rgbButtons[6]`
or treating the D-pad's POV angle as a regular game button would corrupt
an otherwise valid seven-action mapping. These controls remain **unsupported
for live 3D remapping**, and the program still refuses any profile that
assigns them to a core action.

To collect non-mutating evidence, enable `[Debug] SDL3GameplayTrace=1`
with the existing `ExperimentalSDL3Probe=1` and
`ExperimentalSDL3NativeOwner=1`. In normal foreground 3D gameplay,
press and release **L2**, then **D-pad Up, Down, Left and Right**, each
separately; afterwards press **R2** as a control sample. The additional
`SDL3-3D-TRACE` lines correlate the selected SDL3 button mask and
normalized LT/RT values with the **separate, nonexclusive** DirectInput
UI handle's raw 16-button mask, `lZ`, sliders and POV (up to 64 lines,
neutral-first/foreground only). Do not press several buttons together.

This diagnostic **does not** call `Acquire`, poll the game's original
DirectInput COM device, write mappings, synthesize keyboard events,
or claim the secondary DirectInput view is identical to the native
game-owned reader. It is only a prerequisite for deciding whether a
particular physical DS4 L2 button index can safely be supported. D-pad
POV controls will require a separate original input-reader design, not
a fabricated `rgbButtons` index.

During a custom Alt+Tab PAUSED screen *before* SDL3's first gamepad
selection, a secondary foreground-only DirectInput device can become
`DIERR_INPUTLOST`. If PAUSED detects this, the next `PauseWake_End`
now retires that stale **fix-owned** device once; the next regular
foreground Flip performs fresh detection without another failed
`GetDeviceState` call. The original game's device is untouched.


## 1 November 2026 — next Xbox 360 physical gameplay test: no code changes

The subsequent mouse-removal draft PR #90 is based directly on this
native-owner PR #80 and includes all its experimental Xbox logic, but
**not** separate PR #88's Left Alt latch repair. A new successful
Wine 11.0 runtime log `SpiderMan2k1Fix(20261101-091342).log`
(`v1.1.0-5cd333b`, exact PR #90 GitHub temporary test merge)
provides this baseline:

- Xbox 360-compatible `VID:PID=045E:028E` is enumerated by
  XInput, DirectInput8 and SDL3 at lines **29–31**, **120–141**
  and **434–445**; the native game-owned DirectInput identity is
  verified and the Xbox seven-action profile was applied in RAM.
- The single selected Xbox remains recoverable by the UI/native
  owner after **three actual external focus pauses**, with
  rediscovery at lines **671–675**, **719–723**, **766–770**.
- Audio recovery succeeded **9/9**, **9/9**, **10/10** after
  focus pauses of about **4.9 s, 267 s and 5.4 s**; game 3D
  returned normally to menu. This proves **focus stability**
  with Xbox present, not physical A/B/X/Y, sticks, D-pad,
  LT/RT or customized profile correctness.
- A single `GAMEPAD-XBOX360: TRIGGER-PROBE #1` baseline
  appears (lines **543, 676, 724, 771**) without a trigger
  transition. No `SDL3-3D-TRACE` records appear, because
  the **opt-in `[Debug] SDL3GameplayTrace=1` was absent/off**.
  Xbox physical face-button translation and RT Swing thus
  remain unverified *in this run*. Do not infer a button
  press from enumeration or a neutral-axis sample.

### Next read-only diagnostic run — same DLL is sufficient

Keep the existing experimental options and *verified* Xbox-only
controller selected. Do **not** remove or change gamepad profiles.
Add or change **within the existing INI sections**:

```ini
[Debug]
Log=1
SDL3GameplayTrace=1
Xbox360TriggerProbe=1
```

The source permits the read-only 3D trace only when
`[Input] ExperimentalSDL3Probe=1` **and**
`ExperimentalSDL3NativeOwner=1`; both were requested
in the 09:13 startup log, so **no new runtime patch/build is
needed for this diagnostic on the same DLL**. Verify
`SDL3-3D-TRACE: requested` at startup. The trace produces
at most **64** selected-pad edge/heartbeat samples per
process, while the existing independent Xbox trigger probe
allows at most **48** significant raw axis samples; both
observe the already-read secondary DirectInput and SDL3
device states, **never the game's original buffered input**.

Enter **playable** 3D, keep the Xbox the sole selected
gamepad, and observe actual in-game effects during distinct
physical presses separated by complete releases:
1. A, B, X, Y: independently log Jump, Kick, Punch, Web or
   the user's saved **customized action** (do not assume
   factory bindings).
2. LB and RB, then Back and Start, each separately. Verify
   Pause vs Xbox controller selection/owner handoff and
   absence of unintended gameplay actions.
3. D-pad Up/Down/Left/Right, then left and right sticks
   independently, followed by L3 and R3, avoiding accidental
   stick clicks during analog movement.
4. RT fully press/release, then LT fully press/release:
   correlate `SDL3-3D-TRACE` normalized `RT/LT` and
   `DI-buttons/lZ`, and `TRIGGER-PROBE` shared-axis
   changes. Specifically inspect whether **RT actually
   initiates/holds Swing** and whether LT leaves Swing
   inactive. The existing `RT-SWING DOWN/UP` markers, if
   present, indicate the key bridge, **not by themselves
   proof of a visible swing**.
5. While in 3D, Alt+Tab once and retest A, RT and Pause
   after manually resuming. Return to the main menu;
   record any wrong action or automatically restarted game.

Avoid excessive rapid presses because the diagnostic has
a fixed sampling cap. Include both the complete
`SpiderMan2k1Fix(...).log` and the actual physical-to-game
action observations. If the supported input or focus is
lost, stop the test, restore the previous DLL and INI.
Afterwards set `SDL3GameplayTrace=0` and
`Xbox360TriggerProbe=0` to avoid continued diagnostics.

**Status:** diagnostic preparation only, no game-code changes.
The next test should be recorded in the Xbox/SDL3 PR #80,
**not** in the mouse-only PR #90, whose Wine acceptance is
already complete. All relevant PRs remain **DRAFT and
unmerged**.
