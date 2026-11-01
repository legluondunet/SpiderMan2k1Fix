# Combined Xbox / mouse-off / Alt-latch runtime acceptance

**Status:** Draft PR #91 — test-only. **Do not merge.** Base PR #80,
opt-in Left Alt recovery from PR #88, permanent game-mouse removal from
PR #90. This document does **not** authorize changing protected `main`
or altering the individual draft PRs.

## Verified artifact and composition (1 November 2026)

- GitHub integration commit: `1159b11e170bfc95b95fbde8b0016b52b7f7c954`.
- Full [Windows x86 CI run #41293775028](https://github.com/legluondunet/SpiderMan2k1Fix/actions/runs/41293775028)
  **PASS**: **741 Python regressions**, PowerShell audio probe,
  native x86 MSVCRT DLL compilation, packaging, bundled SDL3 and
  SHA-256 output.
- [Installable ZIP artifact](https://github.com/legluondunet/SpiderMan2k1Fix/actions/runs/41293775028/artifacts/13079045133):
  `SpiderMan2k1Fix-Windows-x86`, GitHub artifact ID
  `13079045133`. This is **not** a release.
- Source tree audit: **286** non-overlapping files identical to
  latest base PR #80; **40** identical to PR #88; **8** identical
  to PR #90. No unexpected file blob substitutions; only
  `ini_validation.h` is consciously reconciled to retain BOTH
  `ExperimentalAltLatchRecovery` and the obsolete/ignored
  `MouseEnabled` settings. Added only
  `ChatGPT/tests/test_integration_mouse_alt_latch.py`.
- Tested separate DLLs **do not establish hardware acceptance**
  for this new combined binary. Full original
  `SpideyPC.exe` SHA-256 is
  `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.

## Installation and diagnostic configuration

1. With the game **closed**, back up existing `winmm.dll`,
   `SpiderMan2k1Fix.dll` and `SpiderMan2k1Fix.ini`.
   Extract the Windows x86 artifact, install the two DLLs
   **together in their supplied layout**, preserving the existing
   INI where possible. Do not combine DLLs from different runs.
2. For diagnostic runs, use `[Debug] Log=1`.
   The startup log shows `SpiderMan2k1Fix v1.1.0-...`;
   a GitHub PR workflow can build a **temporary test merge**
   with a *different* SHA from the branch head. Verify the
   GitHub commit parents of that SHA rather than assuming
   an exact prefix.
3. On **native Windows 11**, to test the already-hardened
   one-byte Left Alt fix, deliberately opt in:

   ```ini
   [Input]
   ExperimentalAltLatchRecovery=1
   ```

   Without this key (or with `=0`), it stays **OFF**.
   Confirm `ALT-LATCH: ARMED` at startup when opted in;
   do not infer `REPAIRED` must always occur.
4. To test legacy configuration compatibility,
   the same `[Input]` section may contain
   `MouseEnabled=1`. The game mouse **must still remain
   disabled**; expect a log saying the obsolete setting is
   ignored. **No new generated INI** offers this old toggle.
5. Set `SDL3GameplayTrace=0` and `Xbox360TriggerProbe=0`
   in `[Debug]` for ordinary sessions; the 64-sample
   controller trace and raw trigger probes are for targeted
   tests only.

## Windows 11 acceptance checklist (strict focus regression)

- **Menu, intro, playable 3D, game Pause, back to menu** work
  with keyboard alone; normal audio, videos, game rendering,
  30-fps behavior, and full-screen borderless remain intact.
- Test an actual **external application Alt+Tab** while 3D:
  the existing focus handler should enter its custom
  PAUSED state when it confirms real external focus loss;
  the game resumes on the intended input, without a
  crash, frozen controls, stuck keyboard Alt, or stuck
  controller buttons. Verify in-game keyboard and
  controller actions **after** return.
- Opt-in Alt experiment: a `REPAIRED ... 0x7F->0x80`
  record occurs **only if** the original game actually has
  a stale native Left Alt latch after an eligible confirmed
  focus loss and return. `SKIPPED`, `EXPIRED` or
  **no repair** can be expected for clean returns and
  are **not** themselves a failure. Must never neutralize
  physical Alt, Tab or AltGr.
- Test supported **Sony DS4** and **Xbox 360** separately.
  Ensure Xbox **RT triggers Swing and releases cleanly**,
  LT does not trigger Swing, Back opens normal game
  Pause, Start/selection does not leak to the other
  controller, and mapped buttons perform intended
  actions. Simultaneous DS4+Xbox handoffs are a
  **separate**, more demanding test and not proven by
  single-controller acceptance.
- In-game mouse cursor, mouse movement and clicks
  remain **nonfunctional**, even with old
  `[Input] MouseEnabled=1`. The **Windows desktop
  cursor must remain visible/usable after Alt+Tab**.
- Attach full `SpiderMan2k1Fix(...).log` and note
  observed physical behavior, not just detected HID IDs.
  One or more successful manual cycles is needed before
  considering an approved combined build.

## Wine/Proton acceptance checklist (tolerate cosmetic focus loss)

- Validate keyboard, Xbox 360 and/or DS4 gameplay, normal
  game Pause, mouse permanently OFF in game, and visible
  OS pointer **outside** the game.
- After changing applications, check whether the **game
  still accepts keyboard/controller commands, sound and
  graphics remain normal, and no crash/hang occurs**.
  **A missing automatic background `PAUSED` overlay is
  explicitly NONBLOCKING if the return is functionally
  stable.** Do not add forced focus, speculative hooks or
  input injection just to display a ribbon.
- If Wine does not report a real background transition,
  **do not count it as an Alt-latch recovery test**:
  `ExperimentalAltLatchRecovery` depends on the original
  confirmed-pause/return path, and must not mutate keys
  on unobserved or spurious focus changes.
- Reopen this issue only if a reproducible *functional*
  regression occurs: keyboard/controller stuck,
  missing audio, broken cursor on desktop, graphics
  lost, or crash. Capture the full log and actual
  reproduction conditions.

## Evidence boundaries

Previously confirmed **separately**:
- PR #88 Windows 11: **three successful Left Alt repairs
  after true 3D external focus returns** on a hardened
  code revision, with original game keyboard states
  observed to clear; the experiment stayed default-OFF.
- PR #90 Wine 11.0: in-game mouse suppressed, old
  `MouseEnabled=1` ignored, normal desktop cursor
  after Alt+Tab, including a background interval over
  four minutes, no visible game instability.
- PR #80 Wine 11.0: Xbox 360 physical gameplay
  confirmed by maintainer, native SDL3/DI identity,
  trigger RT down/up, game Back/Pause and exit.
  A missing custom focus PAUSED overlay was accepted
  by the maintainer because the game continued normally.

**Combined PR #91 hardware tests remain outstanding**.
CI PASS and identical component blobs are necessary
but not a substitute for actual Windows/Wine gameplay.

legluondunet assisted by ChatGPT (OpenAI)
