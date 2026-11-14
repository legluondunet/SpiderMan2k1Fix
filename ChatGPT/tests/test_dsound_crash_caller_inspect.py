"""Evidence provenance and read-only original-game startup failure triage.

The original game must never be bundled with tests or the PR. Our targeted
disassembler is usable with only the user's SHA-verified retail executable.
The direct callsite was established in independent 2026-11-14 WER stack
contexts and the older SHA-bound source call census.
"""
from pathlib import Path
import ast
import re

ROOT = Path(__file__).resolve().parents[2]
PYTOOL = (ROOT / "ChatGPT/tools/inspect_audio_load_x86.py").read_text(
    encoding="utf-8")
PSTOOL = (ROOT / "ChatGPT/tools/Inspect-SpiderManAudioLoader.ps1").read_text(
    encoding="utf-8")
MAIN = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
PROXY = (ROOT / "winmm_proxy.c").read_text(encoding="utf-8")


def test_original_crash_callsite_rel32_is_exact():
    call = bytes.fromhex("E8 21 41 00 00")
    caller = 0x004FF9CA
    displacement = int.from_bytes(call[1:], "little", signed=True)
    assert caller + len(call) + displacement == 0x00503AF0
    assert "0x004FF9CA" in PYTOOL
    assert "0x004FF9CA" in PSTOOL
    assert "0x00503AF7" in PYTOOL
    assert "0x00503AF7" in PSTOOL
    assert "0x00504050" in PYTOOL
    assert "0x00504050" in PSTOOL


def test_python_windows_inspectors_shares_bounded_crash_windows():
    for text in (PYTOOL, PSTOOL):
        for start, end in (
            ("0x004FF700", "0x004FFB40"),
            ("0x004FDC80", "0x004FE000"),
        ):
            assert start in text
            assert end in text
            assert int(end, 16) - int(start, 16) <= 0x500
        assert "0x80004005" in text
        assert ("NOT an unwound frame" in text or
                "NOT an unwound frame" in text)
    assert "EXPECTED_SHA256" in PYTOOL
    assert "$expectedSha256" in PSTOOL
    assert "hashlib.sha256" in PYTOOL
    assert "Get-FileHash" in PSTOOL
    assert "unsupported SpideyPC.exe SHA-256" in PYTOOL
    assert "Unsupported SpideyPC.exe SHA-256" in PSTOOL


def test_game_inspectors_do_not_change_executable_or_suppress_error():
    for text in (PYTOOL, PSTOOL):
        assert "original native" in text
    assert "subprocess.run(cmd, capture_output=True" in PYTOOL
    assert "out.write_text(report" in PYTOOL
    assert "ReadAllBytes($resolvedExe)" in PSTOOL
    assert "WriteAllText($resolvedOutput" in PSTOOL
    assert "Refusing to overwrite the input executable" in PSTOOL
    assert "No game file, process or archive was modified." in PSTOOL
    assert "0x004FF9CA" in PSTOOL


def test_native_ddraw_failure_reports_original_hresult_without_overriding_it():
    section = MAIN.split("static HRESULT WINAPI hooked_create(", 1)[1].split(
        "static void install_ddraw_hook(", 1)[0]
    assert "HRESULT hr=original_create(guid,out,iid,outer);" in section
    assert "DDRAW-STARTUP: DirectDrawCreateEx result=0x%08lX" in section
    assert "log_line(reason);" in section
    assert "return hr;" in section
    assert "return E_FAIL;" not in section
    assert "DirectDrawCreateEx result=0x%08lX" in section.split(
        "} else {", 1)[1]
    assert "DDRAW-STARTUP: ERROR main DLL missing" in PROXY
    assert "DDRAW-STARTUP: ERROR patch initialization not ready" in MAIN


def test_source_is_syntactically_valid_python():
    ast.parse(PYTOOL)
    ast.parse((ROOT / "ChatGPT/tests/test_dsound_primary_null_cleanup.py").read_text())
