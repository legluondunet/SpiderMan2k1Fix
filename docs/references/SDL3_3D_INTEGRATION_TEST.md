# SDL3 validated panel + opt-in 3D integration (draft PR #98)

Status: **EXPERIMENTAL / OFF BY DEFAULT / GE-PROTON HARDWARE TESTED / WINDOWS 11 NATIVE DEFERRED.**

Physical tests November 2–3, 2026 under Manjaro / GE-Proton11-11 and
GE-Proton11-12 validated PS3 `054C:0268` and DS4 `054C:09CC` in
3D, saved remaps, cutscene skip, original Pause-menu navigation, manual
Start handoffs with both pads attached, gamepad-only Alt+Tab resume,
and a held-direction Alt+Tab recovery which **stops** movement after
a documented short 1–2-step impulse. The original buffered keyboard
latch's held state was measured returning to neutral on PS3.
See detailed sections and logs below.

**Not yet validated**: physical hot-unplug-to-other-pad Start handoff
on the latest fix in one full trace, arbitrary third-party gamepads,
all diagonal/stick directions under focus changes, or native Windows 11.
The maintainer has deferred native Windows 11 hardware testing until a
Windows 11 computer is available; **no Windows result is implied**.

This branch is based on the PS3-tested controller setup UI in PR #96.
It imports the keyboard adapter concept and fail-closed input safeguards
from draft PR #95. Neither parent draft is merged to main. These are
separate tracks and PR #98 is the opt-in integrated test branch.

## GE-Proton focus probe: 1 of 2 Alt+Tab attempts paused (2026-11-03)

Tester used `v1.1.0-5869a47` on GE-Proton11-12 and reports
**one successful automatic focus pause out of two requested trials**
(normal 3D then original native Pause). Relevant log:
`SpiderMan2k1Fix(20261103-201523).log`.

**Confirmed first focus loss and successful wake**:
- `SDL3-UNIVERSAL: EXIT reason=focus or window lost` restores original
  DirectInput joystick, and `INPUT-FOCUS: flip-background` observes
  foreign HWND `00010020`, owned by `explorer.exe` PID 252.
- `FOCUS-PROBE: external-candidate-first-flip` followed by
  `external-candidate-confirmed-after-150ms ... elapsed-ms=166`
  confirms that the 150 ms filter passed.
- Game displays PAUSED scene, stops its media, and waits for physical
  PS3 SDL3 press/release `buttons=0x001`. Game resumes successfully.
- `Flip-gap-ms=5137` after returning is expected: the custom focus
  pause intentionally blocks the next Flip until wake.
- `candidate-cancelled-return-to-game ... elapsed-ms=5303` is a
  misleading diagnostic, not a second failure: the already-confirmed
  foreground candidate timestamp survived the blocking wake loop.
  New code resets it **after** logging the confirmed owner and elapsed
  duration. Pause decisions are unchanged.

**The second Alt+Tab is not observed**: no second
`INPUT-FOCUS: flip-background`, external foreground candidate,
native-joystick EXIT due to focus, or auto PAUSED. The extensive native
Pause-menu rendering and advancing Flip counts show that original
native Pause UI was active, but do not identify exactly when or how
Wine handled the second Alt+Tab.

New bounded **read-only native-Pause telemetry** samples
`GetForegroundWindow`, foreground PID, global
`GetGUIThreadInfo(0)` active/focus, and game-thread
`GetGUIThreadInfo(game_tid)` active/focus every >=800 ms while
the original Pause screen is present, up to 120 samples.
It runs on the original Flip thread and does not install hooks,
activate windows, inject input or change any pause/selection logic.

Next hardware validation: run Alt+Tab to an external application for
at least 2 seconds in steady 3D, then repeat with the original Pause
menu displayed. Record outcomes separately and attach the new
SpiderMan2k1Fix log. Compare the `FOCUS-PROBE:
native-pause-snapshot` fields and `Flip-gap-ms` to find
whether Wine leaves the game HWND foreground, reports another GUI
active/focus owner, or suppresses Flip calls. Windows 11 remains
deferred; the PR remains draft/unmerged.

## Focus pause regression follow-up: repeated Alt+Tab does not pause (2026-11-03)

The tester reports **none** of the recommended subsequent focus-loss
checks successfully activated Spider-Man's frozen `PAUSED` overlay.
The only new attached log is **`CrimsonSkiesFix.log`**, which clearly
starts with `CrimsonSkiesFix 0.1.88-5b39d344` and belongs to the
**separate Crimson Skies project**, so it provides **no evidence** for
the new Spider-Man focus checks. Do not misattribute it to this PR.
Earlier Spider-Man log `SpiderMan2k1Fix(20261103-194902).log`
already showed a single valid focus pause and wake, followed by the
tester-observed missed final Alt+Tab.

