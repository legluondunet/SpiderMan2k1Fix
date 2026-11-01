"""Static, SHA-gated input xrefs: no original executable in CI."""
import importlib.util
import subprocess
import sys
from pathlib import Path

import pytest

ROOT=Path(__file__).resolve().parents[2]
TOOLS=ROOT/"ChatGPT"/"tools"


def load_tool():
    sys.path.insert(0,str(TOOLS))
    spec=importlib.util.spec_from_file_location(
        "spidey_callsites_research",TOOLS/"inspect_spidey_input_callsites.py")
    mod=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=mod
    spec.loader.exec_module(mod)
    return mod


def test_full_retail_direct_call_inventory_is_exact_and_increasing():
    mod=load_tool()
    assert len(mod.EXPECTED_SHARED_SERVICE_CALLS)==56
    assert len(set(mod.EXPECTED_SHARED_SERVICE_CALLS))==56
    assert mod.EXPECTED_SHARED_SERVICE_CALLS==tuple(
        sorted(mod.EXPECTED_SHARED_SERVICE_CALLS))
    assert mod.EXPECTED_SHARED_SERVICE_CALLS[0]==0x00427989
    assert mod.EXPECTED_SHARED_SERVICE_CALLS[-1]==0x0050DC72
    assert len(mod.EXPECTED_STATE_UPDATE_CALLS)==20
    assert all(0x00505720<=x<0x00505902
               for x in mod.EXPECTED_STATE_UPDATE_CALLS)
    assert len(mod.VERIFIED_SCRIPT_INPUT_CHAIN)==5
    assert len(mod.SCRIPT_DISPATCH_SHARED_SERVICE_CALLS)==10
    assert mod.EXPECTED_COUNTS=={
        0x0050A190:1, 0x00505720:56,
        0x00479540:20, 0x004796A0:1
    }


def test_source_known_xref_aggregation_without_binary_redistribution(monkeypatch):
    mod=load_tool()
    class FakePE:
        def read(self,va,n):
            for address, signature, _ in mod.VERIFIED_SCRIPT_INPUT_CHAIN:
                if va==address:
                    assert n==len(bytes.fromhex(signature))
                    return bytes.fromhex(signature)
            assert n==21
            return b"\\x00"*n
    data=b"unit fixture, never original retail executable"
    monkeypatch.setattr(mod,"inspect",lambda x:{
        "sha256":"fixture", "bytes":len(x)})
    monkeypatch.setattr(mod,"parse_pe",lambda x: FakePE())
    calls={
        0x0050A190:(0x00505738,),
        0x00505720:mod.EXPECTED_SHARED_SERVICE_CALLS,
        0x00479540:mod.EXPECTED_STATE_UPDATE_CALLS,
        0x004796A0:(0x005058F5,),
    }
    monkeypatch.setattr(mod,"direct_relative_call_candidates",
        lambda _pe,target:["0x%08X"%a for a in calls[target]])
    result=mod.audit(data)
    assert result["identity"]["sha256"]=="fixture"
    assert result["direct_calls"]["0x00505720"]["count"]==56
    assert result["direct_calls"]["0x00479540"]["count"]==20
    assert result["direct_calls"]["0x0050A190"]["count"]==1
    evidence=result["script_dispatch_structural_evidence"]
    assert evidence["number_of_verified_calls"]==10
    assert evidence["other_calls_not_yet_classified"]==46
    assert evidence["game_mode"].startswith("UNKNOWN")
    assert evidence["callsites"][0]=="0x00427989"
    assert evidence["callsites"][-1]=="0x00427E9F"
    assert result["static_invariants"]["hook_authorized"] is False
    assert result["static_invariants"]["bridge_enabled"] is False
    assert result["static_invariants"]["service_called_from_many_code_regions"]
    assert len(result["shared_service_callsite_context_sha256_16"])==56
    assert all(len(k)==16 for k in
        result["shared_service_callsite_context_sha256_16"].values())
    calls[0x00505720]=mod.EXPECTED_SHARED_SERVICE_CALLS[:-1]
    with pytest.raises(mod.InvalidExecutable,
                       match="input xref count mismatch"):
        mod.audit(data)
    calls[0x00505720]=(0x0042798A,)+mod.EXPECTED_SHARED_SERVICE_CALLS[1:]
    with pytest.raises(mod.InvalidExecutable,
                       match="56 verified shared service call"):
        mod.audit(data)
    calls[0x00505720]=mod.EXPECTED_SHARED_SERVICE_CALLS
    calls[0x00479540]=(0x00505786,)+mod.EXPECTED_STATE_UPDATE_CALLS[1:]
    with pytest.raises(mod.InvalidExecutable,
                       match="20 verified action state"):
        mod.audit(data)


def test_cli_rejects_unrecognized_input_without_file_write(tmp_path):
    tool=TOOLS/"inspect_spidey_input_callsites.py"
    file=tmp_path/"Spider-Man.exe"
    file.write_bytes(b"not a retail executable")
    proc=subprocess.run([sys.executable,str(tool),str(file)],
        capture_output=True,text=True)
    assert proc.returncode==2
    assert "unsupported size" in proc.stderr
    assert file.read_bytes()==b"not a retail executable"
    assert set(tmp_path.iterdir())=={file}


def test_read_only_no_runtime_dependency():
    tool=(TOOLS/"inspect_spidey_input_callsites.py").read_text(
        encoding="utf-8")
    assert "inspect(data)" in tool
    assert "direct_relative_call_candidates(pe, target)" in tool
    for banned in ("WriteProcessMemory","VirtualProtect","SendInput(",
                   "ctypes","win32api","os.system"):
        assert banned not in tool
    for runtime in ("spiderman2k1fix.c","sdl3_gamepad_probe.c",
                    "sdl3_gameplay_live.c"):
        src=(ROOT/runtime).read_text(encoding="utf-8")
        assert "inspect_spidey_input_callsites" not in src
