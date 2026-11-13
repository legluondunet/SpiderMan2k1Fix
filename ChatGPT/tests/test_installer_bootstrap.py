"""Installer 2.0.0 bootstrap: standalone resources and owner-proven Wise map.

The build embeds only redistributable project components. The original
Wise script identifies a complete Game Files superset, but authentic ISO
source layout and end-to-end Windows/Wine runtime testing remain release
blockers (#149).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PATHS = (ROOT / "spider_fix_paths.h").read_text(encoding="utf-8")
SRC = (ROOT / "installer/spiderman2k1fix_installer.c").read_text(encoding="utf-8")
BUILD = (ROOT / "scripts/build_installer.sh").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/spiderman2k1fix-installer-prototype.yml").read_text(encoding="utf-8")


def test_installer_names_and_paths():
    for name in ("SpiderMan2k1Fix.exe", "SpideyPC.exe", "SpiderMan2k1Fix_installer.log",
                 "SpiderMan2k1Fix.log", "SpiderMan2k1Fix.dll", "winmm.dll", "SDL3.dll",
                 "spiderman-focus-helper", "SpiderMan2k1Fix.installing"):
        assert name in SRC or name == "SpiderMan2k1Fix.log"
    assert "--inspect" in SRC and "--verify" in SRC
    assert 'SPIDERMAN2K1FIX_MENUBG43_TRIAL' not in SRC
    assert "CreateProcessW" in SRC


def test_installer_retail_validation_and_fail_closed():
    assert "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea" in SRC
    assert "RETAIL_EXE_BYTES 1507328ULL" in SRC
    assert "CryptGetHashParam" in SRC
    assert "if(!verified_retail(target))" in SRC or "!verified_retail(target)" in SRC
    assert "!verified_retail(gameexe)" in SRC
    assert "Original retail game data is NEVER embedded" in SRC
    assert "original Wise Game Files source" in SRC
    assert "ISO: unsafe or unsupported directory records" in SRC
    assert "ISO: expected one adjacent .iso" in SRC
    assert "MoveFileExW(staging,game_dir,MOVEFILE_WRITE_THROUGH)" in SRC
    assert "FILE_ATTRIBUTE_REPARSE_POINT" in SRC


def test_project_payload_inventory():
    required = ["1101 RCDATA", "1102 RCDATA", "1103 RCDATA", "1104 RCDATA",
                "1105 RCDATA", "1106 RCDATA", "1107 RCDATA"]
    for resource in required:
        assert resource in BUILD
    assert "dist/package/SpiderMan2k1Fix/SDL3.dll" in BUILD
    assert "dist/package/SpiderMan2k1Fix/linux/spiderman-focus-helper" in BUILD
    assert "-municode" in BUILD
    assert "-static-libgcc" in BUILD
    assert "grep -q '\\.rsrc'" in BUILD
    assert "Installer-Prototype" in WORKFLOW
    assert "SpiderMan2k1Fix.exe" in WORKFLOW




def test_both_logs_live_next_to_standalone_launcher_only():
    """winmm/core share a runtime log, not the installer's diagnostic log."""
    assert 'L"SpiderMan2k1Fix_installer.log"' in SRC
    assert 'SpiderMan2k1Fix.log' in SRC
    assert '"SpiderMan2k1Fix.log"' in PATHS
    assert 'SetEnvironmentVariableW(L"SPIDERMAN2K1FIX_LAUNCHER_DIR",launcher_dir)' in SRC
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_LAUNCHER_DIR"' in PATHS
    resolver=PATHS.split("static inline int spider_path_log_a(",1)[1].split(
        "static inline int spider_path_code_asset_a(",1)[0]
    assert "spider_path_dir_a(launcher,0)" in resolver
    assert "spider_path_join_a(target,sizeof(target),launcher," in resolver
    assert "SpiderMan2k1Fix.log" in resolver
    assert "spider_path_exe_dir_a(exe" in resolver  # legacy game-direct fallback
    assert "SPIDERMAN2K1FIX_LAUNCHER_DIR" not in BUILD


