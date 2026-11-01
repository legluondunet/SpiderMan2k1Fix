# SpiderMan2k1Fix: integration and release-readiness audit (1 November 2026)

**Status: audit and release checklist only; no merge, tag or release authorized.**

## Outcome of the tested combined code

- The combined source starts with stacked native SDL3 controller PR #80,
  permanently disables original game mouse input with #90,
  and includes guarded Left Alt latch repair from #88; their
  integration lives on **draft PR #91** (`integration/xbox-mouse-alt-focus-test`).
- **Windows 11 physical tests:** 20/20 maintainer checkbox results PASS
  on the *combined binary* `v1.1.0-72e2f2a` (1920×1200, Windows build
  26200, Sony DS4 + Xbox360). Log includes **nine confirmed
  background PAUSED/resume cycles**, **six actual native Left Alt
  `0x7F->0x80` repairs**, **37/37 active DirectSound voices restored**,
  seven Bink pause/resume pairs, and two balanced Xbox RT Swing pulses.
  Gamepad functionality and audio/video quality are also established by
  the maintainer's direct observations, not merely inferred from traces.
- **Wine/Proton physical tests:** Xbox RT Swing, native controller
  ownership, 3D play, 11/11 and 9/9 playing DirectSound
  voices restored, desktop cursor and permanent game mouse-off.
  Under Wine, the custom focus PAUSED overlay can appear immediately
  in menus, at an intro-video boundary, after an unexplained
  ~13-second interval in 3D, or not at all. The maintainer
  expressly **accepts this when return to the game remains fully
  functional**; do not add risky forced focus hooks to fix a cosmetic
  ribbon mismatch.
- **INI incident resolved:** a backup had **duplicate**
  `[Input] MouseEnabled=0` (line 34) and `MouseEnabled=1`
  (line 79) after the negative test's `=1` was appended.
  The strict C parser correctly repaired it, recovering 54
  settings. Later Windows session records `INI-CHECK: valid`.
  Do not relax duplicate-key validation or overwrite subsequent
  saved gamepad/volume changes.
- **Secondary gamepad input-loss:** one native Windows menu trace
  contains `DIERR_INPUTLOST` for the fix-owned auxiliary
  DirectInput device, safely released with original native
  controller rebinding; no unrecovered crash. This specific
  menu-only trace does not prove a 3D hotplug stress test.
- Current source/CI has already passed **741 Python regression
  checks**, portable LLVM-MinGW x86 DLL build, SDL3 packaging
  and SHA-256 verification. CI success **alone does not validate
  new controller models or every physical behavior**.

## Review of how PR #91 relates to main

**Do not retarget or merge PR #91 straight into `main` as a
small standalone change.** The change is an aggregate of a
long-running experimental dependency stack. GitHub's
`compare(main, integration/xbox-mouse-alt-focus-test)`
on 1 Nov reported **diverged: 964 commits ahead, four behind,
190 files changed**. The four newer `main` commits concern
the README, changelog, 1.1.0 release notes, a small test fixture
and audio research documentation; these must not be silently
overwritten by the older integration branch. `main` SHA at
audit: `a6c8b4c0e9727ec38bbe3dce10c8efd1e1726b1b`.
Counts are a snapshot and may change as branches move.

The GitHub PR base/dependency chain at this audit is:

1. **PR #73** `fix/native-save-dialog-title-spacing` →
   `main`.
2. **PR #74** `design/dualshock4-ps1-contextual-profile`
   → #73.
3. **PR #76** `feat/ds4-safe-hotplug-reconnect`
   → #74.
4. **PR #77** `feat/xbox360-native-hotplug`
   → #76.
5. **PR #79** `feat/sdl3-gamepad-probe`
   → #77.
6. **PR #80** `experiment/sdl3-native-input-owner`
   → #79.
7. **PR #91** `integration/xbox-mouse-alt-focus-test`
   → #80; its **three-parent history** contains the
   source heads of **PR #88** (generic candidate research +
   guarded Alt recovery) and **PR #90** (mouse OFF).
   Their common INI validator was resolved intentionally.

