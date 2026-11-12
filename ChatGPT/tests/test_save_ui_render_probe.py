"""Save Game renderer provenance must be optional, bounded and read-only.

Its native shell may report 3D even while 2D menu items are visible.
Until draw identity is proven from a correlated F9 trace, no new geometry
transform or menu ownership inference may ship from this diagnostic PR.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SOURCE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")


def test_save_screen_probe_requires_debug_opt_in_and_explicit_f9():
    fn=SOURCE.split("static int save_ui_trace_active(void)",1)[1].split(
        "static void save_ui_trace_draw(",1)[0]
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_SAVE_UI_TRACE"' in SOURCE
    assert "if(debug_log_enabled)" in SOURCE
    assert "InterlockedCompareExchange(&save_ui_trace_enabled,0,0)!=1" in fn
    assert "(GetAsyncKeyState(VK_F9)&0x8000)!=0" in fn
    assert "InterlockedExchange(&save_ui_trace_until_flip,flip+8)" in fn
    assert "InterlockedExchange(&save_ui_trace_dp_ui,0)" in fn
    assert "InterlockedExchange(&save_ui_trace_dip_ui,0)" in fn
    assert "InterlockedExchange(&save_ui_trace_other,0)" in fn
    assert "menu_upscale_active" in fn
    assert "gameplay_4x3_active" in fn
    assert "fov_game_height" in fn


def test_save_screen_probe_is_bounded_and_draw_path_unchanged():
    fn=SOURCE.split("static void save_ui_trace_draw(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_primitive(",1)[0]
    assert 'SAVE-UI-TRACE: OTHER' in fn
    assert 'SAVE-UI-TRACE: %s' in fn
    assert "if(n<=24)" in fn
    assert "v && fvf==0x144UL && count>=4u" in fn
    assert "[UNKNOWN FVF; NO VERTEX DEREFERENCE]" in fn
    assert "[UNCLASSIFIED; READ-ONLY]" in fn
    assert "if(n>100)return;" in fn
    assert "IDirect3DDevice7_GetViewport(" in fn
    assert "geom_rva(caller,&rva)" in fn
    assert "original_draw_primitive(" not in fn
    assert "original_draw_indexed_primitive(" not in fn
    assert "SetRenderState(" not in fn
    assert "DrawPrimitive(" not in fn
    assert "VirtualProtect(" not in fn
    assert "memcpy(" not in fn


def test_both_original_draw_paths_remain_unmodified_by_probe():
    dp=SOURCE.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    dip=SOURCE.split("static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[1].split(
        "/* Diagnostic: native SetViewport",1)[0]
    assert 'save_ui_trace_draw("DP"' in dp
    assert 'save_ui_trace_draw("IP"' in dip
    assert dp.index('save_ui_trace_draw("DP"')<dp.index(
        "original_draw_primitive(self,type,fvf,(LPVOID)v,count,flags)")
    assert dip.index('save_ui_trace_draw("IP"')<dip.index(
        "original_draw_indexed_primitive(self,type,fvf,(LPVOID)v,count,indices,icount,flags)")
    assert "const void *v=scale_menu_vertices(" in dip
    assert "const void *v=scale_menu_vertices(" in dp
    # No generic forced 4:3 mapping of previously unclassified 3D/quads.
    assert "SpiderUICanvas_43Transform(" not in dip
