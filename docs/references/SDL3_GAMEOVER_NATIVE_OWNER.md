# GAME OVER: verified native owner and SDL3 controller bridge

**Scope:** original retail Spider-Man PC (2001), SHA-256
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`
(1,507,328 bytes). Disassembled offline from a user-supplied original EXE
using `llvm-objdump --x86-asm-syntax=intel`. No original EXE or proprietary
assets are in this repository. Reproduce with
`python3 ChatGPT/tools/inspect_spidey_gameover_owner.py /path/to/SpideyPC.exe`
on the maintainer's copy.

## Precise original GAME OVER ownership

The previous state heuristic `[0x0060CFA4]==2 && [0x0054A510]==0x14`
worked experimentally on PS3 and EasySMX, but request 2 is NOT a scene ID:
four independently identified instruction sites set it:
`0x004459BD`, `0x00455BE9`, `0x004B28B3`, `0x004BE027`.

The original top-level controller dispatches request 2 through
`0x00455F56` to `0x00455F5D`, writes
`[0x0054A510]=0x14` at `0x00455F5E`, waits for native cleanup, and
calls the **original GAME OVER screen** at this exact site:

```asm
00455F91: CALL 0x0047D830     ; original bytes E8 9A 78 02 00
00455F96: TEST EAX,EAX       ; immediately consumes original return
```

Inside original `0x0047D830`, the routine renders the authentic
`"game over"` string at `0x0047D93A`, invokes the original native menu
input/update functions, and directly polls the native input function
`0x0050C180` at `0x0047D9B8`. The executable's code contains a
single direct relative CALL to `0x0047D830`, at `0x00455F91`.
**That CALL's execution is the ownership marker**, not the pending
transition request or an inferred time/health threshold.

### Preceding 3D overlay (useful for passive correlation)

The original 3D frame at `0x0045599C` calls `0x00440AF0`.
In that renderer's source code, the **same literal `"game over"`**
is drawn at `0x00440BDF` only if:

- The original player/script actor `[0x006A9038]` is non-null.
- The signed original word at actor offset `+0xE2` is non-positive.
- The global phase word `[0x005FB1B0]` equals **1**.

One original death/transition path at `0x004B28A5` checks a
`0x78` elapsed-tick threshold at `[0x006A7F98]`, then clears
`[0x005FB1B0]` at `0x004B28AE` and sets request 2 at
`0x004B28B3`. Three other request-2 sites exist. Therefore the
3D title predicate is strong **visual source evidence**, but it is
neither guaranteed to persist into the later interactive screen nor
suitable as the sole input owner.

## Original-call interception and guarded default-on routing

The SDL3 bridge replaces **only** the original five-byte relative CALL
at `0x00455F91` with an exact-EXE-signature-checked relative CALL into
a small wrapper. That wrapper does not replace or bypass the original
game-owned menu: it invokes `0x0047D830` **exactly once** on the
original game thread, returns its unchanged `EAX`, and tracks whether
the specific original GAME OVER function is currently executing.

`[Input] ExperimentalSDL3GameOverConfirm=1` is now **the default** for
SDL3 universal gameplay, including valid legacy INIs missing the key.
Newly generated INIs write `=1`. **Explicit `=0` disables** this
feature. The name remains for backward compatibility with old INIs.
The optional `ExperimentalSDL3GameOverNativeOwner=1` is a
hook-only diagnostic and is never required for confirmation.

The bridge can send one buffered Enter for a new, isolated
South/Cross/A press only when all of these conditions hold:

- The original `0x0047D830` GAME OVER owner is actively executing
  inside the authentic, exact-signature-verified CALL wrapper.
- The game is foreground; a mapped controller has been explicitly
  Start-selected, neutral-armed, and is still the active owner.
- The original 3D/gameplay renderer is active; native 2D, Pause,
  Bink/video and other known input owners are not.
- The original seven-word state corroboration passes (including
  request 2, task 0x14 and active input). It is **never** accepted on
  its own; a native owner is independently mandatory.
- Six stable game Flips precede a full neutral frame followed by
  one new South press; a held/chorded button or ambiguous axis input
  is not a confirmation.

The button press is consumed so it cannot simultaneously cause
Space/Jump. On focus loss, controller handoff and owner exit, input
routing returns to normal. The original EXE file is never modified
on disk; source signature, relative displacement and memory page
protection failures log `hook REFUSED` and **block any Enter**.
No legacy request-2-only injection survives.

### Hardware evidence and remaining validation

**2026-11-07 GE-Proton11-12 / Xbox 360**, release-trial build
`v1.1.0-29e5b88`: the logged hook installation succeeded, and two
**independent** original GAME OVER owner entries (`09:08:14.621`
and `09:08:31.853`) each preceded exactly one stable six-Flip
candidate and **one accepted Enter pulse** (at `09:08:16.429` and
`09:08:32.733`). Both owners exited normally; the first transition
resumed gameplay/cinematic activity, the second returned to the
main menu. This supports successful RETRY followed by QUIT. The log
alone does not contain selected RETRY/QUIT text: a maintainer's
screen observation remains needed to label both options conclusively.

There were **zero** GAME OVER Enter pulses outside the two native
owner invocations. Regular gamepad South was mapped to Space/Jump,
and directional input continued to function normally in gameplay.
No game crash, invalid-INI repair or unverified hook attempt appeared
in that log.

The default-on integration is based on this successful source-verified
opt-in runtime test, plus regression tests for strict owner checking.
A **fresh default-on build** must still receive one consolidated
hardware smoke test (RETRY, QUIT, movement, Jump, ESC Pause,
focus/Alt+Tab, opt-out `=0`). Native Windows hardware behavior
is not yet verified by the Proton trace. Do not merge until final
review, CI and this final smoke are complete.

The independent `ExperimentalSDL3GameOverOwnerProbe=1` enables
bounded, read-only native-state comparison and may remain disabled
during ordinary play.

legluondunet assisted by ChatGPT (OpenAI)
