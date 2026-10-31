# Generic SDL3 gamepad bridge — design proposal (phase 0, read-only)

Status: **research only**, no gameplay takeover and no native input patch.
Issue: [#87](https://github.com/legluondunet/SpiderMan2k1Fix/issues/87).
Experimental parent: [PR #80](https://github.com/legluondunet/SpiderMan2k1Fix/pull/80),
`experiment/sdl3-native-input-owner`.
This research branch is **stacked** on that branch; do not merge either branch
into `main` without explicit user validation.

## Problem, from real Windows 11 tests

The live Windows DirectInput path is verified for the specific Sony DS4
(`054C:09CC`) and wired Xbox 360 (`045E:028E`). Both can acquire menu
ownership and run their own custom face-button mappings in 3D; Xbox RT Swing
is separately guarded.

A Sony DualShock 3 `054C:0268` appears to Windows DirectInput **and** SDL3
(`PS3 Controller`), but the game does not yet accept it. The user's unplug /
replug sequences during that session were intentional; no spontaneous
disconnect failure is established. We must not label it unsupported by
SDL3, or copy DS4 DirectInput button indices into its native game table.

On Steam Deck, Steam Input / Proton may present a physical, virtual or
keyboard-mapped device to a Windows game. Actual presented identity and
analog behavior depend on configuration. We must test it rather than
promise a specific VID:PID or claim universal compatibility today.

## Three different things: device, candidate, game owner

1. **SDL3-opened device**: a runtime *instance ID*, independent of vendor,
   product, name or raw DirectInput indices. SDL3's normalized gamepad
   buttons/axes are the input vocabulary. Unknown/zero VID/PID does not
   by itself invalidate *passive* candidate selection.
2. **SDL3-selected candidate**: at most one instance, chosen on a fresh
   neutral-to-Start edge after foreground validation. A candidate is not
   an authorization to modify the original 2001 DirectInput pointer.
   Do not merge input from two devices. A previously connected or held
   Start is never an eligible new edge.
3. **Native game owner**: the **separately verified** device or future
   authenticated generic input bridge. In the current code only known
   DirectInput products with unique VID/PID and verified action indices
   may be installed. The portable candidate policy must not change this.

This separation prevents the exact bug where SDL3 would select an
unsupported DS3 and the original native reconnect code would detach the
working Xbox instead of delivering controller input. On an unsupported
candidate, retain the last verified game owner until an actually safe
generic-bridge handshake is implemented and tested.

## Proposed runtime architecture (NOT IMPLEMENTED)

```text
  SDL3 handles, gamepad database, Steam Input virtual controller
           |
           v
  normalized per-instance sample + stable semantic profile
           |
           v
  one-candidate selection & input-context neutral/focus guard
           |
           +--> diagnostic / read-only action plan  [phase 0/1]
           |
           v
  verified, single-owner transaction (not yet available)
           |
           v
  game-native input reader or guarded virtual DI state  [phase 2]
           |
           v
  original engine 3D analog + actions / menu / Bink / pause
```

SDL3 standardized button labels are spatial: south/east/west/north.
They do **not** imply stable raw DirectInput button indices. SDL3's
mapping database can be extended for unknown gamepads (see
https://wiki.libsdl.org/SDL3/SDL_AddGamepadMapping), but that alone does
not supply a safe way to drive the 2001 native engine.

## Design constraints / exit criteria

- Preserve **all** validated PR #80 behavior (DS4 native UI, Xbox custom
  B/X and Y/A swaps, Xbox RT Swing, menu Circle/B close owner, hotplug,
  Alt+Tab and pause). The generic bridge starts opt-in **OFF**.
- The candidate can be any **opened** SDL3-mapped instance (including
  DS3 or one with unknown VID/PID); for native game ownership, reject
  until verified. Never use device name or guessed VID/PID as proof.
- Replugging creates a new instance ID: a stale claim must be cleared,
  and a fresh neutral→Start sequence must be required. A device Start
  pressed during Alt+Tab cannot claim ownership on return.
- Duplicate/virtual device exposures: do not concatenate their button
  masks or analog axes. Verify which device actually generates SDL3
  events and prefer explicit user selection; investigate physical ↔
  Steam Input virtual duplication before introducing de-duplication.
- The planned output bridge MUST have signed, SHA-guarded native
  signatures, stable sample lifetime, a bounded game thread only,
  supported axis ranges, noninvasive restoration, and explicit
  emergency fallback to current game-owned DirectInput.
- Never bridge a new controller to native input by synthesizing keyboard
  only and call it feature-complete; that loses analog movement and
  risks double injection. Keyboard can remain a clearly labeled
  fallback through Steam Input if the user chooses it.
- Separate profile *runtime identity* (SDL instance ID) from
  *persistent identity* (validated stable SDL GUID/serial/path when
  available, otherwise safe model fallback); preserve existing
  `VID-PID.ini` custom profiles and atomic writes. The current
  `sdl3_controller_profile.h` requires nonzero VID/PID; its migration
  requires a versioned design, not a permissive one-line change.
- Protect menu/cutscene timing and pause: never let a held Start or
  Circle/B press leak through a dialog or focus boundary.

## Phase-0 deliverables

- Portable C99 **read-only** SDL3 candidate policy in
  `sdl3_generic_candidate.h`, test-only: no Windows API, DirectInput,
  SDL device opens or game memory writes. It uses the already tested
  neutral-armed Start selector but *does not replace* the existing
  owner state.
- C99 tests proving DS3, unknown VID/PID and multiple identical models
  can be candidates by instance ID **without** declaring a native owner;
  hotplug and Alt+Tab invalidation; refusal of stale or competing
  candidates; existing Xbox/DS4 ownership paths unchanged.
- Future phase 1: read-only instrumentation of the actual game-native
  input reader (3D and UI), first on Windows using the SHA-verified EXE,
  then Proton. Inspect axes separately from digital action table.
- Future phase 2: compare an authenticated native-reader bridge against
  a sandboxed virtual DirectInput compatibility layer. Any direct input
  mutation requires new explicit approval + hardware evidence.

## Hardware acceptance matrix (future phases)

| Scenario | Requirement |
|---|---|
| DS3 Windows `054C:0268` | SDL face buttons, Start, analog sticks, D-pad, L2/R2 observed; no unsupported native takeover |
| DS4 + Xbox | Model-specific custom actions unchanged after 4 handoffs; Xbox RT Swing preserved |
| DS3 + Xbox | Unsupported DS3 Start does not strand/disable Xbox on current PR #80 |
| Steam Deck / Proton | Native controls recognized, analog moves in 3D, menu works, focus suspension, Steam Input duplicate exposure investigated |
| Controller removal | No use-after-free, no held keys, no stale instance takeover; keyboard still works |
| Profiles | Accurate user model/instance identity, no overwrite of legacy custom profiles |

## Status protocol

`RECOGNIZED_BY_SDL3` ≠ `CANDIDATE_SELECTED` ≠ `NATIVE_VERIFIED` ≠
`GAMEPLAY_WORKS`.

Only the first two are within scope of this read-only phase. No public
promise of generic controller compatibility yet.

legluondunet assisted by ChatGPT (OpenAI)