def test_standalone_installer_never_upgrades_existing_installation():
    """The only writes of project payloads occur in private ISO staging."""
    assert "deploy_payloads_to(staging)" in SRC
    assert "MoveFileExW(staging,game_dir,MOVEFILE_WRITE_THROUGH)" in SRC
    main=SRC.split("int wmain(",1)[1]
    existing=main.split("if((argc==1 || install_only) && regular_file(gameexe))",1)[1].split(
        "scan=find_adjacent_iso",1)[0]
    assert "verified_retail(gameexe)" in existing
    assert "verify_payloads()" in existing
    assert "deploy_payloads_to(" not in existing
    assert "resource_install(" not in existing
    assert "CreateProcessW" in SRC
    assert "MOVEFILE_REPLACE_EXISTING" not in SRC
    assert "previous DLL backup=" not in SRC
    assert "no automatic change" in SRC
    assert 'if(!deploy_payloads_to(staging))' in SRC
    assert "if(!MoveFileExW(staging,game_dir,MOVEFILE_WRITE_THROUGH))" in SRC
    assert "resource_verify(RES_WINMM,game_dir" in SRC


def test_original_wise_game_files_superset_and_same_cd_folder():
    """Source-side scan of the user-owned Wise installation script:
    the second installation-size path includes media.pkr and texture.dat.
    Avoid mixing contents from two directories/editions of the CD."""
    manifest=SRC.split("static int prepare_game_from_iso(",1)[1].split(
        "static int resource_bytes(",1)[0]
    for name in (
        "SpideyPC.exe", "data.pkr", "media.pkr", "texture.dat",
        "binkw32.dll", "Anet.inf", "SpideyPC.ico",
        "Spider-Man Setup.exe",
    ):
        assert f'L"{name}"' in manifest
    assert "same single directory" in manifest.lower()
    assert 'wcsrchr(iso->entries[n].name,L\'/\')' in manifest
    assert "ISO: ambiguous SpideyPC.exe source directories" in manifest
    assert "RETAIL_EXE_BYTES" in manifest
    assert "find_disc_file(iso,source)" in manifest
    assert "if(!parts[i]->bytes)" in manifest
    assert "!verified_retail(target))goto abort;" in manifest
    assert "deploy_payloads_to(staging)" in manifest
    assert "MoveFileExW(staging,game_dir,MOVEFILE_WRITE_THROUGH)" in manifest
    assert "MOVEFILE_REPLACE_EXISTING" not in manifest
    assert "Spidey.ini was installed into %WIN%" in manifest
    assert "SetEnvironmentVariableW" not in manifest  # no system writes
    assert "iso_copy(iso,parts[i],target)" in manifest


def test_wise_forensic_reference_is_based_on_supplied_original_setup():
    ref=(ROOT / "docs/references/SPIDERMAN_RETAIL_WISE_INSTALL_MAP.md").read_text(
        encoding="utf-8")
    assert "20bea9fb79f10a2f693fb7f103fa6097d572866d6353b92d94accb080e6e9f16" in ref
    assert "38,646-byte" in ref or "38,646 bytes" in ref
    assert "two game-copy lists" in ref
    assert "`media.pkr`" in ref
    assert "Spidey.ini" in ref
    assert "No actual original Spider-Man CD ISO" in ref or "actual original Spider-Man CD ISO" in ref


def test_original_retail_iso_and_windows11_validation_are_still_pending():
    """The owner uploaded setup EXEs but no original disc; do not publish."""
    assert "We still require real retail ISO and Windows/Wine smoke tests" in SRC
    assert "ISO: original Wise source game executable absent" in SRC
    assert "ISO: missing original Wise Game Files source" in SRC
    assert "Original retail game data is NEVER embedded" in SRC


