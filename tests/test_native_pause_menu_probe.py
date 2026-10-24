"""Read-only instrumentation for original Spider-Man gameplay ESC pause menu.

The native UI can only be transformed safely once exact callsite/texture and
depth/RHW signatures are established under Wine. This stage must not change
rendering and must remain disabled without an explicit diagnostic opt-in.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
DOC = (ROOT / "docs" / "NATIVE_PAUSE_MENU_43.md").read_text(encoding="utf-8")
INI = (ROOT / "SpiderMan2k1Fix.ini").read_text(encoding="utf-8")


def trace_block():
    return CORE.split("/* Diagnostic only: classify original in-game ESC pause menu geometry", 1)[1].split(
        "static void free_menu_vertices(", 1)[0]


def test_probe_requires_debug_logging_and_environment_opt_in():
    probe = trace_block()
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_PAUSE_MENU_TRACE"' in probe
    assert "if(!debug_log_enabled)return;" in probe
    assert 'value[0]==\'1\'' in probe
    assert "pause_menu_trace_enabled=0;" in probe
    assert "SPIDERMAN2K1FIX_PAUSE_MENU_TRACE" not in INI
    assert "PAUSE-NATIVE-PROBE: READ-ONLY enabled" in probe


def test_native_gameplay_pause_identity_is_not_focus_pause():
    probe = trace_block()
    assert "0x00682950u" in probe
    assert "*SPIDER_NATIVE_ESC_PAUSE_CONTEXT==0" in probe
    assert "menu_upscale_active" in probe
    assert "pause_on_focus_loss" not in probe
    assert "pause_in_progress" not in probe
    assert "pause_latched" not in probe


def test_analysis_is_bounded_and_never_changes_game_vertices():
    probe = trace_block()
    assert "if(frame<1 || frame>20" in probe
    assert "draw>8192" in probe
    assert "draw>8 && (draw%64)!=0" in probe
    assert "draw>=3140 && draw<=3400" in probe
    assert "draw>8192" in probe
    assert "frame==1 || frame==5 || frame==12 || frame==20" in probe
    assert "PAUSE-NATIVE-PROBE: FRAME-END" in probe
    assert "fvf!=0x144UL && fvf!=0x44UL" in probe
    assert "stride=(fvf==0x144UL)?28u:20u;" in probe
    assert "count<3 || count>64" in probe
    assert "texw=desc.dwWidth;texh=desc.dwHeight;" in probe
    assert "IDirectDrawSurface7_Release(texture)" in probe
    for forbidden in (
        "VirtualProtect(", "FlushInstructionCache(",
        "SetTexture(", "SetRenderState(", "memcpy(", "HeapAlloc(",
        "dst[i].x", "src[i].x=", "WM_CLOSE", "SetWindowPos(",
    ):
        assert forbidden not in probe, forbidden
    assert "[NO GEOMETRY MODIFIED]" in probe


def test_hooks_call_probe_without_affecting_existing_render_pipeline():
    draw = CORE.split("static HRESULT WINAPI hooked_draw_primitive(", 1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(", 1)[0]
    indexed = CORE.split("static HRESULT WINAPI hooked_draw_indexed_primitive(", 1)[1].split(
        "static void instrument_d3d_device(", 1)[0]
    assert "pause_menu_trace_draw(self,type,fvf,verts,count,0,__builtin_return_address(0))" in draw
    assert "pause_menu_trace_draw(self,type,fvf,verts,count,1,__builtin_return_address(0))" in indexed
    assert draw.index("pause_menu_trace_draw(") < draw.index("original_draw_primitive(")
    assert indexed.index("pause_menu_trace_draw(") < indexed.index("original_draw_indexed_primitive(")
    init = CORE.split("static DWORD WINAPI initialize(", 1)[1]
    assert "SpiderCfg_ValidateCurrentIni(log_line);" in init
    assert init.index("SpiderCfg_ValidateCurrentIni(log_line)") < init.index("pause_menu_trace_init();")
    assert init.index("pause_menu_trace_init();") < init.index("SpiderQuickExitF12_Init(log_line);")


def test_diagnostic_docs_define_43_reference_and_next_step():
    assert "1920×1080" in DOC
    assert "1440×1080" in DOC
    assert "240" in DOC
    assert "0.75" in DOC
    assert "PAUSE-NATIVE-PROBE" in DOC
    assert "F10" in DOC and "F9" in DOC
    assert "do not" in DOC.lower() or "ne pas" in DOC.lower()

def test_probe_falls_back_to_bounded_passive_escape_sampling():
    src = CORE.split("/* Diagnostic only: classify original in-game ESC pause menu geometry",1)[1]
    init = src.split("static void pause_menu_trace_draw(",1)[0]
    draw = src.split("static void pause_menu_trace_draw(",1)[1].split(
        "static void free_menu_vertices(",1)[0]
    assert "GetAsyncKeyState(VK_ESCAPE)&0x8000" in init
    assert "pause_menu_trace_escape_until,flip+24" in init
    assert "if(edge<=4)" in init
    assert "frames 1/5/12/20" in init
    assert "PAUSE-NATIVE-PROBE: SAMPLE-FRAME" in draw
    assert "GetTexture(" in draw
    assert "InterlockedIncrement(&pause_menu_trace_escape_edges)" in init
    assert "PAUSE-NATIVE-PROBE: ESC-EDGE" in init
    assert "pause_menu_trace_on_flip(count);" in CORE
    assert "pause_menu_trace_escape_until" in draw
    assert "SPIDER_NATIVE_ESC_PAUSE_CONTEXT" in draw
    assert "[NO GEOMETRY MODIFIED]" in draw
    for bad in ("SetAsyncKeyState(", "SendInput(", "PostMessage(", "WM_CLOSE"):
        assert bad not in init
    assert "hold Esc for about half a second" in DOC

def test_esc_menu_48_new_draws_are_sampled_not_repositioned():
    probe = trace_block()
    assert "3268 candidates in frames 1..4" in probe
    assert "3316 in" in probe
    assert "draw>=3140 && draw<=3400" in probe
    assert "frame==1 || frame==5 || frame==12 || frame==20" in probe
    assert "PAUSE-NATIVE-PROBE: frame=%ld draw=%ld" in probe
    assert "draw>8192" in probe
    assert "SPIDERMAN2K1FIX_PAUSE_MENU_TRACE" not in INI
    assert "48 primitives" in DOC
    assert "3200" in DOC and "3400" in DOC


def test_post_esc_confirm_probe_is_passive_time_bounded_and_key_triggered():
    probe = trace_block()
    assert "GetAsyncKeyState(VK_RETURN)&0x8000" in probe
    assert "GetAsyncKeyState(VK_SPACE)&0x8000" in probe
    assert "pause_menu_trace_action_armed_until,flip+1800" in probe
    assert "pause_menu_trace_action_edges" in probe
    assert "if(n<=3)" in probe
    assert "PAUSE-NATIVE-PROBE: KEY-EDGE" in probe
    assert "menu state UNVERIFIED" in probe
    assert "confirmation" in DOC.lower() and "NO/YES" in DOC
    for unsafe in ("SendInput(", "keybd_event(", "PostMessage(", "SetWindowLong("):
        assert unsafe not in probe


def test_wide_screen_panel_candidates_remain_observational_only():
    probe = trace_block()
    assert "pause_menu_trace_wide_seen" in probe
    assert "wide_overlay_candidate=" in probe
    assert "InterlockedIncrement(&pause_menu_trace_wide_seen)<=12" in probe
    assert "count==4" in probe
    assert "desktop_w*0.60f" in probe
    assert "!wide_overlay_candidate" in probe
    assert "NO GEOMETRY MODIFIED" in probe
    assert "wide" in DOC.lower()


def test_opt_in_native_esc_43_never_rewrites_hud_by_default():
    probe = CORE.split("/* Experimental native ESC panel transform, opt-in only.",1)[1].split(
        "static void free_menu_vertices(",1)[0]
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_NATIVE_ESC_43"' in probe
    assert "native_pause43_enabled=0;" in probe
    assert "menu_upscale_active" in probe and "gameplay_4x3_active" in probe
    assert "rva!=0x00102C72UL" in probe
    assert "native_pause43_banner_flip" in probe
    assert "native_pause43_banner_flip,flip" in probe
    assert "if(!glyph)return source;" in probe
    assert "th==32 && (tw==16 || tw==32 || tw==64)" in probe
    assert "0.00122f" in probe and "11.176f" in probe
    assert "0.00098f" in probe and "12.686f" in probe
    assert "memcpy(copy,v,4*sizeof(*copy));" in probe
    assert "copy[i].x=center+(v[i].x-center)*aspect_scale;" in probe
    assert "SPIDERMAN2K1FIX_NATIVE_ESC_43" not in INI
    for disallowed in ("VirtualProtect(", "SetTexture(", "PostMessage(", "SendInput(", "WM_CLOSE", "src[i].x="):
        assert disallowed not in probe, disallowed
    draw = CORE.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert "maybe_correct_native_pause43(" in draw
    assert draw.index("maybe_correct_adaptive_hud(") < draw.index("maybe_correct_native_pause43(")
    assert draw.index("maybe_correct_native_pause43(") < draw.index("maybe_correct_horplus(")
    assert "native_pause43_init();" in CORE
    assert "selector not yet classified" in probe
    assert "3140..3400" in CORE
    assert "PAUSE-NATIVE-43:" in DOC
