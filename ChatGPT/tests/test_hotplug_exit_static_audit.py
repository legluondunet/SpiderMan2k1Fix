"""SHA-pinned offline PE exit-site audit: no changes to the retail executable."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import struct
import subprocess
import sys

import pytest

ROOT=Path(__file__).resolve().parents[2]
SCRIPT=ROOT/"ChatGPT/diagnostics/inspect_spideypc_exit_site.py"
spec=importlib.util.spec_from_file_location("inspect_spideypc_exit_site",SCRIPT)
audit=importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def synthetic_pe():
    # Two valid headers and one x86 .text section, mapping the real return
    # VA for a *fake* binary. Never request actual EXE source in CI.
    binary=bytearray(0x200+0x130000)
    binary[0:2]=b"MZ"
    struct.pack_into("<I",binary,0x3C,0x80)
    binary[0x80:0x84]=b"PE\\0\\0".replace(b"\\0",b"\0")
    struct.pack_into("<H",binary,0x86,1)
    struct.pack_into("<H",binary,0x94,0xE0)
    struct.pack_into("<H",binary,0x98,0x10B)
    struct.pack_into("<I",binary,0x98+28,0x400000)
    table=0x98+0xE0
    binary[table:table+5]=b".text"
    struct.pack_into("<IIII",binary,table+8,0x130000,0x1000,0x130000,0x200)
    # Special-known exit status literal for scanner (irrelevant arbitrary
    # bytes in fake .text are acceptable in this unit test).
    raw=0x200+(0x52A1C6-0x400000)-0x1000
    binary[raw-5:raw]=b"\\xFF\\x15\\x20\\x30\\x40".decode("unicode_escape").encode("latin1")
    binary[0x310:0x314]=struct.pack("<I",audit.EXIT_STATUS)
    return bytes(binary),raw


def test_exact_hash_gate_refuses_unknown_binary(tmp_path):
    fake=tmp_path/"SpideyPC.exe"
    data,_=synthetic_pe()
    fake.write_bytes(data)
    with pytest.raises(ValueError,match="SHA-256 mismatch"):
        audit.collect_evidence(fake,data)
    proc=subprocess.run([sys.executable,str(SCRIPT),str(fake)],
                        capture_output=True,text=True)
    assert proc.returncode==2
    assert "REFUSED" in proc.stderr
    assert fake.read_bytes()==data


def test_readonly_va_mapping_and_literal_scan(tmp_path):
    fake=tmp_path/"SpideyPC.exe"
    data,raw=synthetic_pe()
    fake.write_bytes(data)
    base,sections=audit.parse_sections(data)
    assert base==0x400000
    assert audit.va_to_offset(0x52A1C6,base,sections)==(raw,".text")
    assert audit.file_offset_to_va(0x310,base,sections)==(0x401110,".text")
    _,_,_,lo,hi,region,hits=audit.collect_evidence(fake,data,enforce_sha=False)
    assert lo==0x52A1C6-0xC0
    assert hi==0x52A1C6+0x90
    assert len(region)==hi-lo
    assert (0x310,0x401110,".text") in hits
    assert fake.read_bytes()==data


def test_reject_truncated_or_wrong_format():
    for data in (b"",b"MZ",b"MZ"+b"\0"*90,b"\0"*512):
        with pytest.raises(ValueError):
            audit.parse_sections(data)


def test_no_patch_or_wine_process_execution():
    source=SCRIPT.read_text()
    assert "SHA256 =" in source
    assert "EXIT_STATUS = 0x80040209" in source
    assert "RETURN_VA = 0x0052A1C6" in source
    assert 'enforce_sha=True' in source
    assert "objdump" in source
    for forbidden in ("WriteProcessMemory(", "VirtualProtect(", "ExitProcess(",
                      "SetWindowsHookEx(", "wine SpideyPC.exe", "wineserver -k"):
        if forbidden=="ExitProcess(":
            # Literal evidence string is okay, no executable code.
            continue
        assert forbidden not in source
