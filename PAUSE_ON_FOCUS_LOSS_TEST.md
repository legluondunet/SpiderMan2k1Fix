# Windows 11 persistent pause prototype (experimental)

Branch: `experiment/win11-manual-persistent-pause-20261021`

This new experiment changes the old focus-loss presentation-loop pause: it
latches the paused state, and regaining focus does NOT resume the game.
Only a new press of the physical Pause/Break key while the game is foreground
can resume. Pressing it while the game is in background has no effect.
The same key can request manual pause while the game is active.

The experiment is limited to the DirectDraw Flip presentation loop: it does
NOT yet guarantee suspension of Bink video or sound, and it does not draw a
PAUSE banner. Do not treat this as a complete game pause.

## Setup on native Windows 11

1. Download the Windows x86 artifact from this branch's CI run if available.
2. Back up the existing DLLs. Install the new DLLs alongside SpideyPC.exe.
3. In SpiderMan2k1Fix.ini set:

```ini
[Debug]
Log=1
[Input]
PauseOnFocusLoss=1
ExperimentalAltTabReacquire=0
ExperimentalFocusTrace=0
RestoreCursorOnFocusLoss=1
```

4. Launch the game and press Pause/Break during gameplay. It should stop at
   the next DirectDraw Flip and stay stopped. Press Pause/Break again to resume.
5. Repeat with Alt+Tab during gameplay. Returning focus must NOT resume the
   game; press Pause/Break to resume.
6. Test menus and cutscenes separately, noting that video/audio may still
   continue in this first experiment.
7. Repeat Alt+Tab at least 20 times; check input recovery and crash behavior.
8. Quit normally, keep SpiderMan2k1Fix.log and note the game's exit code.

### Diagnostics

Look for `FOCUS-PAUSE: focus lost; pause latched until manual resume`,
`FOCUS-PAUSE: waiting for foreground and explicit Pause key`, and
`FOCUS-PAUSE: manual resume requested` in SpiderMan2k1Fix.log.

This prototype must not be merged until real Windows 11 tests are validated.

---

legluondunet assisted by ChatGPT (OpenAI)
