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

## 2026-11-09 — native Save Game shell route hypothesis (PR #138 follow-up)

The exact F9 image `scrn0112.png` supplied by the maintainer shows the
**eight-slot SAVE GAME list** (not the YES/NO confirmation popup).
Correlated `SpiderMan2k1Fix(20261108-231804).log` at Flip ~4118:

```
SDL3-GAMEOVER-PROBE: F9 selected=1 ctx=0
renderer(menu=0 3d=1 bink=0 native-pause=0)
engine-mode=00000003 task=00000014 input-active=00000001
pause-ctx=0 menu-ctx=1 dispatcher=00000000 transition=00000000 raw=OK
```

Another physical South edge earlier in the session showed the same
request/task/menu tuple under `ctx=0`. The previous extra-Flip menu pulse
cannot help when `ctx=0` routes Cross to the 3D gameplay action map.

**Experimental correction:** when the exact original PE32 and all seven
read-only retail input/shell scalars are readable and match the observed
tuple for **two consecutive foreground game Flips**, allow the existing
universal keyboard bridge to route `SPIDER_UNIVERSAL_CTX_MENU` even though
the renderer still says 3D. This does not synthesize Return itself; it
uses the existing neutral-first, menu-edge, and OS KEYDOWN/KEYUP logic.
The route check also happens *before* focus-direction replay. Log
`SDL3-NATIVE-SHELL: ACTIVE/INACTIVE` independently of the ordinary
trace budget.

**Boundaries / uncertainty:** `[0x00682948]` is an opaque menu pointer,
not a unique Save Game identity; `[0x0060CFA4]` is a pending transition
request, not a continuous game-mode enum. The full correlation is from
one observed Save Game screenshot and other sampled edges, **not a
source-proven unique menu signature**. Do not use `menu-ctx` alone.
Never override Bink, native Pause or ordinary 2D menu classification.
Invalid scalar reads, owner/focus changes, task/mode/transition changes
return to the old 3D behavior. No hooks, original EXE patch sites or
`SPIDRMAN.DAT` save logic are changed.

Candidate source landmarks in the *partial* original decompilation:
`Shell_AddGameSlots` = `0x0049F220` (draws eight slots),
`Shell_SaveGame` = `0x0049FAF0` (stub, not decoded),
`PCSHELL_CheckTriggers` = `0x0050C180`.
These symbols identify the area but do not prove the confirmation callback
or the exact raw tuple's uniqueness.

**New hardware acceptance:** use build with an `SDL3-NATIVE-SHELL:
ACTIVE` log while showing Save Game. With neutral controls, press
Cross/A once on a slot and then on the separate YES/NO confirmation;
verify the highlighted YES actually saves, no multiple slots activate,
and Back remains functional. Regress 3D Jump, native ESC Pause,
Bink, Game Over, controller handoff, Alt+Tab/KEYUP, Wine and Windows.
Do not merge or claim fixed before hardware confirmation.
