"""Opt-in audio voice-slot opcode patch, not yet validated during gameplay."""
from pathlib import Path
import hashlib
import os
import re

ROOT = Path(__file__).resolve().parents[2]
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
VALID = (ROOT / "ini_validation.h").read_text(encoding="utf-8")
GENERATED = (ROOT / "virtual_spidey_cfg.c").read_text(encoding="utf-8")
DOC = (ROOT / "docs/references/CONFIGURATION.md").read_text(encoding="utf-8")
EXPECTED = bytes.fromhex("8B 44 24 04 5E 83 E0 04 59 C3")
RETAIL_HASH = "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"


def test_opt_in_only_after_verified_exe_and_ini():
    assert 'GetPrivateProfileIntA("Audio","ExperimentalMusicStutterFix",0,path)!=1' in CORE
    assert "ExperimentalMusicStutterFix=" not in GENERATED
    assert '"ExperimentalMusicStutterFix"' in VALID
    assert CORE.index("if (!supported_exe())") < CORE.index("install_experimental_music_stutter_fix();")
    assert CORE.index("SpiderCfg_ValidateCurrentIni(log_line);") < CORE.index(
        "install_experimental_music_stutter_fix();"
    )


def test_single_byte_patch_has_full_guard_and_rollback():
    src = CORE[CORE.index("static void install_experimental_music_stutter_fix(void)"):
               CORE.index("/* Exact-signature Win11 fclose(NULL) recovery")]
    signature = src.split("static const BYTE expected[10]={")[1].split("};")[0]
    octets = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", signature))
    assert octets == EXPECTED
    assert EXPECTED[0x10448F-0x104488] == 4
    for token in (
        "nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386",
        "nt->OptionalHeader.SizeOfImage<0x104488u+sizeof(expected)",
        "memcmp(base+0x104488u,expected,sizeof(expected))!=0",
        "target=base+0x10448Fu;",
        "VirtualProtect(target,1,PAGE_EXECUTE_READWRITE,&old)",
        "*target=0x01;",
        "FlushInstructionCache(GetCurrentProcess(),target,1)",
        "*target=0x04;",
        "VirtualProtect(target,1,old,&ignored)",
        "AUDIO-STUTTER: ON",
    ):
        assert token in src


def test_original_executable_if_provided():
    original_path = os.getenv("SPIDERMAN2K1FIX_TEST_EXE")
    if not original_path:
        return
    original = Path(original_path).read_bytes()
    assert hashlib.sha256(original).hexdigest() == RETAIL_HASH
    assert original[0x104488:0x104488+len(EXPECTED)] == EXPECTED


def test_technical_documentation_says_experimental():
    assert "ExperimentalMusicStutterFix=1" in DOC
    assert "OFF by default" in DOC
    assert "not yet been validated" in DOC
