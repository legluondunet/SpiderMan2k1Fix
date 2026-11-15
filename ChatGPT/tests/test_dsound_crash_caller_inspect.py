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


def test_focused_linux_mode_executes_on_synthetic_sha_checked_pe(
        tmp_path, monkeypatch):
    """Test the actual scanner, not just its source strings.

    Fake minimal PE32 contains only tiny known signatures. Tests replace
    the SHA *in the test module instance only*, never in production source.
    We stub disassembly to avoid relying on an external objdump executable.
    No game asset, original executable, or private dump is distributed.
    """
    import hashlib
    import importlib.util
    import struct

    spec = importlib.util.spec_from_file_location(
        "spidey_audio_inspector_test",
        ROOT / "ChatGPT/tools/inspect_audio_load_x86.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    blob = bytearray(module.EXPECTED_SIZE)
    blob[0:2] = b"MZ"
    struct.pack_into("<I", blob, 0x3C, 0x80)
    blob[0x80:0x84] = b"PE\x00\x00"
    coff = 0x84
    struct.pack_into("<HH", blob, coff, 0x014C, 1)
    struct.pack_into("<H", blob, coff + 16, 0xE0)
    opt = coff + 20
    struct.pack_into("<H", blob, opt, 0x10B)
    struct.pack_into("<I", blob, opt + 28, 0x00400000)
    section = opt + 0xE0
    blob[section:section + 8] = b".text\x00\x00\x00"
    struct.pack_into("<IIII", blob, section + 8,
                     0x120000, 0x1000, 0x120000, 0x400)

    for _, (va, signature) in module.KNOWN.items():
        if signature is not None:
            raw = 0x400 + (va - 0x00401000)
            blob[raw:raw + len(signature)] = signature

    exe = tmp_path / "SpideyPC.exe"
    exe.write_bytes(blob)
    before = exe.read_bytes()
    monkeypatch.setattr(module, "EXPECTED_SHA256",
                        hashlib.sha256(blob).hexdigest())
    calls = []

    def fake_disassemble(executable, start, end):
        assert executable == exe
        calls.append((start, end))
        return f"TEST DISASSEMBLY [{start:08x}..{end:08x}]"

    monkeypatch.setattr(module, "disassemble", fake_disassemble)
    report_path = tmp_path / "targeted-dsound.txt"
    report = module.inspect(bytes(blob), exe, report_path,
                            startup_crash_only=True)
    assert report_path.read_text(encoding="utf-8") == report
    assert report.count("TEST DISASSEMBLY") == 5
    assert "0x004FF9CA" in report
    assert "0x80004005" in report
    assert "NOT a validated unwind frame" in report
    assert "FOCUSED RESOURCE/DSOUND CALL" not in report
    assert calls == [
        (0x004FF700, 0x004FFB40),
        (0x004FDC80, 0x004FE000),
        (0x00503AE0, 0x00503B45),
        (0x00504050, 0x00504110),
        (0x005027F0, 0x00503300),
    ]
    assert exe.read_bytes() == before  # read-only, no game modification

    # Wrong original hash must abort before any additional report is written.
    monkeypatch.setattr(module, "EXPECTED_SHA256", "0" * 64)
    other_report = tmp_path / "should-not-exist.txt"
    import pytest
    with pytest.raises(ValueError, match="SHA-256"):
        module.inspect(bytes(blob), exe, other_report, startup_crash_only=True)
    assert not other_report.exists()

    # Explicit same-path and hard-link safety on Linux: refuse overwrite,
    # even with synthetic test bytes, and leave original file unchanged.
    monkeypatch.setattr(module, "EXPECTED_SHA256",
                        hashlib.sha256(blob).hexdigest())
    with pytest.raises(ValueError, match="refusing to overwrite"):
        module.inspect(bytes(blob), exe, exe, startup_crash_only=True)
    alias = tmp_path / "same-inode.exe"
    alias.hardlink_to(exe)
    with pytest.raises(ValueError, match="refusing to overwrite"):
        module.inspect(bytes(blob), exe, alias, startup_crash_only=True)
    assert exe.read_bytes() == before


def test_cli_exposes_focused_linux_mode_without_changing_default_probe():
    assert 'parser.add_argument("--startup-crash-only", action="store_true"' in PYTOOL
    assert "startup_crash_only=args.startup_crash_only" in PYTOOL
    assert "def inspect(blob: bytes, exe: Path, out: Path, startup_crash_only: bool = False)" in PYTOOL
    assert 'if startup_crash_only:' in PYTOOL
    assert "out.write_text(report,encoding=\"utf-8\")" in PYTOOL


def test_native_ddraw_setcooplevel_efail_is_proven_upstream_error():
    """Real WER EDI is the original DirectDraw7 vtable result, not audio.

    Verified against the user's exact original SHA256 on Linux. The
    method +0x50 is IDirectDraw7::SetCooperativeLevel. The native flag
    0x11 requests DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN; our existing
    hooked_coop uses DDSCL_NORMAL if it intercepted the call.
    """
    call_sequence = bytes.fromhex(
        "6A 11 56 50 FF 52 50 8B F8 81 FF 45 02 76 88"
        " 0F 84 B7 00 00 00 3B FB 0F 84 AF 00 00 00")
    assert call_sequence[:2] == bytes([0x6A, 0x11])
    assert call_sequence[4:7] == bytes([0xFF, 0x52, 0x50])
    assert call_sequence[7:9] == bytes([0x8B, 0xF8])  # mov edi,eax
    # Both acceptance conditions for success/error exceptions jump to
    # 0x4FFA4F; E_FAIL does not match either of them.
    first_je = 0x4FF992
    second_je = 0x4FF99A
    first_disp = int.from_bytes(call_sequence[17:21], "little", signed=True)
    second_disp = int.from_bytes(call_sequence[25:29], "little", signed=True)
    assert first_je + 6 + first_disp == 0x4FFA4F
    assert second_je + 6 + second_disp == 0x4FFA4F
    # Original error shutdown uses JGE to avoid cleanup on non-negative.
    # 0x80004005 is signed-negative: the JGE is not taken.
    jge = bytes.fromhex("0F 8D 94 00 00 00")
    assert 0x4FF9B5 + len(jge) + int.from_bytes(
        jge[2:], "little", signed=True) == 0x4FFA4F
    signed_failure = 0x80004005 - (1 << 32)
    assert signed_failure < 0
    assert 0x80004005 not in (0, 0x88760245)

    assert "0x004FF983" in PYTOOL
    assert "0x004FF98C" in PYTOOL
    assert "0x004FF9B3" in PYTOOL
    assert "0x004FF987" in PYTOOL
    assert "0x004FF983" in PSTOOL
    assert "0x004FF98C" in PSTOOL
    assert "0x004FF9B3" in PSTOOL
    assert "0x004FF987" in PSTOOL

    # No claim that the game's 0x11 request reached native DirectDraw:
    # when the patch hook is active, it must keep forwarding DDSCL_NORMAL.
    assert "original_coop(self,hwnd,DDSCL_NORMAL)" in MAIN
    assert "BORDERLESS: ERROR DDSCL_NORMAL rejected hr=" in MAIN
    assert "BORDERLESS: intercept SetCooperativeLevel" in MAIN
    coop = MAIN.split("static HRESULT WINAPI hooked_coop(", 1)[1].split(
        "static HRESULT WINAPI hooked_mode(", 1)[0]
    assert "hr=original_coop(self,hwnd,DDSCL_NORMAL);" in coop
    assert "if(SUCCEEDED(hr))" in coop
    assert "BORDERLESS: COOP-FAIL-CONTEXT" in coop
    assert "BORDERLESS: COOP-FAIL-BACKEND" in coop
    assert "VirtualQuery((LPCVOID)original_coop" in coop
    assert "GetModuleFileNameA((HMODULE)mem.AllocationBase" in coop
    assert "strrchr(module_path" in coop
    assert "GetWindowThreadProcessId(hwnd,&hwnd_process_id)" in coop
    assert "IsWindow(hwnd)" in coop
    assert "GetCurrentThreadId()" in coop
    assert "return hr;" in coop
    assert "original_coop(self,hwnd,DDSCL_EXCLUSIVE" not in coop
    assert "Sleep(" not in coop
    assert "MessageBox" not in coop

def test_directdraw_vtable_creation_distinguishes_actual_hook_from_prepared_table():
    """A prepared vtable is not proof the COM object was redirected.

    This is crucial for the original caller 0x004FF987: an unmodified
    original object can still request fullscreen/exclusive DDSCL 0x11.
    """
    create = MAIN.split("static HRESULT WINAPI hooked_create(", 1)[1].split(
        "static void install_ddraw_hook(", 1)[0]
    assert "BOOL coop_hook_present=FALSE;" in create
    assert "if(vtable==patched_ddraw_vtable" in create
    assert "coop_hook_present=TRUE;" in create
    assert "*object=patched_ddraw_vtable;" in create
    assert create.index("*object=patched_ddraw_vtable;") < create.index(
        'if(!coop_hook_present)')
    assert "DDRAW-STARTUP: WARNING native IDirectDraw7 object not redirected" in create
    assert "vtable[20]" in create
    assert "return hr;" in create
    # During this diagnostic phase there is no silent success substitution,
    # retry or destructive release on a COM mismatch.
    assert "hr=DD_OK;" not in create
    assert "IDirectDraw7_Release" not in create
    assert "Sleep(" not in create

def test_directdraw_coop_reports_hwnd_owner_thread_on_success_and_failure():
    coop = MAIN.split("static HRESULT WINAPI hooked_coop(", 1)[1].split(
        "static HRESULT WINAPI hooked_mode(", 1)[0]
    assert "BORDERLESS: COOP-SUCCESS-CONTEXT" in coop
    assert "BORDERLESS: COOP-FAIL-CONTEXT" in coop
    assert "same-thread=%d" in coop
    assert "owner_tid && owner_tid==caller_tid" in coop
    assert "hwnd_thread_id && hwnd_thread_id==GetCurrentThreadId()" in coop
    assert "GetWindowThreadProcessId(hwnd,&owner_pid)" in coop
    assert "GetWindowThreadProcessId(hwnd,&hwnd_process_id)" in coop
    assert "InterlockedCompareExchange(&success_context_logged,1,0)==0" in coop
    assert "hr=original_coop(self,hwnd,DDSCL_NORMAL);" in coop
    assert "return hr;" in coop
    assert "DDSCL_EXCLUSIVE" not in coop.split("if(SUCCEEDED(hr))", 1)[0]
