"""Installer 2.0.0 prototype must not ship or pretend to install retail assets.

The first builder packages only redistribution-safe project components.
Real-disc InstallShield/CAB recreation is a separate release blocker (#149).
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
    assert "CD setup archive recipe not yet verified" in SRC
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
    existing=main.split("if(argc==1 && regular_file(gameexe))",1)[1].split(
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


def test_original_cd_install_manifest_is_not_claimed_complete():
    """PKR payload listing is not evidence of a retail InstallShield recipe."""
    assert "CD setup archive recipe not yet verified" in SRC
    assert "CD image cannot yet be installed safely" in SRC
    assert "Original retail game data is NEVER embedded" in SRC