The original focus-pause logic is still **unchanged**: the game Flip
thread accepts only different-process foreground sustained >=150 ms,
then requires explicit keyboard/gamepad wake. Added bounded
`FOCUS-PROBE` **READ-ONLY** instrumentation (24 candidate reports,
12 inter-Flip gap reports) to clarify if a foreground transition was
brief/filtered or whether game Flip callbacks had a long gap.

Diagnostic messages to look for:
- `external-candidate-first-flip` / `...confirmed-after-150ms`:
  a different process foreground was observed on one or more Flips
- `candidate-cancelled-return-to-game`: game returned before
  the 150 ms debounce completed
- `candidate-cancelled-null-or-own-process`: focus candidate lost
  continuity because Win32 foreground was NULL or same-process
- `Flip-gap-ms=`: a >=750 ms interval with no intercepted Flip;
  this alone does **not** prove focus loss, and may occur during loading

If there is no candidate log or Flip-gap during a user-visible focus
loss, this instrumentation is insufficient by design; investigate
game event/message focus reporting separately before changing the
pause behavior. No new native Win32 hooks, timer threads,
`SendInput`, DirectInput interception, pause-policy changes or
foreground activation are introduced.

**Await** an actual `SpiderMan2k1Fix(YYYYMMDD-...).log`
from the latest focus-probe build before declaring a root cause
or considering a correction. Windows 11 testing remains deferred.

## New GE-Proton smoke test — owner handoffs pass, second Alt+Tab unconfirmed (2026-11-03)

Tester reports no instability when returning to the game and an orderly
visual shutdown, but their **last attempted Alt+Tab did not trigger a
focus pause despite a perceived focus loss**. Log
`SpiderMan2k1Fix(20261103-194902).log`,
runtime `v1.1.0-14c5685`, Manjaro / GE-Proton11-12.

Confirmed by this log:
- Both PS4 `054C:09CC` (SDL3 instance `1`) and PS3
  `054C:0268` (instance `3`) are attached at startup; no
  `SDL3-PROBE: detached` or new attach events later.
- The universal 3D adapter refuses any gameplay before an explicit
  Start: several `waiting for selected SDL3 mapped pad` reports
  precede `START selected generic id=1`. This is positive evidence
  against passive startup takeover **with both pads connected**.
- Manual ownership transitions **PS4→PS3→PS4→PS3** via fresh
  Start work. Each prior owner exits and the saved original game-owned
  DirectInput pointer is restored before the new SDL3 owner becomes
  ACTIVE.
- One external focus loss is **definitively detected** while PS3 owns
  gameplay. The focus pause is latched, foreground is restored and
  the pause is resumed with a gamepad digital press/release
  (`buttons=0x001`). Native DI joystick is restored for background
  then suspended on foreground re-entry.
- The log contains **no second** `INPUT-FOCUS: flip-background`,
  `FOCUS-PAUSE: confirmed external focus loss` or corresponding
  subsequent `SDL3-UNIVERSAL: EXIT reason=focus or window lost`.
  Therefore the final attempted Alt+Tab cannot be validated from the
  log, and a second focus change cannot be proven by this trace.
- The tail shows a 3D native Pause-menu transition followed by
  return to the 2D menu; **there is no explicit exit-completion
  marker**. Successful shutdown is tester-observed, not proven by
  the file.

The original focus-pause detector in `spiderman2k1fix.c` requires
foreground ownership by a **different process** for at least **150 ms**,
sampled across Flip callbacks. An unobserved/brief focus transition,
one in the same process, or lack of further Flip callbacks could
explain an unlatched pause; none is established in this log.

**Next focused reproducibility test** (no code changes on this report):
during steady 3D gameplay, Alt+Tab to a clearly separate application
and leave that window foreground for **2 seconds**, then return.
Repeat separately in native Pause and main menu, noting exactly which
state fails. Attach logs. If 3D loss remains undetected, add bounded
read-only diagnostics to report foreground candidates and Flip cadence
before changing any pause logic.

Do **not** mark physical disconnect/reconnect accepted: the prior
smoke test with both gamepads attached validates only manual
Start-to-Start selection. Windows 11 native remains deferred.

## Universal explicit Start safety (post-hardware audit)

