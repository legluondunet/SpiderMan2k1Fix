"""Default in-place Direct3D7 hooks preserve the original COM vtable."""

from pathlib import Path

SRC=(Path(__file__).resolve().parents[2]/"spiderman2k1fix.c").read_text()


def test_inplace_is_default_without_power_shell_switch():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_D3D_INPLACE"' not in SRC
    assert "D3D7: native device vtable patched in place (default; all four hooks ON)" in SRC
    assert "if(device_ready==2 && vt[13]==(void*)original_set_viewport" in SRC


def test_inplace_keeps_original_vtable_pointer():
    assert "VirtualProtect(vt,sizeof(patched_device_vtable)" in SRC
    assert "vt[13]=(void*)hooked_set_viewport;" in SRC
    assert "vt[20]=(void*)hooked_set_render_state;" in SRC
    assert "vt[25]=(void*)hooked_draw_primitive;" in SRC
    assert "vt[26]=(void*)hooked_draw_indexed_primitive;" in SRC
    assert "*obj=patched_device_vtable;" not in SRC
    assert "Do NOT fall" in SRC
    assert "vt[13]==(void*)hooked_set_viewport" in SRC  # shared vtable already patched


def test_explicit_native_diagnostic_bypass_precedes_default_patch():
    bypass=SRC.index('GetEnvironmentVariableA("SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE"')
    inplace=SRC.index("if(device_ready==2 && vt[13]==(void*)original_set_viewport")
    assert bypass<inplace
    assert "D3D7-DIAG: native device vtable" in SRC
