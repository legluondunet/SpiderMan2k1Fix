# SDL3 — native Save Game menu confirmation (experimental)

User observation, 2026-11-08: South/X on the selected controller was visible
in the SDL3 probe on the retail 2D **SAVE GAME** screen but did not confirm.
The physical keyboard **Enter** key confirmed successfully.

The supplied log starts with `v2.0.0-rc.1-11cdbdc`, so it does NOT
establish that the newer duplicate-model PR #137 was being tested. The log
ends on a South edge and lacks the synthetic menu pulse result; a precise
cause is not proven from that log alone.

## Hypothesis and isolated trial

The universal SDL3 bridge sends a short `DIK_RETURN` `SendInput` pulse
after detecting `MenuConfirm`, then releases it in `BeforeUI` on the
next Flip, potentially before the original game polls DirectInput.

For **2D menu Confirm only**, this trial holds the injected Return through
one additional original-game Flip before KEYUP. A context change, loss
of focus, controller/owner loss, shutdown and the existing Bink-4 safety
boundary still release the key immediately. Failed KEYUP remains retryable.
No changes to the physical keyboard, Pause, movie skip, 3D or GAME OVER.

Bounded `SDL3-MENU-CONFIRM` log lines distinguish unarmed input, accepted
KEYDOWN, the one-Flip delay, accepted/rejected KEYUP and context change.
The OS accepting a `SendInput` request is **not** proof the game received it.

## Acceptance

- [ ] Use the PR test artifact, NOT the prior v2.0.0 RC. Check the exact
      build/commit prefix printed in `SpiderMan2k1Fix.log`.
- [ ] Save Game: navigate with a controller, press X/Cross/A to confirm,
      release it, and verify that the game actually saved.
- [ ] Reopen Save Game and verify a held confirm does not activate
      multiple slots and that Back/Circle still cancels.
- [ ] Test 2D main menus, opening Bink movie skip, native 3D Pause,
      GAME OVER and normal gameplay for accidental Enter or stuck input.
- [ ] Alt+Tab while Save Game is open; verify no stuck keyboard Enter.
- [ ] Run on Wine/Proton then Windows 11. Attach the log if a discrepancy
      remains; look for `SDL3-MENU-CONFIRM:`.

This PR is independent from duplicate-instance PR #137 and the v2.0 RC.
Leave all GitHub issues unchanged.

legluondunet assisted by ChatGPT (OpenAI)
