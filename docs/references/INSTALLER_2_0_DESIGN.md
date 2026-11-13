# SpiderMan2k1Fix 2.0.0 — standalone installer design and evidence gate

The maintainer requested one executable beside their own retail ISO, on the
same model as Rayman2Fix. This is a **release blocker**, not a promise that the
original disc is already supported by the first bootstrap executable.

## Required user-facing layout

```text
SpiderMan2k1Fix.exe
Spider-Man.iso
SpiderMan2k1Fix_installer.log      # one-time install/verification
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
    SpiderMan2k1Fix.ini            # created by game, user-mutable
    linux/spiderman-focus-helper  # native observer; optional under Wine
```

The bundled payload must not contain `SpideyPC.exe`, `data.pkr`, `media.pkr`,
any disc assets, save data, or the ISO. The exact supported retail executable
is 1,507,328 bytes, SHA-256
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.

## One-time installer prototype in draft PR #150

- Native PE32 Windows/Wine console launcher, `--help`, `--version`, `--inspect`,
  `--verify`, `--install-only`, `--launch-no-verify`, and zero-argument automatic detection.
- `--install-only` uses the same strict original-ISO discovery, Wise
  complete-install file map, game SHA-256 validation and atomic staging as
  the automatic first launch, but **does not call the game process** afterward.
  If the installation already exists, it verifies the original EXE and
  embedded runtime components read-only and exits successfully; on any
  mismatch it fails without updates or repairs. The command never reinstalls
  or overwrites existing game files. If the ISO is absent or invalid, it fails
  with a nonzero exit and a diagnostic in `SpiderMan2k1Fix_installer.log`.
- `--launch-no-verify` explicitly runs `SpiderMan2k1Fix/SpideyPC.exe`
  without any **launcher-side** existence check, SHA-256 hash verification,
  companion resource checks or ISO discovery/installation. It does **not**
  recreate missing game files, install/update the bundled DLLs, override the
  operating system loader or disable the game DLLs' own original-binary
  compatibility guards. If the EXE is missing, `CreateProcessW` fails and
  the installer log reports the Win32 error. Normal automatic startup and
  `--verify` remain unchanged and strict.
- Strictly bounded ISO9660/Joliet file inspection. Reject unknown/cyclic
  records, invalid file extents, unsafe relative names, multiple adjacent
  `.iso` files and unsupported raw-sector formats. Do not mount ISO or
  execute the CD `Autorun.exe` / `Setup.exe`.
- Exact retail executable SHA-256 check before adopting or launching any
  game folder; fail closed on all mismatches.
- Deploy project-owned/redistributable resources from embedded RCDATA
  **once**, inside the same private staging directory as the validated
  original CD files; publish the complete tree only after all assets pass.
  No DLL backup, upgrade, repair or overwrite of an existing installation.
  Existing installs are **verified read-only and launched**; missing or
  changed runtime files trigger an explicit refusal.
- Game working directory is the install directory. On Wine the game child
  receives `winmm=n,b` without altering the user's system registry.
- Both logs are beside `SpiderMan2k1Fix.exe`: launcher resets only
  `SpiderMan2k1Fix_installer.log`; the game child inherits the validated
  launcher directory through `SPIDERMAN2K1FIX_LAUNCHER_DIR`, and
  both the winmm proxy and core share `SpiderMan2k1Fix.log`.
  A direct legacy `SpideyPC.exe` run retains its original log fallback.
- An original **Wise** script, recovered from the uploaded 5,174,358-byte
  installer, lists all needed **loose** Game Files under `%INST%\\...`.
  The prototype now locates exactly one directory containing the entire
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

We **still do not have the actual retail Spider-Man ISO**. The Wise
source prefix `%INST%` identifies the directory of the source installer,
but does not prove its concrete path within the ISO. Nor have the original
ISO file sizes or the registry independence been checked in a new Wine
prefix or on Windows 11. The uploaded installer script is strong evidence
for the copy recipe, **not proof of a working complete installation**.

More technical findings and SHA-256 provenance:
[Original Wise installer evidence](SPIDERMAN_RETAIL_WISE_INSTALL_MAP.md).
No game EXEs, PKR archives, installation archives, or ISO image are
committed or distributed with this project.

## Release gate

1. Original Wise full-install file map recovered from uploaded PE32 setup;
   **actual ISO folder/source sizes and Windows/Wine runtime still unverified**.
2. Synthetic ISO tests (valid/Joliet, malformed record/extent, bad hash,
   multiple images, tampered embedded resources, failed stage/rollback).
3. One-time embedded payload deployment; existing installation is verified
   read-only and never updated/repaired. No DLL `.bak` is created.
4. Actual one-EXE Windows x86 CI artifact with verified embedded resources.
5. Independent Windows 11 and Wine installation, normal launch and shutdown.
6. Only then merge the installer and publish tag `v2.0.0`.

