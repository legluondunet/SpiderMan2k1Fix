# SpiderMan2k1Fix 1.0.0 — source and release-readiness audit

> **Date:** 2026-10-24. **Status:** static review and candidate cleanup
> prepared in `release/v1.0.0-audit`, pending GitHub Actions and full
> maintainer Windows/Wine smoke tests. This is not a certification that
> every game level, driver or undocumented native code path has been
> tested. No tag or public stable Release exists at this stage.

## 1. Scope and evidence

Reviewed the effective Windows x86 compilation manifest in `build.sh`,
the runtime C translation units (the game-facing `spiderman2k1fix.c`,
separate `winmm_proxy.c`, `quick_exit_f12.c`, rendering, FOV,
HUD, screenshot, keyboard, focus, audio, virtual configuration and
input modules), the original `version_info.h`, public player docs,
the workflow, and representative targeted regression tests.

The checked compilation manifest contains **23 game DLL C units**
and **one proxy C unit**. Two additional historical C units plus their
headers — `pause_overlay.c/.h` and `pause_dim.c/.h` — were
present in the repository but **absent from the build** and superseded
by the active surface-based focus pause. They were removed from the
release candidate after confirming no runtime source or build depends
on them; history remains in Git. The only test reading the old
`pause_dim.c` was updated to verify that these files do not return.

The baseline PR #47 CI passed **471 automated tests** plus a 32-bit
MSVCRT DLL compilation; that passing result describes the **previous
release-candidate base**, not the new v1.0.0 branch. A fresh run of
the same workflow is mandatory for this branch.

### Technical inventory

| Subsystem | Important code / dependencies | Review result |
| --- | --- | --- |
| DLL boot | `winmm_proxy.c`, `spiderman2k1fix.c` | Preserve worker/init synchronization and first DirectDraw IAT gate; hook before game requests unsupported 640×480 exclusive mode |
| Supported executable | SHA-256 guard and original x86 signatures | Preserve exact game identity check and fail-closed behavior |
| Disc check | Early No-CD patch at RVA `0x1163E0` | Preserve runtime-only signature check, original EXE unchanged on disk |
| Graphics | DirectDraw/Direct3D7 hooks, `hud_widescreen.c`, `hud_adaptive.c`, `menu_scale.c`, `comic_native.c`, `fov_aspect*` | Geometry and ABI/caller guards are sensitive to tiny changes: no unproven optimization or hook reordering |
| Native resolution | `display_plan.c`, `display_policy.c`, `display_menu_lock.c` | Keep monitor-native borderless and 4:3 canvas policies |
| FPS | `frame_limiter.c` | Uses `QueryPerformanceCounter`, avoids `timeBeginPeriod`, caps default to 30 FPS; no level-specific 20 FPS behavior |
| Focus/input | `input_focus_trace.c`, `input_notacquired_retry.c`, `pause_any_key.c`, `pause_surface.c` | Permanent pause and DirectInput reacquire; strict signatures and Windows/Wine validation needed |
| Native media/audio | `focus_media_pause.c`, `focus_dsound_pause.c` | Bink API and 32 secondary DirectSound voices scoped to recognized original handles; avoid additional COM hooks |
| F9 PNG | `f9_png.c`, `f9_toast.c` | Dynamically loads GDI+ only for capture, writes temporary PNG and publishes by rename; no startup GDI+ dependency |
| Settings | `virtual_spidey_cfg.c`, `ini_validation.h`, `ini_recovery.h`, `keyboard_names.c` | Keep versioned compact INI, old-settings migration, safe recovery and single `.ini.old` backup |
| Debug F10 | `quick_exit_f12.c` | Large but **not dead code**: explicitly controlled development instrumentation; do not remove without independent validation |
| Proxy exports | `winmm.def` and source forwarding | Do not prune WinMM exports by usage guesses: middleware and original game may require them dynamically |
| Non-shipping research | `docs/`, `ChatGPT/`, `.gitattributes` | Kept in Git, excluded from automatic source archives; player package stays minimal |

## 2. Changes in the 1.0.0 candidate

1. **Delete genuinely obsolete source:** retire the four previously
   uncompiled Win32 popup/dimming files. The active `pause_surface.c`
   paints the focus-pause presentation on the existing DirectDraw
   surface without an extra popup window. No runtime link object is
   removed, so this change is expected to leave the DLL code path
   unchanged.
2. **Version:** change the previous development placeholder
   `v0.0.0-<commit>` to **`v1.0.0-<commit>`**, preserving
   reproducible commit identification in menu/logs.
3. **Documentation hygiene:** correct source comments and diagnostic
   descriptions that still described permanent Bink/DirectSound focus
   pause, F9 capture and DIERR_NOTACQUIRED recovery as opt-in or
   experimental. No audio, input or screenshot algorithms were changed.
