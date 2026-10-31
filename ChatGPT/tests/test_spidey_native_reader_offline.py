"""Read-only regression tests for the offline native reader inspector.

No original or synthetic executable is distributed in CI.
"""
import importlib.util
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "ChatGPT" / "tools" / "inspect_spidey_native_reader.py"


def load_tool():
    spec = importlib.util.spec_from_file_location("offline_native_reader", TOOL)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def test_reject_unknown_content_and_malformed_binary():
    mod = load_tool()
    with pytest.raises(mod.InvalidExecutable):
        mod.parse_pe(b"not a Windows executable")
    with pytest.raises(mod.InvalidExecutable, match="exact SHA-256"):
        mod.inspect(b"not a game binary")


def test_file_backed_virtual_address_mapping_without_executable_fixture():
    mod = load_tool()
    data = bytes(range(256)) * 4
    pe = mod.PE(data=data, base=0x400000, headers=0x80,
                sections=(mod.Section(".text", 0x1000, 0x100, 0x180),))
    assert pe.va_offset(0x401000) == 0x100
    assert pe.read(0x401012, 4) == data[0x112:0x116]
    with pytest.raises(mod.InvalidExecutable):
        pe.read(0x400FFF, 4)
    with pytest.raises(mod.InvalidExecutable):
        pe.read(0x401180, 1)


def test_direct_relative_call_search_is_bounded_to_text_only():
    mod=load_tool()
    raw=bytearray(0x400)
    # .text base=0x401000 and file offset 0x100.
    target=0x402000
    call_site=0x401020
    relative=target-(call_site+5)
    import struct
    raw[0x120]=0xE8
    struct.pack_into("<i",raw,0x121,relative)
    # Identical raw bytes in a non-.text section MUST be ignored.
    raw[0x220:0x225]=raw[0x120:0x125]
    pe=mod.PE(
        data=bytes(raw),base=0x400000,headers=0x100,
        sections=(mod.Section(".text",0x1000,0x100,0x100),
                  mod.Section(".data",0x2000,0x100,0x200)))
    assert mod.direct_relative_call_candidates(pe,target)==["0x00401020"]
    assert mod.direct_relative_call_candidates(pe,target+1)==[]
    assert mod.direct_relative_call_candidates(pe,0x505720)==[]


def test_pattern_match_is_explicitly_unverified():
    mod = load_tool()
    samples = bytes([255, 80, 36, 255, 81, 40])
    matches = mod.indirect_call_patterns(samples, 0x501E6D)
    assert [entry["possible_method"] for entry in matches] == [
        "GetDeviceState", "GetDeviceData"
    ]
    assert all(entry["status"] == "unverified_byte_pattern_not_disassembly"
               for entry in matches)


def test_cli_failure_does_not_modify_file(tmp_path):
    source = tmp_path / "sample.bin"
    original = b"invalid input data"
    source.write_bytes(original)
    result = subprocess.run([sys.executable, str(TOOL), str(source)],
                            capture_output=True, text=True)
    assert result.returncode == 2
    assert source.read_bytes() == original
    assert not list(tmp_path.glob("*.json"))


def test_no_runtime_mutation_or_external_modules():
    script = TOOL.read_text(encoding="utf-8")
    assert "EXPECTED_SHA256" in script
    assert "unsupported EXE: exact SHA-256" in script
    mod = load_tool()
    assert len(mod.KNOWN_NATIVE_SIGNATURES) == 10
    assert mod.KNOWN_NATIVE_SIGNATURES[0] == (
        0x00501E79, "ff 51 64", "Poll indirect COM call")
    assert mod.KNOWN_NATIVE_SIGNATURES[3] == (
        0x00501E9F, "8b 10 ff 52 1c",
        "Acquire indirect COM call sequence")
    assert "source-confirmed native reader signature differs" in script
    assert len(mod.BINARY_VERIFIED_INPUT_SITES) == 10
    assert mod.BINARY_VERIFIED_INPUT_SITES[0][:2] == (
        0x00501F0E, "ff 52 28")
    assert mod.BINARY_VERIFIED_INPUT_SITES[7][:2] == (
        0x0050A2AD, "e8 9e 7b ff ff")
    assert mod.BINARY_VERIFIED_INPUT_SITES[8][:2] == (
        0x00505738, "e8 53 4a 00 00")
    assert "disassembled joystick dataflow site mismatch" in script
    assert '"pov_event_offset": 32' in script
    assert '"axis_lx_event_offset": 0' in script
    assert '"axis_ly_event_offset": 4' in script
    assert '"event_record_size": 20' in script
    assert '"event_queue_limit": 16' in script
    assert len(mod.BINARY_VERIFIED_MASK_CONSUMER_SITES) == 17
    assert mod.BINARY_VERIFIED_MASK_CONSUMER_SITES[4][:2] == (
        0x00505738, "e8 53 4a 00 00")
    assert mod.BINARY_VERIFIED_MASK_CONSUMER_SITES[5][:2] == (
        0x00505759, "8b 54 24 08")
    assert mod.BINARY_VERIFIED_MASK_CONSUMER_SITES[8][:2] == (
        0x00479540, "8b 44 24 04")
    assert mod.BINARY_VERIFIED_MASK_CONSUMER_SITES[12][:2] == (
        0x00479560, "c6 40 01 01")
    assert '"first_consumed_output": "held only in this consumer"' in script
    assert '"number_of_direct_latch_calls": 20' in script
    assert "disassembled mask consumer mismatch" in script
    assert "direct_relative_call_candidates(pe,0x0050A190)" in script
    assert "direct_relative_call_candidates(pe,0x00505720)" in script
    assert "direct_relative_call_candidates(pe,0x00479540)" in script
    assert "direct_relative_call_candidates(pe,0x004796A0)" in script
    for forbidden in ("ctypes", "subprocess", "urllib", "WriteProcessMemory"):
        assert forbidden not in script
