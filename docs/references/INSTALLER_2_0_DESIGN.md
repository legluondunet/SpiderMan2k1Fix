# SpiderMan2k1Fix 2.0.0 — standalone installer design and evidence gate

The maintainer requested one executable beside their own retail ISO, on the
same model as Rayman2Fix. The source implementation is **merged** (PRs #150
and #151). An authentic retail ISO was installed and played under Wine; a
native Windows 11 original-ISO end-to-end test and complete CD-data verification
remain release-acceptance items.

## Required user-facing layout

```text
SpiderMan2k1Fix.exe
Spider-Man.iso
SpiderMan2k1Fix_installer.log      # install, verification and explicit upgrades
SpiderMan2k1Fix_inspect.log        # separate unsupported-CD diagnostic only
SpiderMan2k1Fix.log                # game runtime log (same launcher directory)
SpiderMan2k1Fix/
  SpideyPC.exe                     # SHA256-confirmed original retail game
  winmm.dll                        # built-in proxy from project
  data.pkr, media.pkr, texture.dat # full Wise Game Files set from user ISO
  binkw32.dll, Anet.inf, SpideyPC.ico, Spider-Man Setup.exe
  # game log is beside the launcher, NOT inside this folder
  SpiderMan2k1Fix/
    SpiderMan2k1Fix.dll            # built-in fix DLL
    SDL3.dll                       # verified official SDL redistributable
    SDL3-LICENSE.txt
    README.md, CHANGELOG.md
    installed-resources.sha256     # exact managed-runtime provenance
    installed-game.sha256          # eight original game file sizes + SHA-256
    SpiderMan2k1Fix.ini            # created by game, user-mutable
    linux/spiderman-focus-helper  # native observer; optional under Wine
```

The bundled payload must not contain `SpideyPC.exe`, `data.pkr`, `media.pkr`,
any disc assets, save data, or the ISO. The exact supported retail executable
is 1,507,328 bytes, SHA-256
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.

## Standalone installer merged from PR #150

- Native PE32 Windows/Wine launcher with identical public commands to
  the other Fix installers: `--help`/`-h`, `--version`, `--verify`,
  `--install-only`, `--launch-no-verify`, `--verbose` and
  `--inspect image.iso | image.cue [track.bin|track.img]`.
  Positional ISO/CUE installation parameters are not accepted.
  Normal no-argument mode discovers exactly one adjacent ISO **or CUE**
  image, which references its BIN/IMG track inside its own CUE metadata.
- `--inspect` reads ISO9660/Joliet or an explicit CUE/BIN/IMG data track
  without launching or installing the game. It writes the **separate**
  `SpiderMan2k1Fix_inspect.log` beside the launcher, including failures,
  to help diagnose new CD editions. `--verbose` adds diagnostic output.
- `--install-only` performs strict original-ISO discovery, the full Wise
  Game Files installation and original game SHA-256 preflight without
  starting the game. For existing supported game executables, it verifies
  the installed game and can **explicitly upgrade the seven installer-owned
  runtime/docs components** when their previous SHA-256 receipt proves their
  origin and intact bytes. It stages complete new payloads and receipt,
  backs up only known files, and attempts rollback on any failed publish.
  Foreign, missing or user-modified prior managed files are **not**
  overwritten; game EXE, CD data, user INI and saved games are not part of
  this upgrade. An installation without the original managed-resource
  receipt is **rejected** rather than automatically adopted from an obsolete
  test prototype. Normal startup remains verify-and-launch, never updater.
- `--launch-no-verify` explicitly runs `SpiderMan2k1Fix/SpideyPC.exe`
  without any **launcher-side** existence check, SHA-256 hash verification,
  companion resource checks or ISO discovery/installation. It does **not**
  recreate missing game files, install/update the bundled DLLs, override the
  operating system loader or disable the game DLLs' own original-binary
  compatibility guards. If the EXE is missing, `CreateProcessW` fails and
  the installer log reports the Win32 error. Normal automatic startup and
  `--verify` remain unchanged and strict.
- Bounded ISO9660/Joliet and read-only CUE raw-sector inspection. Reject
  invalid extents, cyclic records, unsafe names and ambiguous adjacent
  images. No CD mounting or execution of legacy `Autorun.exe`/`Setup.exe`.
- Exact retail executable SHA-256, seven-resource receipt and eight-original-file
  SHA-256/size receipt required before verifying or launching an installed
  game. Earlier standalone prototypes without a game receipt require
  `--install-only` with the original adjacent ISO/CUE. Each of the eight
  existing game files is SHA-compared directly against the image; an
  invalid/present receipt is never overwritten. This proves equivalence
  to that image, **not independently authenticated PKR hashes**.
- First installation atomically publishes the verified original files,
  embedded components and both `installed-resources.sha256` and
  `installed-game.sha256` receipts. Subsequent **normal launches** and
  `--verify` require both receipts and hash all eight game files plus
  seven owned resources; only explicitly requested `--install-only`
  may update receipt-authenticated components.
  Unknown files or third-party binaries are never silently replaced.
- Game working directory is the install directory. On Wine the game child
  receives `winmm=n,b` without altering the user's system registry.
- Both logs are beside `SpiderMan2k1Fix.exe`: launcher resets only
  `SpiderMan2k1Fix_installer.log`; the game child inherits the validated
  launcher directory through `SPIDERMAN2K1FIX_LAUNCHER_DIR`, and
  both the winmm proxy and core share `SpiderMan2k1Fix.log`.
  A direct legacy `SpideyPC.exe` run retains its original log fallback.
- An original **Wise** script, recovered from the uploaded 5,174,358-byte
  installer, lists all needed **loose** Game Files under `%INST%\\...`.
  The launcher locates exactly one directory containing the entire
  eight-file **full-install superset**, including `media.pkr` and
  `texture.dat`; it rejects missing, duplicated and cross-directory source
  files rather than publishing a partial installation.
- The Wise installer also offers an optional Help Files group and performs
  registry/shortcut/system INI/DirectX operations. We do not execute it or
  perform these legacy side effects. Actual gameplay has been observed
  after original-ISO installation under Wine-staging 11.17; clean native
  Windows 11 and fresh-prefix registry-independence tests remain.
- See the owner-binary-derived file and registry map:
  [SPIDERMAN_RETAIL_WISE_INSTALL_MAP.md](SPIDERMAN_RETAIL_WISE_INSTALL_MAP.md).

## Original install source: reconstructed Wise script

The project maintainer has now supplied the original `Autorun.exe`,
two `Setup.exe` variants, the main 5.17 MB Wise `Spider-Man Installation`
package, the display-adapter selector and their INI/INF files. All uploaded
EXEs are native **PE32**, not 16-bit NE programs. Offline parsing of
the Wise overlay, with no execution, recovered a **38,646-byte** install
script and an exact source-to-destination `Game Files` inventory.

The original script offers a small installation with `data.pkr` and
a larger installation which **also copies `media.pkr`**. The default
installation was the reason Lutris separately copied `Setup/media.pkr`.
The new standalone installer is configured to install the **complete**
list from one consistent ISO directory, including `texture.dat` and
original support files, as established by the script.

This paragraph originally described the pre-ISO reverse-engineering stage.
On November 14, 2026, the maintainer supplied successful Wine inspection,
installation and gameplay logs from an authentic retail Spider-Man ISO.
The reader observed the concrete `Setup/` source directory, copied the
eight-file Wise full-install list, and verified the exact original EXE hash.
These field observations **do not** certify a native Windows 11 clean install,
a complete source-authenticated game-data receipt or all negative paths.
See `RELEASE_2_0_CODE_CLEANUP_AUDIT.md` for the reviewed evidence.

More technical findings and SHA-256 provenance:
[Original Wise installer evidence](SPIDERMAN_RETAIL_WISE_INSTALL_MAP.md).
No game EXEs, PKR archives, installation archives, or ISO image are
committed or distributed with this project.

## Release gate after merging #150 and #151 (November 14, 2026)

1. The authentic Wise `Setup/` original-disc source layout and eight-file
   installation succeeded under Wine. The original executable SHA-256 matched.
2. **Implemented in the integrity follow-up:** separate game-data receipt
   with SHA-256/size for all eight Wise retail files; explicit legacy
   enrollment is anchored to a matching adjacent ISO/CUE and the supported
   original EXE hash. Runtime smoke testing of this change is still required.
3. **Passed in merged PR #155:** real Windows PE32 executable tests cover
   malformed ISO/CUE, missing/duplicate source files, forged extents,
   interrupted first-install staging, receipt tampering, ISO-based enrollment
   and a deterministic mid-update rollback with byte-for-byte restoration.
   These tests use synthetic assets and a distinctly labeled TEST-ONLY fixture;
   they cannot establish resilience to every real power-loss/filesystem fault.
4. The [merged-main standalone CI](https://github.com/legluondunet/SpiderMan2k1Fix/actions/runs/43583076518)
   **passed** PE32 x86 verification and byte-for-byte matching of all
   seven embedded RCDATA payloads, and recorded the EXE SHA-256
   `5d3c87015e72c37ed43f6d08f0e43d67c7c30017e70c7fef881b1c7e5063700e`.
   The developer DLL workflow remains useful, but its unused pre-2.0
   versioned release ZIP is now retired. It is not the stable installer.

5. Newer Windows 11 logs demonstrate native original-ISO installation,
   graphics, SDL3 controller selection, audio and Alt+Tab; the launcher
   reported a game process exit code of 0. The runtime log does not
   contain a distinct clean-shutdown marker.
6. Publication of `v2.0.0` is a separate, explicit action only after
   the remaining acceptance limitations have been assessed. The CI
   candidate workflow does **not** tag or publish releases.
