# SDL3 and original Left Alt recovery: default-on staging plan

**Status: PR #92, DRAFT, not released, no main merge.** This document
describes the new integration built on top of the current `main` release,
with the tested PR #91 combined runtime as its second merge parent.

## What changes from the opt-in test binary?

All these keys are now generated with `=1` and have **1 as their
runtime fallback when absent**. A user-specified `=0` remains an
explicit opt-out, and cannot be silently overwritten.

```ini
[Input]
ExperimentalSDL3Probe=1
ExperimentalSDL3NativeOwner=1
ExperimentalSDL3ControllerSetup=1
ExperimentalSDL3ContextBridge=1
ExperimentalSDL3GameplayBindings=1
ExperimentalSDL3XboxRTSwing=1
ExperimentalNativeJoystickUnplugGuard=1
ExperimentalNativeJoystickReconnect=1
ExperimentalNativeJoystickMappingSwap=1
ExperimentalAltLatchRecovery=1
```

- **SDL3 Probe:** discovers mapped pads only when the official x86
  `SDL3.dll` exists, matching original device identity. An unknown,
  unverified, ambiguous or disconnected controller does not gain
  game-owned input ownership automatically.
- **Native owner:** still depends on exact original EXE/memory
  verification and the three native safety prerequisites
  (unplug-guard, reconnect and in-memory mapping swap).
  If even one prerequisite is explicitly disabled or refuses
  activation, the SDL3 native-owner handoff fails closed.
- **Gamepad menu/Bink/UI and 3D bindings:** supported original Sony
  DS4 and Xbox 360-compatible controllers retain strict identity,
  neutral-state, mapping and game-state gates. Unsupported custom
  button semantics cannot modify game-owned native mappings.
  The Xbox RT Swing path retains its physical trigger down/up
  guards; no research-only generic DS3/Steam Deck mapper is enabled.
- **Original Left Alt recovery:** still only checks one verified
  native game-owned byte after confirmed external loss and
  foreground return, and only after six stable release frames,
  with both OS and original game's modifier states clear. No
  keyboard injection, external-window focus steal or user-key
  replacement is added.
- **Not promoted:** `[Debug] Log=1`, controller trigger/trace
  probes, read-only reverse-engineering experiments, legacy
  `ControllerPS1Preview` and `ControllerXbox360Preview`, and
  independent Sony audio-output routing PR #86.

## Upgrading an existing INI: important distinction

The new defaults apply to *missing* keys, and to newly generated
`SpiderMan2k1Fix.ini` files. **An older INI may already have
`ExperimentalNativeJoystickUnplugGuard=0`,
`ExperimentalNativeJoystickReconnect=0`, or
`ExperimentalNativeJoystickMappingSwap=0`** because
an earlier version generated them as off. Those **saved zeros
take precedence over the new default 1** and may intentionally
keep SDL3 native control disabled. This avoids silently
overwriting user configuration.

To enable the new defaults on an older installation, with the game
closed, back up the INI and change those explicit `=0` values
to `=1`, or remove the related keys so the new runtime fallback
applies. **Edit each existing key rather than appending duplicates**:
a prior Windows test caused correct INI repair after a second
obsolete `MouseEnabled` key was accidentally appended.
Never edit the 504-character `[Internal] RawHex`.

For a deliberate opt-out, set `ExperimentalSDL3Probe=0` to
disable the SDL3 device-selection path, or
`ExperimentalAltLatchRecovery=0` for the original Left Alt
repair alone. More granular `=0` flags remain accepted, but
must be tested together with the safety dependency chain.
The keyboard remains usable without gamepads.

## Mandatory new physical smoke test before release

**This is new behavior**. Previous 20/20 Windows 11 checks were
performed with manually enabled opt-in settings. They demonstrate
the runtime implementation, **not** the fresh-INI default-on
activation after this change.

1. Install **both DLLs from the same PR #92 artifact** and
   the bundled Windows x86 `SDL3.dll`. Back up the current
   INI. For a *fresh factory-default trial only*, temporarily
   move the existing INI aside while the game is closed.
2. Run on **Windows 11 native**, with one supported DS4 and/or
   Xbox 360. Confirm the new factory INI contains all ten
   `=1` keys, no duplicate keys, and the log records verified
   original DirectInput ownership, permanent in-game mouse OFF,
   and **`ALT-LATCH: ARMED`**. Do not treat lack of
   `REPAIRED` as failure when Alt is naturally cleared.
3. Test menu/Bink keyboard and controller navigation, 3D
   gamepad actions (including **Xbox RT Swing release**),
   native Pause, save/quit return-to-menu and several Alt+Tab
   transitions with active effects audio. Ensure the
   Windows desktop pointer is usable outside the game.
4. Repeat a limited smoke run under **Wine/Proton** with the
   new factory defaults. The custom PAUSED banner may appear
   late/not at all in 3D and is **nonblocking if keyboard,
   gamepad, audio, graphics and stability remain correct**.
5. For an *existing INI*, use the original current file, verify
   that saved `=0` still disables only the desired option,
   and that restoring `=1` re-enables it **without
   automatically erasing custom joystick/volume bindings**.
6. Attach native Windows and Wine logs with the selected
   controller and **installed archive commit/hash**, then
   approve or report any functional regression. A CI PASS
   alone cannot satisfy this physical default-activation test.

**Integration boundary:** keep PR #92 as a draft against
`main`. Do not merge stacked PRs #80/#88/#90/#91 independently
or overwrite already published `RELEASE_NOTES_1.1.0.md`.
Decide release version, defaults and publication only after
the final branch CI, artifact comparison and smoke results.

legluondunet assisted by ChatGPT (OpenAI)
