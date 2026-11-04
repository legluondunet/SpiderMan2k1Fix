# SDL3 universal — compact hardware acceptance (PR #98)

**Current status (4 November 2026):** opt-in / draft, **not yet fully accepted**.
The PS3 (054C:0268) and DS4 (054C:09CC) already passed separate
GE-Proton tests for mapped 3D actions, remapping, Pause, selected cutscenes,
manual Start-to-Start switches, controller-only wake, and held-direction
focus recovery (with a known 1–2-step residual impulse). Do not re-run
these as separate standalone tests.

## Minimum remaining hardware work: one combined PS3 + DS4 run

**One game launch, one full log, four consecutive checkpoints.** Use the
*latest passing PR #98 artifact*, not a historical DLL, in a **separate
game copy** with a verified original `SpideyPC.exe`. Back up the INI
and any per-model profiles before changing them. Attach both controllers
before starting. Set:

```ini
[Input]
ExperimentalSDL3Probe=1
ExperimentalSDL3ControllerSetup=1
ExperimentalSDL3UniversalGameplay=1
```

Universal 3D gameplay remains **OFF by default** for everyone else.

### Checkpoint 1 — explicit owner, setup, remapping, menus and media

1. Start with **both pads neutral** (sticks centered, no trigger/button).
   Before pressing Start, neither a passive first controller nor a
   post-setup auto-selection must claim universal 3D gameplay. Press
   **PS3 physical Start** once, release all controls, and verify
   `SDL3-UNIVERSAL: START selected generic id=...` then
   `SDL3-UNIVERSAL: ACTIVE` for **054C:0268**.
2. Open the SDL3 Joystick Configuration panel. Confirm **one navigable
   action selection**, no second moving highlight and no unexpected
   binding capture. Bind Punch to Circle; verify the expected swap
   of Kick to Square in 3D. Check that Circle still means **Back**
   in the original menus/Pause. Restore defaults after the exercise
   (avoid accumulating test profile modifications).
3. Within the **same** normal playthrough, navigate a main/submenu
   with D-pad or stick and Cross/Circle, start a level, skip **one**
   permitted Bink clip with one fresh press, skip a permitted scripted
   3D cutscene, open/close original Pause and exercise several distinct
   3D moves/actions. If an **Enter/continue still image** occurs, attempt
   the same deliberate new press and record the *visible* result.
   A missing still-image encounter is **NOT TESTED**, not PASS.
   Check for double navigation, accidental double video skip, injected
   input bleeding between contexts or loss of native keyboard access.

### Checkpoint 2 — diagonal held input and focus safety

4. In safe 3D gameplay hold a **PS3 diagonal**, Alt+Tab to Firefox,
   **release the stick while Firefox is foreground**, then return
   and explicitly wake with PS3 Cross. Spider-Man must stop after the
   previously documented maximum short 1–2-step impulse; an
   indefinitely held direction is **FAIL**. Confirm Firefox's keys
   are not intercepted, the game's `PAUSED` gate remains effective,
   and fresh intentional movement still works. Do not accept a
   synthetic-key SendInput success log in place of visible gameplay.

### Checkpoint 3 — physical PS3 → DS4 → PS3 handoff, no auto-takeover

5. Release all controls and **unplug the selected PS3**. Check
   `SDL3-UNIVERSAL: EXIT reason=no selected mapped controller`
   (or a specific equivalent focus/disconnect exit) and that the
   native game-owned DirectInput pointer is restored. The remaining
   DS4 must **NOT silently take over** or inherit held keys.
6. With DS4 neutral, press its physical **Start** once. Confirm
   `SDL3-UNIVERSAL: START selected generic id=...` then
   `ACTIVE ... model=054C:09CC`. Test a short 3D move/action, menu
   Pause and a second held-direction Alt+Tab (use a different
   direction to checkpoint 2). Verify manual wake, no stuck motion.
7. Reconnect the PS3 as a **new SDL3 instance**. The current DS4
   ownership must not change just because PS3 reappeared. After
   centering PS3, press its **Start** and confirm a new explicit
   PS3 `ACTIVE` owner, usable 3D action and one menu confirmation.
   Switching ownership must never leave duplicate input or a stale
   keyboard/DirectInput owner.

### Checkpoint 4 — end-of-run result

8. Exit normally, send **one complete `SpiderMan2k1Fix(...).log`**
   from this run, and report only these four outcomes (PASS / FAIL /
   NOT TESTED): **context flow**, **PS3 diagonal focus**, **PS3→DS4→PS3
   hotplug**, **DS4 diagonal focus**. Mention specifically whether a
   still/Enter screen occurred, and whether there were residual
   steps after either focus return. No separate logs or screenshots
   are needed unless one checkpoint fails or a visual defect cannot
   be represented in the log.

**Stop conditions:** crash, infinite movement, blocked controls,
double activation/skip, input sent while Firefox is foreground, or
failure to restore game-owned DirectInput on disconnect. Do not try
the next checkpoint in that state: save the log, disable
`ExperimentalSDL3UniversalGameplay`, restart the test copy.

## Conditional second run — only if a third SDL3 device is available

A PS3/DS4 pass proves **two models**, not universal support. If a
third-party or initially-unmapped joystick is available, run **one
additional integrated session**: recognize or calibrate (all 20 raw
steps if unmapped), save its GUID mapping and model profile, restart
to verify persistence, explicitly select it, then traverse menu,
skippable Bink, scripted cutscene, still/Enter (if encountered), 3D
movement/actions, Pause and Alt+Tab. Check no automatic owner claim,
duplicate input or stuck movement. If the controller has zero/ambiguous
VID:PID or remains raw and the universal 3D bridge refuses it, record
**unsupported / NOT PASSED**; the existing PR #98 intentionally does
not include all of PR #94's raw GUID support. Never assert universal
compatibility from PS3/DS4 alone.

## Deferred platform gate

Native Windows 11 hardware testing remains **deferred** until the
maintainer has access to Windows. CI PE compilation is not a runtime
hardware test. Wayland/XWayland is likewise not accepted as equivalent
to native X11. These are **separate platform gates**, not additional
GE-Proton retests to repeat now.

## Trace signals for review

- Selection / safety: `SDL3-UNIVERSAL: START selected generic`,
  `SDL3-UNIVERSAL: ACTIVE`, `SDL3-UNIVERSAL: EXIT`,
  `SDL3-PROBE: detached`, native DirectInput pointer `RESTORED`.
- Focus / held direction: `FOCUS-X11: host-focus-state`,
  `FOCUS-PAUSE`, `FOCUS-DIR`,
  `FOCUS-KEYUP`, `SendInput KEYDOWN/KEYUP`.
- These traces support review but **do not substitute for the
  player's observed buttons, menu behavior and motion**.

The older long-form
[`SDL3_3D_INTEGRATION_TEST.md`](SDL3_3D_INTEGRATION_TEST.md)
and [`SDL3_UNIVERSAL_TEST_PROTOCOL.md`](SDL3_UNIVERSAL_TEST_PROTOCOL.md)
remain historical diagnostic references, not additional mandatory
test sequences.

legluondunet assisted by ChatGPT (OpenAI)
