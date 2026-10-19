"""Native Windows in-place COM vtable diagnostic regression."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/"spiderman2k1fix.c").read_text()
DOC=(ROOT/"WINDOWS_VIEWPORT_DIAGNOSTIC.md").read_text()

def test_inplace_opt_in_environment():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_D3D_INPLACE"' in SRC
    assert "length==1 && inplace[0]=='1'" in SRC
    assert "D3D7-DIAG: INPLACE native device vtable patched" in SRC

def test_inplace_keeps_original_vtable_pointer():
    assert "VirtualProtect(vt,sizeof(patched_device_vtable)" in SRC
    assert "vt[13]=(void*)hooked_set_viewport;" in SRC
    assert "vt[20]=(void*)hooked_set_render_state;" in SRC
    assert "vt[25]=(void*)hooked_draw_primitive;" in SRC
    assert "vt[26]=(void*)hooked_draw_indexed_primitive;" in SRC
    assert "Never fall back to cloned vtable in opt-in mode." in SRC

def test_native_bypass_precedes_inplace_and_no_default_behavior_changes():
    bypass=SRC.index('GetEnvironmentVariableA("SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE"')
    inplace=SRC.index('GetEnvironmentVariableA("SPIDERMAN2K1FIX_D3D_INPLACE"')
    assert bypass < inplace
    assert "if(device_ready==2 && vt[13]==(void*)original_set_viewport)" in SRC
    assert "$env:SPIDERMAN2K1FIX_D3D_INPLACE='1'" in DOC
    assert "ExperimentalAltTabReacquire=0" in DOC