4. **Release material:** add release notes and a manual publishing plan;
   update the player homepage for a **candidate**, not an already
   published stable download.
5. **Regression contract:** update structural version/packaging tests
   and assert the deleted legacy renderer cannot accidentally return.

**Performance decision:** no speculative optimization of the frame
limiter, D3D7 rendering hot path, COM buffers or game-memory patches
is proposed for the first stable build. The build already uses
`-O2`, signature-gated function hooks, and scoped geometry tests.
An unmeasured micro-optimization would have a worse release risk than
a plausible performance gain. The dead modules were not compiled,
so their deletion improves source maintenance, **not** expected runtime
CPU usage or DLL size.

## 3. Risk register and required follow-ups

| Severity | Area | Finding and next action |
| --- | --- | --- |
| High / release gate | Regressions in native game | CI tests are source/contract checks plus helper tests, **not** a replacement for real Windows 11 and Wine gameplay. Recheck No-CD, startup, F9, focus, input, sound, native ESC, first 3D, 4:3 and Auto. |
| High / release gate | DLL binary ABI | Preserve x86 PE32, **MSVCRT only** (no UCRT import), static libgcc, WinMM forward exports, early DirectDraw entry. Confirm `objdump` inspection on candidate artifact. |
| High / release gate | Binary ownership / patch signatures | Fixed native memory addresses apply to one SHA-pinned original executable. Fail closed for unsupported versions. Never retarget without separate analysis. |
| Medium / gameplay limitation | Timing-sensitive stages | Default 30 FPS remains user-configurable; some original sequences may need 20 FPS. No auto-stage limiter is included or promised. |
| Medium / native sound | Original audio bugs | Known mix/loop problems are **not** repaired by focus pause. Investigate audio separately after the 1.0 gate. |
| Medium / progress saves | `SPIDRMAN.DAT` | The game owns progress files. The fix's robust INI migration does not protect the original save slots. Save-format research exists in a separate documentation PR #48, not automatically included. |
| Medium / maintenance | Oversized source units | `spiderman2k1fix.c` (~155 kB) and optional `quick_exit_f12.c` (~184 kB) should eventually be decomposed into cohesive modules, **after** the stable tag; refactoring now risks hook ordering and debug code ownership. |
| Medium / supply chain | Portable LLVM-MinGW | `build-native-msvcrt.sh` selects a dated compiler archive but validates it through `tar -tJf` only, not an authenticated SHA-256 against a pinned upstream checksum. Official CI uses installed MSVCRT MinGW; consider pinning and verifying downloadable toolchains in a future dedicated PR. |
| Low / runtime diagnostics | Log messages | Keep logging opt-in (`Log=0` by default) and do not reintroduce hot-path pixel probes or noisy periodic traces. |
| Low / packaging | Source/artifact separation | Do not ship original assets, `SpideyPC.exe`, generated player INIs, saves, debug fixtures or research material inside the Windows two-DLL package. |

Additional limits: a static review cannot establish that every `strcpy`
path is exploitable or safe without tracing the preceding buffer-size
conditions in that path. The selected configuration and screenshot
code shows explicit path/size checks, but neither the proprietary game
nor the patch has been exhaustively fuzzed. Avoid declaring
memory-safety or security certification on this evidence.

## 4. Release workflow controls

- CI workflow:
  `.github/workflows/spiderman2k1fix-windows-x86.yml`.
  It runs `python3 -m pytest -q ChatGPT/tests`, compiles
  both DLLs through `build.sh`, and packages
  `winmm.dll`, `SpiderMan2k1Fix.dll`, `README.md`, `CHANGELOG.md`.
- `build.sh` checks **CRT imports, PE32, stable startup export and
  required WinMM exports** before installing generated binaries.
- Generated `SpiderMan2k1Fix.ini` is deliberately absent from the ZIP.
- GitHub Actions artifacts are **temporary test builds**, not a public
  stable GitHub Release.
- Do not create `v1.0.0`, publish assets or merge this PR until the
  user has reviewed the CI run, tested Windows/Wine and explicitly
  approves the final merge/publication.
- `RELEASE_NOTES_1.0.0.md` provides the acceptance checklist, player
  requirements, legal notices and the manual release procedure.

## 5. Audit conclusion

The project is **credible as a 1.0.0 release candidate**: it has
documented runtime improvements tested by the maintainer, strict
original-binary guards, binary import/export validation and extensive
regression tests. The audit found a small amount of truly uncompiled
legacy source and outdated comments, now removed/corrected.

**Not yet a published v1.0.0.** Passing the new candidate CI and real
Windows/Wine game verification, followed by explicit approval, remain
the gate. Separately documented experimental/debug code is not
automatically "unused" and should not be deleted to make a diff look
smaller.

legluondunet assisted by ChatGPT (OpenAI)
