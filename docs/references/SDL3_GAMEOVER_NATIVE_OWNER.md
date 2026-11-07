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

## Narrow runtime interception, experimental

PR #110 can replace **only** the original five-byte relative CALL at
`0x00455F91` with an exact-EXE signature-checked relative CALL into a
small wrapper. This wrapper does **not** change the native menu,
write original game-state fields, or bypass the original function:
it invokes `0x0047D830` **exactly once** on the original game thread,
returns its actual `EAX`, and tracks whether that precise original
menu function is currently executing.

A new **disabled-by-default** opt-in
`[Input] ExperimentalSDL3GameOverNativeOwner=1` selects the strict
owner-gated trial. It requires the existing
`ExperimentalSDL3GameOverConfirm=1`, which sends one buffered
Enter for a fresh isolated South/Cross/A press only when:
- The authentic `0x0047D830` wrapper is executing.
- Original game is foreground and the SDL3-mapped controller was
  explicitly Start-selected and neutral-armed.
- Normal 3D/gameplay renderer is active; native 2D/Pause/Bink are not.
- The legacy seven-word candidate guard still matches, then
  six stable Flips plus a fully neutral frame precede a fresh isolated
  confirmation.

The original injection is OFF unless `ExperimentalSDL3GameOverConfirm=1`
**and** `ExperimentalSDL3GameOverNativeOwner=1`. If the second option is
missing, the runtime explicitly logs `SDL3-GAMEOVER-NATIVE: CONFIRM BLOCKED`.
There is no longer any legacy request-2-only Return injection path.
If byte verification, relocation bounds or protection change fails,
the owner hook refuses and **no GAME OVER Return is injected**.
The independent `ExperimentalSDL3GameOverOwnerProbe=1` mode retains
its non-mutating telemetry; it can separately compare native 3D title
presence and screen-owner entry/exit.

### Current acceptance status

The prior opt-in *heuristic* confirmation was positively observed on
EasySMX and PS3 under Proton, and 925 automated tests passed before this
new hook work. Those tests **do not validate** the new source-owner
hook, so it must remain experimental until one fresh runtime test
confirms both RETRY and QUIT, ordinary jumps and native Pause/Alt+Tab,
with no false Enter in game transitions.

The user's 2026-11-07 initial test using build `v1.1.0-1c713de`
(Windows x86 compiled under CI #42346599890) logged two legacy
`SDL3-GAMEOVER-TRIAL: ENTER PULSE` events but **no**
`SDL3-GAMEOVER-NATIVE: ... wrapper installed` or `entered ... owner`
record, because the strict owner flag was not active. This cannot
validate the new call wrapper. The fallback has since been **disabled**;
a correctly configured new build cannot silently repeat that situation.

The next acceptance test should enable the strict owner gate and
existing confirmation trial, with `[Debug] Log=1`. A new
`SDL3-GAMEOVER-NATIVE: entered exact original 0x47D830 owner` log
must precede the GAME OVER candidate and the single Enter pulse.
Only after this exact gated path passes on hardware can it replace the
legacy heuristic and be considered for default-on integration.

legluondunet assisted by ChatGPT (OpenAI)
