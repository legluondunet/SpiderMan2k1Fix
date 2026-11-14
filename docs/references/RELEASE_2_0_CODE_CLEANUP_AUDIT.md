# SpiderMan2k1Fix 2.0.0 — source cleanup and release gates

Status: **integrity and negative-path work merged into `main`** (PRs
#150, #151, #154, #155); the official v2.0.0 release is **not published**.
The chronological findings below include historical statements from the
November 13–14 prototype audit. Later Windows 11 original-ISO logs and
the [Windows PE32 executable CI](https://github.com/legluondunet/SpiderMan2k1Fix/actions/runs/43616844363)
supersede those initial unknowns. The final pending gate is a maintainer
smoke test with the new eight-file receipt build and authentic ISO; CI alone
does not replace this test.

## Target behavior kept intact

- One Windows x86 `SpiderMan2k1Fix.exe` with the project WinMM proxy,
  `SpiderMan2k1Fix.dll`, pinned Windows x86 SDL3, license and optional
  Linux X11 helper embedded (not the copyrighted original game).
- Five install/verification modes: default automatic, `--install-only`,
  `--verify`, `--launch-no-verify`, `--inspect ISO/CUE [BIN/IMG]`;
  plus `--verbose`, `--version`, `--help`, `-h`. Only `--inspect`
  accepts explicitly named media; automatic install searches beside the EXE.
- Two main independent logs beside the launcher; a third, separate
  `SpiderMan2k1Fix_inspect.log` for unsupported discs.
- First install stages the original Wise eight-file full-install superset
  together with project resources and a receipt. Only
  `--install-only` may change *receipt-authenticated installer-owned*
  resources of an existing installation. Normal startup stays read-only.
- Preserve SDL3-only gamepad support, 4:3 menus/save/pause, 3D widescreen,
  audio, focus and Wine behavior, user settings and saves; do not rewrite
  the original EXE or alter undocumented original-game bytes.

## Safe source cleanup implemented

1. **Removed obsolete prototype adoption** from installed-game handling.
   A prior untracked test build with no `installed-resources.sha256`
   can no longer silently manufacture a receipt from whatever files happen
   to be present. `--install-only` still allows strictly authenticated
   updates; normal launch and `--verify` also require the receipt.
   Old experimental installations need a clean reinstall from the owned
   original CD if provenance cannot be proven.
2. **Unified automatic support for ISO and CUE discovery.**
   `find_adjacent_disc` selects exactly one adjacent image; its CUE references
   the original BIN/IMG data track without accepting positional media
   arguments. Ambiguous sources and reparse points are refused. Explicit
   `--inspect` still accepts supported ISO/CUE/BIN/IMG forms.
3. Removed two **unread ISO parser fields** (`DiscImage.sector`,
   `DiscImage.error`) and an ineffective `WIN32_EXTRA_LEAN` definition.
   Deleted misleading comments referring to a prior incomplete prototype.
4. Updated the 2.0.0 README and release notes so the standalone EXE is the
   planned release artifact instead of instructions to copy a DLL ZIP by
   hand. Updated obsolete regression checks which required a manual
   `WINEDLLOVERRIDES` command, while retaining licensing notices.
5. Added regression checks for receipt provenance, release-state cleanup
   and mixed ISO/CUE media discovery. Kept the existing DLL build/CI checks
   intact instead of removing active compatibility modules based on names.

## First retail-ISO installation evidence — owner Wine log

The maintainer supplied `SpiderMan2k1Fix_installer.log` from a first real
installation using the draft standalone 2.0.0 launcher, dated November 14,
2026 in the present review. This **is evidence of an end-to-end copy,
verified original game executable and successful launcher process exit**,
not of gameplay, all runtime features or a clean Windows 11 installation.

- Original CD filesystem: `SPIDERMAN`, ISO9660+Joliet, 694,661,120
  bytes (662.48 MiB). The reader enumerated 313 filesystem entries,
  293 regular files.
- Exact original Wise source directory is **`Setup/`**. Source names,
  lengths (bytes) and file copy success:
  `SpideyPC.exe` 1,507,328; `data.pkr` 166,992,524;
  `media.pkr` 290,302,112; `texture.dat` 40;
  `binkw32.dll` 299,571; `anet.inf` 289;
  `SpideyPC.ico` 3,310; `Spider-Man Setup.exe` 45,056.
- Retail executable SHA-256 after extraction:
  `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`
  (matched supported hash).
- All seven bundled project-owned files were written; `winmm.dll`
  173,568 bytes, `SpiderMan2k1Fix.dll` 1,043,456 bytes,
  `SDL3.dll` 2,361,344 bytes; SDL3 license, Linux X11 helper,
  README and CHANGELOG also deployed.
- Launcher reports `LAUNCH: game exit code=0` and `END: status=0`.
  **The owner did not run `--install-only` in this captured session**,
  since `SpideyPC.exe` was started.
- No filesystem errors, missing-file diagnostics or mismatched original
  EXE signature were reported. The experiment does not establish the
  game's actual rendered startup, audio/inputs, runtime log content,
  source media checksums, or Windows 11 compatibility.

This closes the **unknown CD source directory** gap for the owner's
specific ISO and validates copying the recovered Wise full-install list
in one Wine environment. Keep the other below-mentioned release gates.
Never commit the original ISO, commercial game assets or the owner's
local filesystem path.

## Second field test — original-CD inspection, install-only and real gameplay

Owner-supplied logs dated November 14, 2026 provide **three independent
traces** from Wine-staging 11.17 and the same authentic original retail ISO:

1. `SpiderMan2k1Fix_inspect.log`:
   `mode=inspect`, ISO9660+Joliet volume SPIDERMAN, 313 filesystem
   entries, **293 regular files**, all original `Setup/` source entries
   visible; `INSPECT_RESULT=COMPLETE files=293` and `END: status=0`.
   Inspection is read-only and produces a separate report.
2. `SpiderMan2k1Fix_installer.log`:
   **fresh `--install-only` successful** for all eight original Wise
   game files plus seven project resources. Source `SpideyPC.exe`
   matches the supported exact SHA-256. Ending is
   `INSTALL: --install-only complete; game was not launched`,
   then `END: status=0`. This is distinct from the first test where
   default install launched the game.
3. `SpiderMan2k1Fix.log`:
   `SpiderMan2k1Fix v2.0.0-b1f49a6` loaded through WinMM,
   supported exe hash matched, native DirectDraw7/D3D7 initialized,
   desktop 1920×1080 with borderless presentation and centered 4:3
   UI, world FOV scale 0.75 on entering 3D gameplay,
   SDL3 discovered two Xbox 360 and one PS4 controller, menu and
   gameplay interactions occurred, native 4:3 pause and Save/Name UI
   render paths were exercised, and two external-focus pause/resume
   cycles were logged (secondary DirectSound voice failures=0).
   Native WAV source index lists 656 valid sounds, with loader tail
   repairs performed. These logs demonstrate real game activity, not
   just a successful process spawn.

**Caveats:** The 2,215-line runtime log ends during another controller
menu confirmation. It contains **no final game-shutdown marker**;
a running-process log snapshot cannot prove clean exit. The line
`DDRAW: IAT target mismatch; hook not installed` occurred early but
the later DirectDraw7/D3D7 bootstrap was successful; do not infer a
crash or broken graphics from this isolated diagnostic. Game-over
confirmation and F9 screenshot output were not directly exercised
by the supplied log. Native Windows 11 and the read-only
`--verify`/existing-install `--install-only` paths still need tests.

**Final cleanup identified by this test:** Normal install used to
write all 293 `ISO-FILE` records to the installer log even when
`verbose=0`. The updated launcher preserves the count and detailed
Wise-copy messages but puts per-file inventory under `--verbose`,
while `--inspect` always emits its full independent CD inventory.
This changes logging only, not validation/extraction.

## Existing-install verification — November 14, 2026

The owner exercised `WINEDEBUG=-all mangohud wine SpiderMan2k1Fix.exe --verify`
under Wine; the terminal showed the supported retail EXE SHA-256 and
`END: status=0`. The attached `SpiderMan2k1Fix_installer(2).log`
records the separate `--install-only` invocation on an existing install,
which returned `END: status=0`, did not start the game and reported
`existing installation verified/updated`. **That earlier wording did not
prove that an upgrade occurred.**

Both uploaded runtime logs `SpiderMan2k1Fix(20261114-093029).log` and
`SpiderMan2k1Fix(20261114-093106).log` were **byte-identical**
(SHA-256 `bc1b1cc3d771ce86903e3da6eb44f7d16a4565fdfddcad0de94a5fff6c7e152d`,
2,215 lines). They therefore **do not constitute two independent
gameplay/exit tests**. This snapshot documents over 5,040 frames,
active 3D and menu rendering and two successful focus/voice resume
transitions (all documented `FOCUS-DSOUND` failed counts are zero).
The final recorded event is a menu controller confirmation, **not an
orderly game shutdown**. The early diagnostic
`DDRAW: IAT target mismatch; hook not installed` remains visible;
later DirectDraw7/D3D7 hooks and presentation succeeded.

**Follow-up code adjustment:** The installer now prints explicit
`VERIFY: OK` or `VERIFY: FAILED` result summaries for the original
EXE, install receipt and seven bundled resources, and distinguishes
`--install-only` **no update needed** from **authenticated resources
updated**. These summaries intentionally do **not** claim to verify all
original CD archives: that remains an independent release gate.

## Code-review findings that are NOT yet release-validated

- **Original game data integrity:** the current `--verify` gate verifies
  the exact original `SpideyPC.exe` SHA and the seven embedded/runtime
  assets, but does **not yet compare all eight original Wise Game Files**
  (`data.pkr`, `media.pkr`, `texture.dat`, etc.) with an authenticated
  per-file source hash/size receipt. A later-deleted data archive can escape
  this verifier. A full retail data receipt and read-only verification tests
  are needed before describing `--verify` as a complete game-integrity audit.
- **Original image acceptance:** the owner's authentic Spider-Man ISO
  was inspected and its exact `Setup/` source layout copied under Wine;
  the original EXE hash and installer-reported game-process exit code were
  observed, and a separate runtime trace proves real gameplay. Still
  unproven: all runtime functions on a clean native Windows 11 ISO install,
  a second image edition, registry/DirectX/INI independence on fresh
  prefixes, and an independently captured final clean game exit.
- **Negative tests:** static CLI checks and a successful cross-build are not
  substitutes for runnable synthetic/malformed ISO and CUE scenarios:
  missing files, duplicate names, forged extents, truncated media,
  unexpected reparse points, interrupted staged install and receipt
  mismatch/rollback need behavioral tests.
- **Platform smoke tests:** fresh Windows 11 and Wine prefixes need
  installation, --install-only on both fresh/existing, --verify, default
  launch, --launch-no-verify negative case, sound/video/controller/Alt+Tab,
  normal exit and both logger locations checked with a genuine disc image.
- **Release publishing:** the standalone main CI now publishes a candidate
  **single-EXE** Actions artifact, independently checked against the seven
  embedded payloads. The older DLL workflow retains a **developer-only**
  artifact, never the v2.0.0 release distribution. Public release packaging
  remains a separate explicit decision, without original CD data.

Historical reverse-engineering documentation and diagnostic utilities are
kept in source control but **not embedded in the release installer**. Avoid
deleting them as "obsolete" when active tests, runtime call sites or user
bug investigations still reference them.

## November 14, 2026 — pre-release cleanup follow-up

This is a **conservative source and CI audit**, not a change of the accepted
gameplay/rendering/input behavior:

- Examined the explicit DLL compilation inventory in `build.sh`: every root
  C translation unit is accounted for, either in `fix_sources` or as the
  independently built WinMM proxy. Do **not** delete diagnostic-looking C
  units, SDL3 guards, 4:3 UI code, native-audio paths or vendored miniz
  inflator solely on the basis of a historical name.
- Retired the obsolete **versioned DLL release ZIP builder** in the legacy
  Windows x86 workflow. GitHub Actions' developer-only DLL artifact remains
  available for compatibility investigations, while the release candidate
  artifact is exclusively the verified standalone EXE produced by the
  separate workflow. This avoids building and checksumming a second archive
  that is never uploaded or used by the release process.
- Adapted regression contracts to require absence of the old `dist/release`
  packaging, while preserving DLL, SDL3 upstream license/hash, Linux helper
  runtime and full regression checks in the older CI job.
- Corrected the public README, release notes, changelog and installer
  design: real original-ISO Wine-staging 11.17 inspection/install/gameplay
  has been observed; native Windows 11 original-ISO install is **not** yet
  established. The standalone launcher places both installer and game logs
  beside itself; only legacy direct game launches use the fallback.
- Record [#57](https://github.com/legluondunet/SpiderMan2k1Fix/issues/57)
  as closed following SDL3 integration, with unimplemented Chase Venom
  automatic 20 FPS limiting tracked in
  [#152](https://github.com/legluondunet/SpiderMan2k1Fix/issues/152).
  The installer implementation [#149](https://github.com/legluondunet/SpiderMan2k1Fix/issues/149)
  is closed after merging #150/#151, **not** because the stable release
  validation gate has been waived.

**Historical release-gate snapshot (superseded by PRs #154/#155):**
the eight-file game receipt and synthetic Windows executable negative-path
checks are now implemented, and native Windows 11 original-ISO installation
has been observed. See the follow-up sections below. A final authentic
retail-ISO smoke of the new receipt build and manual publication decision
are still required; no v2.0.0 tag/release is created by this audit.

## Follow-up: eight-file original-game integrity (PR #154)

The original-installer SHA-256 gap is addressed in the source candidate.
A new `SpiderMan2k1Fix/installed-game.sha256` receipt, separate from
`installed-resources.sha256`, stores the byte size and SHA-256 of all
**eight** files defined by the authenticated Wise full-install manifest
(`SpideyPC.exe`, `data.pkr`, `media.pkr`, `texture.dat`, `binkw32.dll`,
`Anet.inf`, `SpideyPC.ico`, `Spider-Man Setup.exe`).

- On initial ISO/CUE installation, the game receipt is written and verified
  in the private `.installing` directory **before** publishing the install.
- Routine `--verify` and launch verify both receipts and all fifteen files
  read-only. Unknown, missing, truncated or modified original data is refused,
  not repaired, and original saves and user INI remain untouched.
- Old prototype installations **without** the game receipt can enroll only
  under explicit `--install-only` when exactly one retail ISO/CUE is placed
  beside the launcher. A shared Wise source-mapping validator locates one
  consistent folder, then streams SHA-256 directly from the image and compares
  source size/hash with all eight installed files. The original source EXE
  must independently match its known retail SHA-256. An existing invalid
  receipt is never overwritten.
- The hash record provides **integrity against the selected owner-supplied
  image**, not independent verification of the other seven media assets
  against an externally authenticated checksum list. The original supported
  EXE remains independently SHA-256 locked.
- The new code still requires Windows x86 cross-build and integration
  validation. Malformed-media/rollback **executable behavior tests** are
  tracked as a separate follow-up; source-level assertions do not establish
  their runtime behavior.
- The maintainer's November 14 Windows 11 native log now establishes a
  real original ISO install, 1920x1200 DirectDraw/Direct3D display and SDL3
  input/focus activity, plus launcher game exit code 0. This corrects the
  earlier historical statement that only Wine ISO runs had been observed;
  it does **not** retroactively test this new receipt implementation.

## Executable negative-path CI (follow-up to game-integrity PR)

The standalone installer CI now builds and audits the **real public PE32
candidate** on Linux, then executes that binary on a Windows Server 2022
runner against generated, non-commercial ISO9660/CUE data. In parallel it
creates a distinctly named `SpiderMan2k1Fix-TEST-ONLY.exe` from an
**ephemeral, uncommitted source copy** whose approved EXE hash is replaced
with a deterministic synthetic fixture hash. That copy also exposes one
intentional mid-update failure point. Neither test substitution exists in
the production source or in the 30-day standalone release-candidate asset;
the fixture artifact has a one-day retention limit.

The Windows process-level tests cover:

- Read-only `--inspect` on a valid minimal ISO and MODE1/2352 CUE/BIN.
- Production rejection of a forged `SpideyPC.exe` with the right length
  but the wrong SHA-256, including complete first-install staging cleanup.
- Missing required Wise file, duplicate case-insensitive source names,
  forged out-of-volume extents, truncated/corrupt descriptors, missing BIN,
  CUE parent-directory traversal and ambiguous adjacent ISO images.
- Successful synthetic `--install-only` and `--verify` with the fixture
  binary, changed `data.pkr` rejection, read-only verification, and
  source-ISO-confirmed legacy game-receipt enrollment.
- Refusal to replace a present but corrupted game receipt.
- Deterministic injection of a failure after several resource updates,
  asserting exact byte-for-byte restoration of the old runtime components,
  removal of transient `.bak`/staging files and preserved original game
  data. A subsequent normal upgrade and `--verify` must succeed.

No official game or copyrighted assets are used in these automated
tests. They do not replace a final smoke test using the authentic retail
image, nor do they prove every possible system failure mode (disk
removal, power loss, antivirus interference). **Never release the TEST-ONLY
binary.**
