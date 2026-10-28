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
