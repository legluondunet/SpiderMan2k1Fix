"""Level-dependent native ESC glyph trace is strictly read-only and bounded."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")

def test_native_pause_detail_trace_only_after_verified_banner():
    block=CORE.split("static void maybe_log_native_pause43_detail(",1)[1].split(
        "static void free_menu_vertices(",1)[0]
    for token in (
        "if(!debug_log_enabled", "native_pause43_visible_near_flip(flip)",
        "rva!=0x00102C72UL", "native_pause43_detail_samples",
        ">=80", "tw==8u && th==8u", "tw==32u",
        "IDirect3DDevice7_GetTexture(", "IDirectDrawSurface7_Release(tex)",
        "PAUSE-NATIVE-DETAIL:", "[READ-ONLY]", "log_line(line)"
    ):
        assert token in block
    for forbidden in ("SetTexture(", "SetRenderState(", "SendInput(",
                      "copy[i].x=", "original_draw_primitive("):
        assert forbidden not in block

def test_native_pause_trace_receives_original_vertices_after_guarded_corrections():
    draw=CORE.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert draw.index("maybe_correct_native_pause43(")<draw.index(
        "maybe_log_native_pause43_detail(")<draw.index(
        "maybe_correct_horplus(")
    assert "maybe_log_native_pause43_detail(self,type,fvf,verts,count," in draw
    assert "__builtin_return_address(0),v!=verts" in draw
    assert "original_draw_primitive(self,type,fvf,(LPVOID)v,count,flags)" in draw
