# SpiderMan2k1Fix 2.0.0 — source cleanup and release gates

Status: **draft PR #150**, not yet a stable 2.0.0 release.
Audit performed November 13, 2026; source examined on
`feature/2.0.0-iso-installer-bootstrap`. Do **not** tag, merge or announce a
validated original-CD installer based solely on source review or CI.

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

## Code-review findings that are NOT yet release-validated

- **Original game data integrity:** the current `--verify` gate verifies
  the exact original `SpideyPC.exe` SHA and the seven embedded/runtime
  assets, but does **not yet compare all eight original Wise Game Files**
  (`data.pkr`, `media.pkr`, `texture.dat`, etc.) with an authenticated
  per-file source hash/size receipt. A later-deleted data archive can escape
  this verifier. A full retail data receipt and read-only verification tests
  are needed before describing `--verify` as a complete game-integrity audit.
- **Original image acceptance:** the owner's authentic Spider-Man ISO
  has now been inspected and its exact `Setup/` source layout copied
  successfully under Wine; the original EXE hash and game-process exit code
  were confirmed. Still unproven: full in-game runtime/session behavior,
  a second image edition, clean-prefix registry/DirectX/INI independence,
  and a native Windows 11 installation.
- **Negative tests:** static CLI checks and a successful cross-build are not
  substitutes for runnable synthetic/malformed ISO and CUE scenarios:
  missing files, duplicate names, forged extents, truncated media,
  unexpected reparse points, interrupted staged install and receipt
  mismatch/rollback need behavioral tests.
- **Platform smoke tests:** fresh Windows 11 and Wine prefixes need
  installation, --install-only on both fresh/existing, --verify, default
  launch, --launch-no-verify negative case, sound/video/controller/Alt+Tab,
  normal exit and both logger locations checked with a genuine disc image.
- **Release publishing:** the installer CI currently retains a separately
  named **prototype** EXE artifact, while the older Windows x86 CI retains
  a DLL ZIP for development/compatibility. The official v2.0.0 release must
  publish the **one-file installer** as its primary asset only after all
  above acceptance steps, without leaking original CD data.

Historical reverse-engineering documentation and diagnostic utilities are
kept in source control but **not embedded in the release installer**. Avoid
deleting them as "obsolete" when active tests, runtime call sites or user
bug investigations still reference them.
