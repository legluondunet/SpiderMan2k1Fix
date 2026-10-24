"""Native D3D7 device-vtable bypass diagnostics remain opt-in."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SOURCE=(ROOT/"spiderman2k1fix.c").read_text()
DOC=(ROOT/"docs/diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md").read_text()

def test_native_d3d7_device_mode_is_opt_in():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE"' in SOURCE
    assert "n==1 && value[0]=='1'" in SOURCE
    assert "native device vtable, viewport/draw/render-state hooks bypassed" in SOURCE
    assert "patched_device_vtable[13]=(void*)hooked_set_viewport;" in SOURCE
    assert "patched_device_vtable[25]=(void*)hooked_draw_primitive;" in SOURCE
    assert "patched_device_vtable[26]=(void*)hooked_draw_indexed_primitive;" in SOURCE

def test_bypass_is_reversible_from_powershell():
    assert "$env:SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE='1'" in DOC
    assert "Remove-Item Env:" in DOC
