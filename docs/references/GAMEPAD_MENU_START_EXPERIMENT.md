# Experimental DualShock / DirectInput gamepad menu and video Start support

**Status: draft PR #73, not merged, not a confirmed fix.** The maintainers
reported that a DualShock 4 works during gameplay and moves menu selection,
but cannot enter menu choices or skip videos.

## What the partial PC decompilation establishes

See the community [`PCInput.cpp`](https://github.com/krystalgamer/spidey-decomp/blob/master/PCInput.cpp),
[`pcdcPad.cpp`](https://github.com/krystalgamer/spidey-decomp/blob/master/pcdcPad.cpp),
and [`ps2gamefmv.cpp`](https://github.com/krystalgamer/spidey-decomp/blob/master/ps2gamefmv.cpp).

- The native controller mappings have fourteen eight-byte pairs
  (`uint32 action_mask, uint32 button_index`), including four directional
  markers, seven gameplay actions and **Start action bit `0x1000`**.
- The default compiled-in `Start` mapping is **button 11** (zero based),
  distinct from gameplay mapping and not exposed in the seven gameplay
  bindings in our generated `[Joystick]` INI section.
- `Pad_Update()` takes those mapped states and calls
  `Pad_Button(&gSControl[0].Start, v3 & 0x1000)`.
- `GameFMV_PlayMovie()` uses `Pad_Update()` and
  `PCSHELL_CheckTriggers(0x7000000,1,1)` once skipping is allowed
  after at least 60 movie-frame-loop iterations.
- **Unresolved:** The community partial decompilation has only a stub for
  `PCSHELL_CheckTriggers`, and we have not verified the retail binary's
  table addresses nor the physical Options/Share button indices reported
  by this specific DS4 under Wine or Windows.

## Failed user test — 27 October 2026, build `v1.1.0-a807941`

The user confirmed **no controller menu confirmation / no movie skip**.
The supplied `SpiderMan2k1Fix(20261027-233002).log` proves that
the supported SHA-256 game launched normally, the experimental PR #73 build
was loaded (the hash suffix `a807941` identifies the PR test merge),
and the virtual `Spidey.cfg` loader returned before rendering movies/menus.

**Critical gap:** that entire log contains **zero** `GAMEPAD-START`
lines. The initial implementation logged nothing when the INI opt-in
was missing or an early executable check failed. Therefore the log
**does not prove** whether `ExperimentalGamepadStart` was set,
whether the game's action tables matched the detector or whether the
requested physical DS4 button number was correct.

**Confirmed ordering fault:** the first implementation called
`SpiderGamepadStart_Init` from `initialize_body()`, **before**
`SpiderCfg_BootstrapGameRead()` invokes the game's original config
loader at VA `0x00515680`. The original loader explicitly restores
joystick assignments as well as keyboard and sound. A Start mapping
modified earlier could be silently overwritten before the interactive
menu. The revised experimental build now attempts its one-shot remap
**after** `SpiderCfg_BootstrapGameRead()`, still before native
`DirectDrawCreateEx`, without additional COM/keyboard hooks.

The revised build logs `GAMEPAD-START: OFF` when the key is missing,
`GAMEPAD-START: REQUESTED` and `SCAN` for configured attempts, an
explicit failure reason for unsupported game layout, and
`GAMEPAD-START: EXPERIMENTAL mapping Start` only if two tables were
modified. The first user's failure is **not yet resolved or game-tested**.

## Experimental opt-in

For a controlled test, keep the game's existing INI. Under `[Input]`
add, **while the game is closed**:

```ini
[Input]
ExperimentalGamepadStart=BUTTON8
```

The example number **8 is provisional**, **not a guaranteed DS4 Share or
Options mapping**. Determine the physical DirectInput button index on the
maintainer's setup (e.g. the original game's joystick configuration screen
using keyboard navigation). Do not guess that other joystick software's
button indices are necessarily identical to the game's.

The native code attempts to remap **only** Start from BUTTON11 to the
chosen `BUTTON0`–`BUTTON31` index. It will refuse any button that
already belongs to one of the seven existing gameplay mappings. For
example, factory `BUTTON9` is occupied by Zipline and will be rejected.
Choose an otherwise unused button. To disable and revert, delete
`ExperimentalGamepadStart` and restart the game.

There is no synthetic Enter/Space key, injected button press, polling
thread, XInput overlay, COM hook or change to the seven original
gameplay mappings. The original native triggered/pressed handling remains
in charge of menus and cinematics.

### Safety gates

The fix is invoked only after the supported `SpideyPC.exe` SHA256 has
been verified. It searches the executable's writable PE `.data` for
**exactly two** matching 14-entry arrays with the original action bits,
disabled directional and spare bits, terminator, and Start's default
BUTTON11. Both arrays must have writable, DWORD-aligned Start entries
and an unassigned target button. If any check fails, it does nothing.

This structural detection is an initial hypothesis drawn from the
partial decompilation, **not proof the retail image contains two such
tables**. A fail-closed result is expected if the original differs.
**Note:** Start remapping is attempted after native `Spidey.cfg` config
reload, because earlier initialization could be overwritten.
`[Debug] Log=1` displays the exact outcome:

- `GAMEPAD-START: OFF`: opt-in absent; no remap attempted
- `GAMEPAD-START: REQUESTED ...` and `SCAN ...`: user opt-in read and native tables inspected
- `GAMEPAD-START: EXPERIMENTAL mapping Start ...`: both tables changed
- `GAMEPAD-START: expected two verified controller maps; found=...`:
  unsupported layout, no modification
- `GAMEPAD-START: conflicting gameplay button ...`: no modification

## Runtime test matrix before enabling by default

1. On Linux/Wine with the exact supported executable, check startup log,
   original 1.1.0 gameplay controls and analogue menu navigation.
2. Test **main menu**: move highlight then press the selected physical
   button once, confirm it enters exactly one menu. Return with keyboard.
3. Test the native **SAVE GAME PROGRESS?** dialog and NO/YES selection,
   plus title spacing from the other PR #73 change.
4. Test each opening Bink video and the New Game movie after its normal
   unskippable initial interval; ensure one press advances **only once**
   and does not auto-skip following 3D scripts.
5. Test in 3D gameplay: movement, Jump/Kick/Punch/Web/Aim/Zipline/Swing,
   and native pause/Start if supported. Ensure the assigned button does
   not fire an unrelated action.
6. Run the same tests natively on Windows. Compare against 1.1.0 and
   preserve a `SpiderMan2k1Fix.log` with `[Debug] Log=1`.

**Acceptance criteria:** exact desired gamepad menu/video semantics
under Wine and Windows, no startup crash or gameplay regression,
and no added keyboard/focus/DirectInput hook. If native Start alone does
not confirm choices or skip a Bink sequence, diagnose the game-specific
`PCSHELL_CheckTriggers` path rather than force a keyboard input.

legluondunet assisted by ChatGPT (OpenAI)