The native SDL3 selection policy automatically chooses the only open
mapped controller for **legacy read-only/native** purposes. The
experimental universal 3D keyboard bridge must **never** treat this
passive auto-selection as a player claim. It only samples the
`generic_selected_id` assigned after a fresh Start or a validated
controller-setup modal handoff.

Fix (PR #98): `SpiderSDL3Probe_SelectedMappedSample()` rejects
`generic_selected_id==0` and does not fall back to
`SpiderSDL3Probe_SelectedSample()`. A stale, disconnected generic
instance also fails unique active SDL3 instance validation. A second
controller cannot silently take over after the first is unplugged:
it must receive a fresh neutral-armed Start before keyboard injection
or native DI pointer suspension resumes.

This is a fail-closed source-contract fix with portable regression
tests; it needs a dedicated GE-Proton one-pad startup and physical
unplug handoff hardware smoke test after the fix. The change does not
modify the already hardware-confirmed buffered-keyboard focus recovery.

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

## PS3 unplug -> PS4 takeover (reported on GE-Proton11-11)

On 2026-11-02 the user physically validated `v1.1.0-2bf48ff`:
- custom 3D game action remapping works;
- PS3 `054C:0268` unplugged (`SDL3-PROBE: detached id=2`);
- DS4 `054C:09CC` stayed connected and SDL3 successfully observed its
  button inputs (`selected logical gamepad id=1`), but Start could not
  activate the universal 3D bridge;
- the original DirectInput joystick remained stale, generating repeated
  `DIERR_UNPLUGGED` fallback logs; no crash was reported;
- PS3 replugged as new SDL instance `id=3`; Start correctly reactivated
  universal keyboard control and original DI remained suspended.

Source root cause: `selection_sync` passively auto-selects the only
remaining DS4; the legacy change-only `selection_start` then ignores
an explicit Start on that already active ID. Independently, the generic
SDL3 route used by PS3 rejected Sony/Xbox models classified by native
reconnect, while native rebind is intentionally disabled in universal
mode. The experimental universal controller mode cannot consequently
switch to DS4 until the user explicitly starts it.

PR #98 implements **a fresh Start edge even on a logically auto-selected
same ID** in universal mode, and permits all uniquely identified mapped
Sony/Xbox/PS3 devices to use the same single-controller SDL3 keyboard
adapter. Outside universal mode, legacy native Start change-only and
verified native DS4 rebind paths remain unchanged.

Hardware acceptance: install a **new PR #98 Windows x86 CI build** into
a disposable copy, activate universal gameplay, use PS3 then unplug it,
press Start on DS4 only after releasing all DS4 controls, and confirm the
new `SDL3-UNIVERSAL: START selected generic id=...` followed by
`SDL3-UNIVERSAL: ACTIVE ... model=054C:09CC`. Try 3D, menu Pause,
video skip, then press Start on the reattached PS3 to regain it.
Never auto-switch in response to unplug; hold the requested owner
inactive until user confirmation. With two identical VID:PID pads present,
the unique-model guard should refuse takeover.

No console-specific button arrays are used by universal 3D:
`spider_sdl3_profile_default` assigns identical normalized SDL3
south/east/west/north/shoulder/trigger meanings for all nonzero VID:PID
models. A PS3 with different SDL3 mapping, clone firmware, driver or a
duplicate identical VID:PID may require separate mapping/validation.

## Hardware acceptance — held-direction Alt+Tab PS3 and DS4 (2026-11-03)

**PASS with known small residual impulse** on both gamepads under
Manjaro / GE-Proton11-12. This is physical tester feedback, not a
conclusion inferred solely from successful Win32 `SendInput` calls.

- **DS4 `054C:09CC` PASS**: with direction held before Alt+Tab,
  Spider-Man takes one or two steps immediately after resuming, then
  stops without another keypress. Runtime `v1.1.0-6768caa`, log
  `SpiderMan2k1Fix(20261103-182654).log`.
- **PS3 `054C:0268` PASS**: same observed behavior with held stick
  across Alt+Tab, runtime `v1.1.0-b9c6ee6`, log
  `SpiderMan2k1Fix(20261103-185946).log`. PS3 selected as SDL3
  instance `id=3`, and the gamepad-only focus resume succeeded.
- For PS3 RIGHT (`DIK=0xCD`), the **original game-owned buffered
  keyboard latch** read-only probe provides decisive state evidence:
  at focus loss `raw=0x7F held=1`; after all twelve foreground KEYUP
  replays `raw=0x7F held=1`; after the fresh buffered DOWN -> UP,
  `raw=0x00 held=0` for four subsequent Flip samples.
  `FOCUS-DIR fresh keyboard DOWN after 0 wait frames (retry attempts
  0->1; not an Acquire ACK)` and `fresh DOWN/UP completed` also occur.
- The read-only data confirms that *plain repeated KEYUPs were
  insufficient to clear that original keyboard latch* in this run,
  whereas after the deliberate fresh DOWN/UP the latch returned to
  zero. Correlation with the tester's movement stopping supports the
  buffered-keyboard resync approach; exact lower-level input timing
  and the separate engine semantic action latch remain unmeasured.
- **Known side effect**: a brief 1–2-step movement impulse on
  resumption, most plausibly produced by the synthetic fresh DOWN.
  This is accepted as a documented experimental trade-off, **not**
  perfectly motionless Alt+Tab recovery. Do not add unverified native
  latch writes or more keyboard injection merely to eliminate it.

Both modes still require manual controller selection (Start), the
neutral/focus gates and the `ExperimentalSDL3UniversalGameplay=1`
opt-in. The PR is **draft and unmerged**. Windows 11 native, all
directional combinations and broader third-party controller support
remain separate acceptance items.

## Further DS4 focus-resync log — diagnostic, not gameplay proof

Tester uploaded `SpiderMan2k1Fix(20261103-182654).log`,
build `v1.1.0-6768caa` under GE-Proton11-12:

- **DS4**, not PS3, was explicitly selected: SDL3 instance `id=1`,
  `054C:09CC`. The PS3 `id=2` was present but not the input owner.
- `SendInput KEYDOWN action=3 DIK=0xCD` (Right) before focus loss.
- Loss of focus captured exactly one held direction, then temporarily
  restored the original game-owned DirectInput joystick pointer.
- The focus-pause overlay resumed with a physical gamepad button.
  The native joystick pointer was re-suspended.
- All 12 repeated KEYUP calls returned Win32 success.
- The separate keyboard retry-attempt count advanced **2 → 3**.
  The resync emitted a fresh RIGHT KEYDOWN and KEYUP on separate
  Flip frames, then reported completion. This still does not prove
  the game's buffered `GetDeviceData` delivered either event.
- Subsequent in-game Pause menu commands are logged, demonstrating that
  the game stayed responsive. **This trace alone cannot establish
  whether Spider-Man physically stopped running; user feedback on
  the visible result remains necessary.**

New **read-only diagnostic** for the next test build, with no behavioral
keyboard or joystick changes: inspect `SpideyPC.exe`'s original
256-byte keyboard-event latch `0x006B792C..0x006B7A2B` only after
checking the exact retail image base and known x86 reader/write
signatures. Each held scan is logged at focus loss, after the 12
foreground KEYUPs, immediately before fresh DOWN, at the next Flip
before fresh UP, and for four subsequent Flips after fresh UP.

Example:
`SDL3-UNIVERSAL: DI-LATCH stage=post-fresh-up-flip-1 DIK=0xCD raw=0x00 held=0 (original game read-only; action-latch unverified)`.

The `held` field is interpreted using the retail keyboard getter's
low 7 bits. If the state remains nonzero following the release,
investigate DI buffering / reacquisition; if the keyboard latch clears
but Spider-Man continues running, investigate the *separate original
engine action-state latch* instead. These logs do not directly probe
that action-state latch. No memory writes, native COM hooks, or
additional synthetic events are added by this diagnostic.

## Follow-up 2026-11-03: 12 accepted KEYUP replays did NOT fix running

Second physical PS3 test, log `SpiderMan2k1Fix(20261103-180555).log`,
runtime `v1.1.0-fbb705b`, GE-Proton11-12: Spider-Man **still runs**
after Alt+Tab while a direction was held. Exact sequence:

- Before losing focus, normalized SDL3 emitted `KEYDOWN action=2
  DIK=0xCB` (Left) and `KEYDOWN action=1 DIK=0xD0` (Down),
  subsequently releasing Down but leaving **Left held**.
- Focus loss captured exactly one held synthetic key.
- Focus pause and PS3-only Cross wake worked.
- `FOCUS-KEYUP foreground cleanup accepted ... passes-left=11`
  and `... passes-left=0` prove Win32 accepted all 12 release calls;
  **they do not establish original DirectInput buffered reception**.
- `DI8-RETRY ... keyboard=1` was later logged. This counter is sampled
  only at Flip multiples of 120 and increments before the original
  game's Acquire call; it is NOT evidence of the precise Acquire
  moment or successful keyboard reacquisition.
- After the replay, no new SDL3 directional KEYDOWN occurred before
  the player attempted an unrelated action. Therefore the persisting
  movement is consistent with **a stale original-game held-action
  latch**, not a still-held SDL3 stick.

A different experimental correction now queues only the ORIGINAL four
movement scan codes that were actually held at focus loss. It preserves
the harmless foreground KEYUP replay, then waits for the game keyboard
reacquire *attempt count* to advance, or falls back after 24 additional
Flip frames; it allows 3 settling frames, sends a **fresh KEYDOWN on one
Flip and KEYUP on a later Flip**, and never processes another 3D input
until that one buffered press/release has been delivered. This can
create a brief movement impulse. It is limited to this exact selected
controller, original 3D context, and physical neutral. If the
controller, focus, or game context changes, the extra synthesized
direction must be released/cancelled. Each step has bounded
`FOCUS-DIR` logs.

This is still a **hypothesis-based experimental fix**. The original
engine's keyboard buffer and action latch are not directly verified.
It is not a general DirectInput reset API and must not be merged or
made default until successfully exercised on PS3 and DS4 hardware.

Acceptance: run exactly the held-stick Alt+Tab scenario again, then
inspect `FOCUS-DIR captured owned 3D keys=1`,
`FOCUS-DIR fresh keyboard DOWN after ...` and
`FOCUS-DIR fresh DOWN/UP completed ...`. Check if the character stops
WITHOUT a physical keyboard keypress, that directional movement is
normal afterwards, and ordinary menu Pause / gamepad wake still work.
If not, stop patching by speculative SendInput repeats and gather
native buffered-keyboard action-state diagnostics.

## Alt+Tab while holding a stick — PS3 regression (2026-11-03)

Hardware evidence on `v1.1.0-c2579b7` under Manjaro /
GE-Proton11-12, log `SpiderMan2k1Fix(20261103-175210).log`:

- **PASS**: PS3 gamepad-only resume from the automatic focus-pause screen
  (digital press/release without a physical keyboard key).
- **PASS**: Repeat Alt+Tab after manually changing controller with Start
  to the DS4 `054C:09CC`.
- **FAIL**: Alt+Tab while **left stick is physically held** causes
  Spider-Man to continue running after the stick is released and the
  game is resumed.
- Exact trace sequence at 3D: `SDL3-UNIVERSAL: SendInput KEYDOWN
  action=3 DIK=0xCD` (Right), immediately followed by
  `EXIT reason=focus or window lost`. Original native game-owned DI
  pointer is restored. The gamepad wake and reactivation occur
  successfully, but running persists. Win32 accepted the background
  keyboard release, yet **actual original-game DirectInput reception
  was not verified**.
- The preceding working Alt+Tab sequences did not have a fresh
  directional KEYDOWN immediately adjacent to focus loss.

Candidate fix (draft PR #98): snapshot exactly the synthetic key scans
currently held when focus is lost, before `release_all` clears its
mask. Following gamepad/keyboard wake and return to the foreground,
wait until the selected pad is neutral, suspend the original native
joystick source, and send **KEYUP for each previously held scan** across
12 successive Flip frames while suppressing all new actions. This
allows a subsequently reacquired original keyboard device to see a
release even if it missed the original background event. Do not send
KEYDOWN during recovery, never replay outside foreground, and keep
the remaining frame budget if SendInput fails. The replay is one-shot
per focus loss and does not affect ordinary button releases or manual
controller handoff.

Retest in the next PR #98 Windows x86 build:

1. In a safe 3D level, **hold the PS3 left stick right**, Alt+Tab out,
   release it while another application is foreground, return to the
   game, and resume with a new Cross press/release.
2. Observe that Spider-Man **stops** when control resumes and responds
   normally to a fresh intentional direction input.
3. Confirm new log entries `FOCUS-KEYUP deferred foreground replay
   armed scans=1 frames=12` and
   `FOCUS-KEYUP foreground cleanup accepted ... passes-left=0`,
   alongside native DI owner restoration/suspension and ordinary
   focus-pause transitions.
4. Repeat on **PS4**, in another direction, and with a 3D action
   button. Confirm normal Alt+Tab without held input remains unaffected.
5. Stop and attach the new log if Spider-Man still moves without
   input, the release replay reports `retry`, the game crashes,
   or a keyboard key becomes unresponsive after re-entry.

**Not yet hardware-confirmed:** this foreground replay is an
evidence-driven fix, not proof of Wine's exact DirectInput internal
behavior. Keep the universal 3D opt-in default OFF.

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
