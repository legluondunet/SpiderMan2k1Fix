# SpiderMan2k1Fix — unified single-DLL architecture (draft)

**Development only — not a playable candidate, not a release.** This proposal
implements the layout approved on 17 November 2026 while preserving the
previous working version until the complete startup and installer transition
passes automated and on-device Windows/Wine tests.

## Target on-disk layout

```text
spiderman2k1fix/
  SpiderMan2k1Fix.exe             # mandatory installer/verifier/launcher
  SpiderMan2k1Fix.ini             # single user configuration
  SpiderMan2k1Fix.log
  SpiderMan2k1Fix_installer.log
  SpiderMan2k1Fix_inspect.log     # generated only by --inspect
  state/
    installed-game.sha256
    installed-resources.sha256
  docs/
    README.md
    CHANGELOG.md
    LICENSE
  third_party/
    SDL3/
      SDL3.dll
      LICENSE.txt
  linux/
    spiderman-focus-helper
  game/
    SpideyPC.exe                  # original SHA-approved retail EXE
    fix.dll                       # only project DLL
    binkw32.dll
    data.pkr
    media.pkr
    texture.dat
    ...                           # remaining authentic Wise assets/saves
```

No empty `mods/` directory: create it only if modding becomes a supported
feature. No automatic migration from older flat/nested installation layouts.
No original game content is distributed.

## Startup contract

1. The launcher verifies the original executable's complete supported hash,
   checks the embedded DLL payload and receipts and opens `SpideyPC.exe` with
   `CREATE_SUSPENDED`.
2. While the main thread is still suspended, verify the exact mapped PE32
   image, the original DirectDraw import descriptor and its original import
   name bytes before applying an *in-process-only* same-length alias
   `DDRAW.dll\0` to `fix.dll\0\0\0`. Preserve the original on-disk EXE.
   The existing original-game No-CD patch must also be performed before
   the game thread runs, with all original instruction signatures checked.
3. `game/fix.dll` is loaded by the original Windows/Wine loader. Its
   `DllMain` must not create threads, perform file I/O or call LoadLibrary.
4. On the FIRST actual `DirectDrawCreateEx` call, `fix.dll` resolves the
   native system DirectDraw provider, synchronously authenticates the EXE,
   completes the core fix setup and delegates to the existing verified
   DirectDraw7 virtualization. There must be no unprotected 640×480
   fullscreen fallback on failure.
5. The launcher is required: running `game/SpideyPC.exe` directly does not
   enable the fix.

The proven early native DirectSound NULL cleanup guard from **draft PR #161**
must be preserved when that fix is integrated. This migration must never
claim to solve the upstream intermittent `SetCooperativeLevel E_FAIL`; issue
#160 remains independent and open. Retain `DDSCL_NORMAL` and the original
HRESULT. Do not blindly retry, force an appcompat bypass, or suppress a failed
graphics initialization.

## Transactional installation and verification

- `state/` is installer-owned and contains ONLY the two integrity receipts.
  Their hashes must describe original `game/` assets and the separately
  located project-owned `fix.dll`, `third_party/SDL3/`, `linux/` and
  `docs/` assets. The receipts are not user documentation.
- Stage all new files and *both* receipts together. Publish receipts only
  after their targets exist; a partially published installation MUST fail
  `--verify` without overwriting user content. Failed replacement must
  either roll back completely or retain the data for explicit recovery.
- Never overwrite an unknown DLL or user-edited game/INI/save file.
  `--verify` is read-only and `--install-only` is the explicit update path.
- `docs/` contains README, changelog and project license. Keep SDL3's
  own license next to `third_party/SDL3/SDL3.dll`.
- `linux/spiderman-focus-helper` lives in the launcher root's Linux folder,
  not under `game/`.
- Screenshots and controller profiles need explicit user-data preservation,
  writable-path checks and native/Wine regression coverage before changing
  their established paths.

## Independent implementation gates

1. Build `fix.dll` with the original game-fix source code, plus the DirectDraw
   factory export. No WinMM proxy or second project DLL in the final package.
2. Implement a fail-closed PE32 import-name alias and in-memory No-CD bootstrap
   on the suspended game process; verify against the genuine supported EXE.
3. Rehome all runtime paths and the bundled installer payload and implement
   atomic multi-directory receipts/upgrades, with corrupt-state recovery.
4. Add deterministic synthetic Windows PE32 tests, an actual import-alias smoke
   test under Windows/Wine and negative unsupported-EXE tests.
5. Test on the owner's original game: menus, cinematics, music and SFX, HUD,
   4:3/widescreen, SDL3 pads, Alt+Tab, focus helper, saves, screenshots,
   DirectDraw errors and shutdown, both Windows 11 and Wine.

**Do not merge until all gates pass.** No release implied.

## Progress — 17 November 2026

The experimental compile path now produces a real PE32 x86 standalone
installer bundling a single `game/fix.dll`, pinned SDL3 runtime and license,
project documentation and native Linux focus observer. It retains the original
retail EXE hash gate, includes a suspended-process import-alias and No-CD
bootstrap, verifies the two `state/` receipts, and stages all five top-level
directories before ordered publication with rollback. Old installations are
neither adopted nor automatically renamed.

[GitHub Actions #44083007598](https://github.com/legluondunet/SpiderMan2k1Fix/actions/runs/44083007598)
passed the unified PE32 build and native Windows synthetic ISO integration
tests, including forced failure during directory publication, rollback,
tampered SHA receipts, user-modified data and refusal to overwrite an existing
directory. Original v2.0 WinMM installer tests also remain green.

**Still not accepted:** these synthetic tests deliberately use a non-runnable
fake game and do not exercise the real import alias with the original
executable. The DirectSound NULL primary-buffer guard of PR #161 is not yet
integrated into the unified process bootstrap. Cross-platform gameplay,
SDL3, focus and audio tests remain necessary before merge.
