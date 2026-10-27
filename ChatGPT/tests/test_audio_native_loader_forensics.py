"""Safe offline native-audio loader reconnaissance: no game execution or writes."""
import hashlib
import importlib.util
import struct
from pathlib import Path

MODULE = Path(__file__).resolve().parents[1] / "tools" / "inspect_audio_load_x86.py"
spec = importlib.util.spec_from_file_location("audio_root_probe", MODULE)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


def fake_pe():
    """Tiny legal PE32 with one .text section and a verifiable raw rel32 call."""
    data = bytearray(0x600)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", data, 0x84, 0x14C)  # Machine
    struct.pack_into("<H", data, 0x86, 1)  # sections
    struct.pack_into("<H", data, 0x94, 0xE0)  # optional header size
    struct.pack_into("<H", data, 0x98, 0x10B)  # PE32
    struct.pack_into("<I", data, 0x98 + 28, 0x400000)  # image base
    hdr = 0x98 + 0xE0
    data[hdr:hdr + 5] = b".text"
    struct.pack_into("<IIII", data, hdr + 8, 0x200, 0x1000, 0x200, 0x200)
    src = 0x401010
    target = 0x401040
    data[0x200 + 0x10] = 0xE8
    struct.pack_into("<i", data, 0x200 + 0x11, target - src - 5)
    data[0x200 + 0x20:0x200 + 0x23] = b"ABC"
    data[0x200 + 0x30:0x200 + 0x34] = bytes.fromhex("00 08 00 00")
    return bytes(data)


def test_pure_readonly_pe_parser_and_displacement_report():
    buf = fake_pe()
    exe = probe.PE32(buf)
    assert exe.image_base == 0x400000
    assert exe.read(0x401020, 3) == b"ABC"
    assert exe.text()[0] == 0x401000
    assert (0x401010, 0x401040) in probe.candidate_rel32_calls(exe, 0x401040, 0x401050)
    assert "0x00401030" in " ".join(probe.probe_constants(exe, 0x401000, 0x401100))
    try:
        exe.read(0x402000, 2)
    except ValueError as exc:
        assert "unmapped raw VA" in str(exc)
    else:
        raise AssertionError("unmapped address should be refused")


def test_exact_sha_and_signature_gates_fail_closed(tmp_path, monkeypatch):
    buf = fake_pe()
    exe = tmp_path / "original.exe"
    exe.write_bytes(buf)
    report = tmp_path / "report.txt"
    try:
        probe.inspect(buf, exe, report)
    except ValueError as exc:
        assert "unsupported SpideyPC.exe length" in str(exc)
    else:
        raise AssertionError("unsupported original should be refused")
    assert not report.exists()
    monkeypatch.setattr(probe, "EXPECTED_SIZE", len(buf))
    monkeypatch.setattr(probe, "EXPECTED_SHA256", hashlib.sha256(buf).hexdigest())
    monkeypatch.setattr(probe, "KNOWN", {"verified prologue": (0x401020, b"ABC")})
    monkeypatch.setattr(probe, "WINDOW_START", 0x401000)
    monkeypatch.setattr(probe, "WINDOW_END", 0x401100)
    text = probe.inspect(buf, exe, report)
    assert "STATIC / READ-ONLY" in text
    assert "verified prologue" in text
    assert "caller=0x00401010 target=0x00401040" in text
    assert "0x00401040" in text
    assert "0x00401030" in text
    assert report.read_text() == text
    monkeypatch.setattr(probe, "KNOWN", {"bad prologue": (0x401020, b"XYZ")})
    try:
        probe.inspect(buf, exe, tmp_path / "should-not-exist.txt")
    except ValueError as exc:
        assert "native signature mismatch" in str(exc)
    else:
        raise AssertionError("original native code signature must be checked")


def test_source_uses_no_game_writes_runtime_patches_or_shell():
    content = MODULE.read_text()
    assert "EXPECTED_SHA256" in content
    assert "EXPECTED_SIZE" in content
    assert "0x00504110" in content
    assert "0x00472870" in content
    for forbidden in ("VirtualProtect", "WriteProcessMemory", "ptrace", "wine ", "shell=True"):
        assert forbidden not in content
    assert "read_bytes()" in content
    assert "out.write_text(report" in content


def test_windows_powershell_probe_matches_native_analysis_and_is_readonly():
    """PowerShell 5.1 implementation is self-contained; runtime needs Windows."""
    ps = MODULE.with_name("Inspect-SpiderManAudioLoader.ps1")
    body = ps.read_text(encoding="utf-8")
    assert "[CmdletBinding()]" in body
    assert "Get-FileHash -LiteralPath $resolvedExe -Algorithm SHA256" in body
    assert probe.EXPECTED_SHA256 in body
    assert "1507328" in body
    for addr in ("0x005039F0", "0x00504110", "0x00504230", "0x005042A0",
                 "0x00472870", "0x005039D0", "0x00504340"):
        assert addr in body
    for needle in ("Read-U16", "Read-U32", "Get-BytesAtVa",
                   "0xE8", "ToInt32", "objdump.exe", "llvm-objdump.exe",
                   "-ObjdumpPath", "0x800 (2048 bytes)", "Exact"):
        assert needle.lower() in body.lower()
    assert "New-Object byte[]" in body
    assert "Get-BytesAtVa -Address $windowStart" in body
    assert "STATIC / READ-ONLY" in body
    assert "no instruction signature asserted" in body
    assert "Refusing to overwrite the input executable" in body
    # Only the report is written, after PE and all original x86 signatures.
    for forbidden in ("Set-Content -Path $resolvedExe", "WriteAllBytes($resolvedExe",
                      "VirtualProtect", "WriteProcessMemory", "Start-Process SpideyPC",
                      "Start-Process wine", "Invoke-Expression", "IEX ", "python.exe"):
        assert forbidden not in body
    assert body.index("Get-FileHash -LiteralPath $resolvedExe") < body.index("WriteAllText($resolvedOutput")
