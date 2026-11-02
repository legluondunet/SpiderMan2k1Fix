# SDL3 validated panel + opt-in 3D integration (draft PR #98)

Status: **EXPERIMENTAL / OFF BY DEFAULT / PARTIALLY VERIFIED ON PS3 HARDWARE.**

Physical test, November 2, 2026: PS3 `054C:0268` under GE-Proton11-11
successfully controls Spider-Man in 3D, skips videos/cutscenes, and navigates
the native in-game Pause menu (tester log `SpiderMan2k1Fix(20261102-022140).log`).
This does **not** validate custom 3D bindings, focus loss / Alt+Tab,
disconnect / reconnect, a second gamepad or native Windows 11.

This branch is based on the PS3-tested controller setup UI in PR #96.
It imports the keyboard adapter concept and fail-closed input safeguards
from draft PR #95. Neither parent draft is merged to main. These are
separate tracks and PR #98 is the opt-in integrated test branch.

## Scope
- SDL3-mapped gamepad, unique nonzero VID:PID, explicit Start selection.
- PS3 example under GE-Proton: `054C:0268` (often listed in SDL3 as
  `XInput Controller #1`), DS4 only when its existing verified SDL3
  selection is available. Native DirectInput owner is never claimed for
  the unsupported PS3.
- Reads the *existing, saved* per-model SDL3 profile as configured in
  PR #96, including 3D action button swaps and Restore defaults.
- Sends key scans for original game actions and direction keys only.
  SendInput acceptance **does not prove** SpideyPC.exe consumed them.
- The optional raw GUID-based unmanaged-controller support in PR #94
  is not imported. No promises for SDL3-unmapped or zero VID/PID pads.

## Isolated test and rollback
1. **Back up** the full original game directory, `SpiderMan2k1Fix.ini`,
   and the `SpiderMan2k1Fix` per-model profiles. Do not install the draft
   on the main daily-play copy.
2. Use the **Windows x86 artifact from PR #98's latest successful CI**,
   including the bundled SDL3 library, in the test installation only.
3. In the test copy's `SpiderMan2k1Fix.ini`:
   ```ini
   [Input]
   ExperimentalSDL3Probe=1
   ExperimentalSDL3ControllerSetup=1
   ExperimentalSDL3UniversalGameplay=1
   ```
   The generated default of UniversalGameplay remains `0`.
4. Start from a **neutral** PS3 (no buttons, D-pad, trigger or stick held).
   Press physical **Start** once to request the generic selected input
   owner. Expect `SDL3-UNIVERSAL: START selected generic id=...` in the
   log, rather than the old unsupported-native owner refusal.
5. Expect `SDL3-UNIVERSAL: ACTIVE selected-id=...` after verified
   foreground, native keyboard table, original DI owner slot and neutral
   frame. If it instead logs `REFUSED`, do not continue in 3D; collect
   the exact refusal and keep the original adapter.
6. Test **one action at a time** in 3D: left stick, D-pad, Jump, Kick,
   Punch, Web, Aim, Zipline, Swing, Pause. Ensure each custom mapping
   corresponds to the physical control chosen in the SDL3 panel.
   The adapter emits capped `SendInput KEYDOWN/KEYUP action=` diagnostics.
   Cross-check against actual on-screen response.
7. While holding a *non-destructive* action, Alt+Tab away and back.
   Synthetic keys must release and `SDL3-UNIVERSAL: EXIT reason=...`
   should appear; if a non-NULL native DI pointer was borrowed, its
   restoration must be logged. Verify no stuck input or crash.
8. Test the original 2D menu, an intro/Bink skip, reopening setup,
   Cross on Restore defaults, then controller disconnect/reconnect.
   The setup dialog must log `SDL3-UNIVERSAL: setup modal entered`.
9. **Stop immediately** on a crash, stuck keys, double activation,
   broken focus or incomplete owner restore. Set
   `ExperimentalSDL3UniversalGameplay=0` and restart. The old stable
   setup behavior must remain available.

## Next hardware acceptance checks — custom mappings and focus safety

Use the PR #98 artifact and back up `054C-0268.ini` before editing it:

1. Open the SDL3 panel with the PS3, assign **Punch to Circle**.
   Because Circle was previously Kick, the panel should atomically swap
   Kick to **Square**. Save and close the panel.
2. In live 3D gameplay, press Circle: verify **Punch**, not Kick.
   Press Square: verify **Kick**, not Punch. The 2D menu and native
   in-game Pause must **still** use Circle as Back/Cancel.
3. Restore defaults with Cross in the SDL3 panel. Confirm Circle returns
   to **Kick** and Square to **Punch** in 3D. Confirm the change persists
   after quitting/restarting with the same saved model profile.
4. On a test level, while holding a non-destructive direction, switch
   away using Alt+Tab and stay in another window. The controller should
   not keep moving Spider-Man when focus returns. Confirm `EXIT
   reason=focus or window lost` and native DI pointer `RESTORED` in log,
   followed by an intentional neutral frame before reactivation.
5. Unplug the PS3 *after releasing every input*. Confirm the adapter
   logs `EXIT reason=no selected mapped controller` and restores
   native ownership. Reconnect and press **Start** on that same physical
   controller to explicitly select its new SDL3 instance: the adapter
   must not silently select the DS4 or inherit the previous key state.
6. Exercise Bink skip, cutscenes, menu Pause/Resume and return to gameplay
   again after Alt+Tab and replug. Abort on stuck keys or repeated
   navigation; copy the log before changing any further options.

**Regression protection now in source:** an unacknowledged synthetic
KEYUP blocks new gameplay ownership until release succeeds. The bridge also
restores native ownership when a KEYUP fails while waiting for a neutral
frame. Portable policy tests cover Square/Circle action swaps, controller
model defaults and trigger reassignment, but real gameplay confirmation
is still required.

Never promote the flag to default or merge this draft into `main`
until physical PS3 + DS4 and Windows 11/Proton checks are accepted.

legluondunet assisted by ChatGPT (OpenAI)
