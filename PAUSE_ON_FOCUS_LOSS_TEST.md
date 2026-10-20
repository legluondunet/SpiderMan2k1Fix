# PR #2 — Optional pause when Spider-Man loses focus

This PR is stacked on the graphics and Windows-stability fixes in
[PR #1](https://github.com/legluondunet/SpiderMan2k1Fix/pull/1).
It changes only the focus-loss pause and its configuration/tests.
It does not reintroduce Direct3D7 cloned-vtable hooks, DPI virtualization,
or the experimental DirectInput8 reacquire hook.

## Configuration

In `SpiderMan2k1Fix.ini`, edit:

```ini
[Input]
ExperimentalAltTabReacquire=0
ExperimentalFocusTrace=0
PauseOnFocusLoss=1
```

The pause is disabled by default for new installations. With the option
enabled, a running game that has previously been foreground waits at the
next DirectDraw `Flip` when the window loses foreground status. The
window message queue continues to be processed so focus can return.
No background thread is suspended, no cursor is forcibly recaptured and
other applications are not activated.

**Limitations:** this is a cooperative presentation-loop pause, not an
engine-wide pause. Independent movie decoding and audio may continue.
The opt-in behavior has been tested on native Windows 11, including
Alt+Tab in menus and gameplay and video skipping after focus returns.
Wine remains **untested** in this revision.

## Repeatable Windows validation

1. Install the two DLLs from this PR's passing Windows x86 CI artifact.
2. Enable `[Debug] Log=1` and `[Input] PauseOnFocusLoss=1`.
3. Start the game without `SPIDERMAN2K1FIX_D3D_INPLACE` or
   `SPIDERMAN2K1FIX_FCLOSE_NULL_GUARD` overrides.
4. Alt+Tab repeatedly while navigating the menu and during gameplay.
5. Verify that the game pauses without focus, resumes with restored
   keyboard/mouse controls, and allows videos to be skipped on return.
6. Quit from the game menu and verify a process exit code of `0`.

Expected logs include `FOCUS-PAUSE: experimental Flip-boundary pause
ENABLED`, `FOCUS-PAUSE: waiting for foreground`, and
`FOCUS-PAUSE: foreground restored or window destroyed`.

This PR must not be merged before explicit approval.

---

legluondunet assisted by ChatGPT (OpenAI)
