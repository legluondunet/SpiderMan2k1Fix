# SpiderMan2k1Fix 2.0.0 — standalone installer design and evidence gate

The maintainer requested one executable beside their own retail ISO, on the
same model as Rayman2Fix. This is a **release blocker**, not a promise that the
original disc is already supported by the first bootstrap executable.

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
- Exact retail executable SHA-256 and managed-resource receipt required
  before verifying or launching an installed game; refuse unverifiable
  pre-release prototype directories rather than silently creating trust.
- First installation atomically publishes the verified original files,
  embedded components and an exact `installed-resources.sha256` receipt.
  Subsequent **normal launches** and `--verify` require this receipt and
  are verification-only; only explicitly requested `--install-only`
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
  perform these legacy side effects. Registry independence and actual
  gameplay still require clean Windows/Wine runtime testing.
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

## Release gate after merging #150 (November 14, 2026)

1. The authentic Wise `Setup/` original-disc source layout and eight-file
   installation succeeded under Wine. The original executable SHA-256 matched.
2. Before calling `--verify` a full data-integrity audit, add a provenance
   receipt and read-only hash checks for all eight retail game files, not
   only the original EXE and seven embedded project resources.
3. Exercise malformed ISO/CUE, missing source files, interrupted install,
   receipt tampering and rollback paths with executable automated tests.
4. The proposed standalone CI must confirm an actual PE32 x86 executable
   with **all seven embedded RCDATA resources matching their staged
   redistributable bytes**, and record the EXE SHA-256. Do not publish the
   legacy developer DLL ZIP as the stable 2.0.0 distribution.
5. The archived Wine gameplay log demonstrates actual graphics, audio,
   controllers and Alt+Tab, but not a separate clean-process-exit trace.
   A clean Windows 11 ISO installation was not demonstrated.
6. Publication of `v2.0.0` is a separate, explicit action only after
   the remaining acceptance limitations have been assessed. The CI
   candidate workflow does **not** tag or publish releases.
