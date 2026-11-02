# SDL3 validated panel + opt-in 3D integration (draft PR #98)

Status: **EXPERIMENTAL / OFF BY DEFAULT / NOT VALIDATED IN 3D ON HARDWARE.**

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

Never promote the flag to default or merge this draft into `main`
until physical PS3 + DS4 and Windows 11/Proton checks are accepted.

legluondunet assisted by ChatGPT (OpenAI)
