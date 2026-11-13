"""Installer 2.0.0 prototype must not ship or pretend to install retail assets.

The first builder packages only redistribution-safe project components.
Real-disc InstallShield/CAB recreation is a separate release blocker (#149).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
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