**PR #78** is a separate Xbox profile branch based on #76 and
is **not on the above dependency chain**; compare overlapping
features before any separate merge. **PR #86** is a separate
DS4 audio-output routing experiment based on `main` and
is **not validated by PR #91**. Issue #87 and the generic
DS3/Steam Deck branch #88 still describe research and should
**not be advertised as supported controllers**.

Recommended merge/review procedure **only after explicit
maintainer authorization**:

- Check each stacked PR and required checks in order; preserve
  `main` and any independently released audio documentation
  when reconciling conflicts.
- Do not blindly merge #88 and #90 **in addition** to the
  three-parent #91 integration commit: their commits are
  already ancestors of #91 and duplicates or conflicts
  could result. Choose a reviewed single history strategy
  before touching protected branch rules.
- Prefer staged review/integration onto current `main`
  (or a fresh `release/*` staging branch), instead of
  force-updating `main` or shipping the raw research
  stack as one opaque change.
- Re-run CI on the **exact final reconciled tree** and compare
  produced packaged files and source to the working tested
  PR #91 binary. Only then run a brief original-exe
  native Windows/Wine game smoke test on that final binary.
- Never silently incorporate separate #86 audio routing,
  #78 controller prototype or unvalidated generic
  DS3/Steam Deck control into the release.

## Scope and default-configuration decision

The maintainer tested experimental controller ownership and
Left Alt repair with **opt-in** INI keys. The factory INI
has fewer settings and **does not enable** all these
experimental features automatically. The current tested
code continues to leave `ExperimentalAltLatchRecovery`
**OFF by default**, and the SDL3 native-owner/bridge
depends on explicit feature opt-ins.

Before release, decide whether to keep such controls
**opt-in** in a beta/testing build (lower compatibility
risk, recommended for first publication) or to promote
them to default behavior in a **separate, hardware-tested
change**. Do not advertise the existing factory defaults
as if they contained every newly tested optional fix.

**Permanent in-game mouse OFF** and the original
game-specific focus PAUSED mechanism are separate
built-in compatibility behaviors; they should not be
confused with these experimental opt-in toggles.

## Documentation and release gate

- New grounded user-facing [README FAQ](../../README.md#frequently-asked-questions)
  addresses intentional lack of in-game cursor,
  distinct missing OS desktop pointer after Alt+Tab,
  and how to diagnose total silence versus
  music/movies/effects. This completes the content
  requested by **issue #89 on the draft branch only**;
  keep the issue open until the FAQ lands on the
  intended release/default branch.
- **Changelog cleanup completed on the draft branch:**
  [public `CHANGELOG.md`](../../CHANGELOG.md) now
  preserves the exact already-published `main`
  entries for **v1.1.0 and v1.0.0**, with a concise
  explicitly **Unreleased/not merged** PR #91 section.
  The prior **52,342-character** experimental research
  journal is preserved **verbatim** under
  [CHANGELOG_EXPERIMENT_HISTORY_PRE_20261101.md](CHANGELOG_EXPERIMENT_HISTORY_PRE_20261101.md),
  marked historical rather than current policy.
  [A regression test](../../ChatGPT/tests/test_changelog_release_boundary.py)
  guards both historical release headings, opt-in
  research boundaries and the presence of that archive.
  Do not treat the older claims about re-enabling the
  game mouse or untested Windows controller hardware
  as current release behavior.
- `RELEASE_NOTES_1.1.0.md` describes the already
  existing 1.1.0 audio release. **Do not silently
  repurpose or overwrite those release notes** for
  the subsequent controller/Alt/mouse update. Pick
  the next version and write separate release notes
  after the release scope is approved.
- CI and real-game evidence covers the exact retail
  `SpideyPC.exe` with SHA-256
  `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.
  No promise of other EXE editions or generic gamepad models.
- Packaging/install must keep **`winmm.dll` beside
  `SpideyPC.exe`**, with `SpiderMan2k1Fix.dll`
  and bundled Windows x86 `SDL3.dll` in the
  provided subdirectory; do not flatten or mix builds.

**Disposition:** code integration plus user-observed Windows/Wine
hardware is **validated on the combined test artifact**, but
**release integration, final default behavior, historical
changelog cleanup and publication are separate decisions**.
No branch was merged, no release was tagged, and no
default-on experiments were enabled by this documentation.

legluondunet assisted by ChatGPT (OpenAI)
