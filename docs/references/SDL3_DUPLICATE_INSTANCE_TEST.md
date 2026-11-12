# SDL3 duplicate VID:PID — instance-only universal owner trial

**Status: PR #137 merged into `main` on November 12, 2026.**
The same source changes are included in the integrated v2.0.0-rc.2
candidate, which still requires full release smoke testing.

## Motivation

The 2026-11-08 GE-Proton11-12 diagnostic session with an EasySMX gamepad
reported two opened SDL3 devices labelled Xbox 360 Controller using
`045E:028E` (instance IDs 2 and 3). Start on ID 3 was repeatedly refused by
the old model-uniqueness rule. After switching the EasySMX's USB mode it
reappeared as `2345:E07A` (instance ID 4) and Start selection worked.
The LED colors alone do not reliably identify the controller's protocol.

## Rule change — universal keyboard bridge ONLY

- Use the exact nonzero connected SDL3 instance ID, paired with its
  nonzero VID:PID. Two connected pads with the same VID:PID may each be
  selected by fresh Start, one at a time; per-model profile files are shared.
- Collect all fresh Start edges across the Flip; **two edges in one Flip
  refuse selection**, rather than making enumeration order the owner.
- Reject repeated SDL instance IDs, stale/detached handles, changed VID:PID,
  disconnected or unknown product IDs. Release/replug requires new Start.
- Leave the old native DirectInput model-unique rule unchanged. No DI
  CreateDevice/rebind is introduced, and the existing DIERR_UNPLUGGED guard,
  neutral-first admission, synthetic KEYUP, focus/Alt+Tab protection and
  one-current-owner keyboard injection remain mandatory.
- Modal controller selection is revalidated against a live SDL instance on
  the next safe foreground game Flip.

## Hardware results and outstanding edge cases

**Confirmed November 9 on Windows 11:** two Xbox-compatible controllers with
VID:PID `045E:028E`, alternating Start ownership and no interference reported
by the maintainer; native SAVE GAME / YES accepted via A. Diagnostic evidence:
`SpiderMan2k1Fix(20261109-063418).log` on integrated RC2.

**Confirmed November 12 on Wine/GE-Proton:** two Xbox-compatible controllers
with `045E:028E` and one PS4 controller with `054C:05C4` detected; the
`SpiderMan2k1Fix(20261112-164531).log` contains successive successful SDL3
Start selection IDs 4 → 3 → 1 → 4 → 3, including four handoffs. The tested
runtime source files are identical to the latest RC2 CI source. These facts
support working controller selection; they do not prove every gameplay input.

The remaining unchecked items describe **additional edge-case or release
acceptance tests**, not an absence of the two-platform hardware validation.


- [ ] With two `045E:028E` devices visible, hold neither controller,
  press Start on **only one** controller, release to neutral, confirm
  its menu/3D/Pause input works and the other remains a spectator.
- [x] Switch owner using Start on the *other* controller and confirm no
  duplicated controls or stuck key after handoff (Windows 11 confirmation;
  Wine/GE-Proton ownership trace).
- [ ] Simultaneously press Start on two pads (or one pad mirrored twice),
  verify selection is refused without a spontaneous new owner.
- [ ] Disconnect/replug selected pad, require fresh Start, confirm
  keyboard fallback, no crash and no runaway input.
- [ ] Alt+Tab during Bink and gameplay; verify synchronization and
  controller/keyboard resumption.
- [ ] Test one DS4 or PS3 with another pad present. Confirm normal
  profile mapping and proper native safety guard behavior.
- [ ] Run the same build on both Wine/Proton and native Windows where
  hardware is available. Attach the diagnostic log if unexpected behavior.

Caution: when an individual physical pad is mirrored as *two* SDL3 gamepads
with identical simultaneous Start edges, this policy intentionally refuses
an ambiguous claim. Hardware proof is required before asserting that such a
pad can be used in its default mode.

legluondunet assisted by ChatGPT (OpenAI)