def test_no_verify_launch_skips_all_launcher_filesystem_guards():
    """Explicit bypass only; default verify/install branches stay strict."""
    assert 'L"--launch-no-verify"' in SRC
    assert "static int launch_game(int verify_exe)" in SRC
    launch=SRC.split("static int launch_game(int verify_exe)",1)[1].split(
        "static void usage(",1)[0]
    assert 'if(verify_exe && !verified_retail(path))return 0;' in launch
    assert "CreateProcessW(path,cmd" in launch
    main=SRC.split("int wmain(",1)[1]
    cli=main.split('!wcscmp(argv[1],L"--launch-no-verify")',1)[1].split(
        'if(!combine(gameexe',1)[0]
    assert 'result=launch_game(0)?0:1;goto done;' in cli
    assert "verified_retail(" not in cli
    assert "verify_payloads(" not in cli
    assert "regular_file(" not in cli
    assert "find_adjacent_iso(" not in cli
    assert "prepare_game_from_iso(" not in cli
    assert main.index('L"--launch-no-verify"') < main.index('if(!combine(gameexe')
    assert 'result=launch_game(1)?0:1;' in main
    # Explicit no-verify neither installs nor attempts repair.
    assert "no automatic change" in SRC
    assert "deploy_payloads_to(staging)" in SRC
    assert "SPIDERMAN2K1FIX_LAUNCHER_DIR" in launch



def test_install_only_runs_iso_install_without_game_launch():
    """Explicit install-only follows normal safe ISO installation and exits."""
    assert 'L"--install-only"' in SRC
    assert 'install_only=argc==2 && !wcscmp(argv[1],L"--install-only");' in SRC
    assert 'if(argc==2 && !install_only)' in SRC
    assert 'if(inspect){' in SRC
    main=SRC.split("int wmain(",1)[1]
    assert main.index('install_only=argc==2') < main.index('scan=find_adjacent_iso(')
    assert 'if(!prepare_game_from_iso(&iso))' in main
    installed=main.split('if((argc==1 || install_only) && regular_file(gameexe))',1)[1].split(
        'scan=find_adjacent_iso(',1)[0]
    assert 'verified_retail(gameexe)' in installed
    assert 'verify_payloads()' in installed
    assert 'if(install_only){' in installed
    assert 'result=0;goto done;' in installed
    assert installed.index('if(install_only){') < installed.index('result=launch_game(1)?0:1;')
    assert 'resource_install(' not in installed
    assert 'deploy_payloads_to(' not in installed
    fresh=main.split('if(!prepare_game_from_iso(&iso))',1)[1]
    assert 'if(install_only){' in fresh
    assert 'INSTALL: --install-only complete; game was not launched' in fresh
    assert fresh.index('if(install_only){') < fresh.index('result=launch_game(1)?0:1;')
    assert 'result=launch_game(0)?0:1;goto done;' in main  # existing bypass unchanged
    assert 'result=launch_game(1)?0:1;' in main  # normal default unchanged



def test_standard_inspect_commands_and_independent_report():
    assert 'L"--inspect"' in SRC
    assert 'L"SpiderMan2k1Fix_inspect.log"' in SRC
    assert 'L".iso"' in SRC and 'L".cue"' in SRC
    assert 'L".bin"' in SRC and 'L".img"' in SRC
    assert "cue_source(" in SRC
    assert "ISO9660/Joliet" in SRC
    assert "sector*iso->physical_sector_bytes+iso->sector_payload_offset" in SRC
    assert "INSPECT_RESULT=COMPLETE" in SRC
    assert "INSPECT_RESULT=FAILED" in SRC
    source=SRC.split("int wmain(",1)[1]
    assert source.index('if(inspect){') < source.index('verified_retail(gameexe)')
    inspect=source.split('if(inspect){',1)[1].split('if(argc>2)',1)[0]
    assert "prepare_game_from_iso" not in inspect
    assert "deploy_payloads_to" not in inspect
    assert "launch_game(" not in inspect
    assert "iso_open(&iso,path,*raw?raw:NULL)" in inspect
    assert 'L"-h"' in SRC
    assert 'L"--verbose"' in SRC
    assert "verbose_mode=1" in SRC
    # No naked ISO/CUE argument is ever accepted in non-inspect CLI dispatch.
    assert "if(argc>2){usage();goto done;}" in SRC
    assert "if(argc==2 && !install_only)" in SRC
