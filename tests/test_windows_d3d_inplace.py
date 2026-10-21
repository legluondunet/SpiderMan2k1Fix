"""Default in-place Direct3D7 hooks preserve the original COM vtable."""

from pathlib import Path

SRC=(Path(__file__).resolve().parents[1]/"spiderman2k1fix.c").read_text()


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


def test_optional_esc_draw_trace_is_read_only_and_never_alters_rendering():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_ESC_DRAW_TRACE"' in SRC
    assert "debug_log_enabled && n==1 && value[0]=='1'" in SRC
    assert "GetAsyncKeyState(VK_F12)" in SRC
    assert "esc_probe_frames=6;" in SRC
    assert "#define ESC_PROBE_RING 40" in SRC
    assert "IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,&tex)" in SRC
    assert "IDirectDrawSurface7_Release(tex)" in SRC
    assert "esc_probe_on_flip(count);" in SRC
    assert "esc_probe_record(self,0,type,fvf,verts,count,0,__builtin_return_address(0));" in SRC
    assert "esc_probe_record(self,1,type,fvf,verts,count,icount,__builtin_return_address(0));" in SRC
    assert "return original_draw_primitive(self,type,fvf,verts,count,flags);" not in SRC
    assert "vt[25]=(void*)hooked_draw_primitive;" in SRC


def test_esc_probe_keeps_early_large_ui_shapes_without_altering_draw_calls():
    # The initial diagnostic sampled only the last 40 calls, which captured
    # native PAUSED/menu letters but missed the earlier full-width violet
    # panel and translucent orange background.
    assert "#define ESC_PROBE_TOP_QUADS 48" in SRC
    assert "#define ESC_PROBE_TOP_TRIS 32" in SRC
    assert "esc_probe_rank_large(item);" in SRC
    assert "esc_probe_dump_candidates(\"quad\",esc_probe_top_quads,esc_probe_quad_count)" in SRC
    assert "esc_probe_dump_candidates(\"tri\",esc_probe_top_tris,esc_probe_tri_count)" in SRC
    assert "ESC-PROBE-LARGE: frame-summary" in SRC
    assert "d->texture_id" in SRC
    assert "d->diffuse" in SRC
    assert "score=(double)w*(double)h;" in SRC
    assert "if(draw->vertex_count==4)" in SRC
    assert "esc_probe_quad_count=0;esc_probe_tri_count=0;" in SRC
