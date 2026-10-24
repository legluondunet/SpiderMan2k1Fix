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
        "/* Experimental native ESC panel transform, opt-in only.", 1)[0]


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
    assert "pause_menu_trace_selector_seen" in probe
    assert "CURSOR-CANDIDATE" in probe
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
        "SetTexture(", "SetRenderState(", "memcpy(copy", "HeapAlloc(",
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

def test_native_esc_sampling_does_not_assume_draw_number():
    probe = trace_block()
    assert "Historical draw 3140..3400 sampling missed the cursor" in probe
    assert "draw>=3140 && draw<=3400" not in probe
    assert "frame==1 || frame==5 || frame==12 || frame==20" in probe
    assert "PAUSE-NATIVE-PROBE: frame=%ld draw=%ld" in probe
    assert "draw>8192" in probe
    assert "SPIDERMAN2K1FIX_PAUSE_MENU_TRACE" not in INI
    assert "48 primitives" in DOC
    assert "1439" in DOC


def test_post_esc_confirm_probe_is_passive_time_bounded_and_key_triggered():
    probe = trace_block()
    assert "GetAsyncKeyState(VK_RETURN)&0x8000" in probe
    assert "GetAsyncKeyState(VK_SPACE)&0x8000" in probe
    assert "GetAsyncKeyState(VK_UP)&0x8000" in probe
    assert "GetAsyncKeyState(VK_DOWN)&0x8000" in probe
    assert "up_down && !up_prev" in probe
    assert "down_down && !down_prev" in probe
    assert "pause_menu_trace_action_armed_until,flip+1800" in probe
    assert "pause_menu_trace_action_edges" in probe
    assert "if(n<=6)" in probe
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
    assert "CURSOR-CANDIDATE" in CORE
    assert "PAUSE-NATIVE-43" in DOC


def test_native_esc_border_sprites_have_independent_shape_and_texture_gate():
    source = CORE.split("/* Experimental native ESC panel transform, opt-in only.",1)[1].split(
        "static void free_menu_vertices(",1)[0]
    # The 8x8 textured sides, two lines, and four corner quads were
    # independently observed in the 1920x1080 Wine trace. Their bounds are
    # relative to the preceding untextured banner in the same Flip.
    assert "native_pause43_banner_top" in source
    assert "native_pause43_banner_bottom" in source
    assert "native_pause43_border_hits" in source
    assert "native_pause43_banner_flip,0,0)!=flip" in source
    assert "tw==8 && th==8" in source
    assert "fabsf(z-0.00122f)<0.00010f" in source
    assert "fabsf(rhw-11.176f)<0.25f" in source
    assert "int horizontal=" in source
    assert "int side=" in source
    assert "int corner=" in source
    assert "border=horizontal || side || corner;" in source
    assert "if(!border)" in source
    assert "if(!glyph)return source;" in source
    assert 'border?"border":"glyph"' in source
    assert "memcpy(copy,v,4*sizeof(*copy));" in source
    assert "copy[i].x=center+(v[i].x-center)*aspect_scale;" in source
    assert "SPIDERMAN2K1FIX_NATIVE_ESC_43" not in INI
    assert "8x8" in DOC and "selector" in DOC.lower()
    for unsafe in ("SetTexture(", "PostMessage(", "SendInput(", "keybd_event(", "src[i].x="):
        assert unsafe not in source


def test_red_spider_selector_trace_logs_stable_signature_without_moving_vertices():
    probe = trace_block()
    assert "pause_menu_trace_selector_seen=0;" in probe
    assert "InterlockedExchange(&pause_menu_trace_selector_seen,0);" in probe
    assert "InterlockedIncrement(&pause_menu_trace_selector_seen)<=100" in probe
    assert "selector_candidate=1;" in probe
    assert "count==4" in probe and "fvf==0x144UL" in probe
    assert "rva==0x00102C72UL" in probe
    assert "z>=0.00070f && z<=0.0030f" in probe
    assert "rhw>=7.0f && rhw<=16.0f" in probe
    assert "tex_identity=texture;" in probe
    assert "memcpy(&diffuse,raw+16,sizeof(diffuse));" in probe
    assert "memcpy(&u0,raw+20,sizeof(u0));" in probe
    assert "memcpy(&v0,raw+24,sizeof(v0));" in probe
    assert 'selector_sample?"CURSOR-CANDIDATE":"ICON-REGION"' in probe
    assert "IDENTITY UNVERIFIED" in probe
    assert "texid=%p" in probe
    assert "if(selector_sample || icon_region_sample || spider_pose_sample)" in probe
    assert "SPIDERMAN2K1FIX_SELECTOR_FIX" not in CORE
    assert "CURSOR-CANDIDATE" in DOC
    for forbidden in ("SetTexture(", "memcpy(copy", "SetRenderState(", "PostMessage(", "SendInput("):
        assert forbidden not in probe


def test_red_icon_region_diagnostic_is_bounded_read_only_and_not_cursor_fix():
    probe = trace_block()
    assert "pause_menu_trace_icon_seen=0;" in probe
    assert "InterlockedExchange(&pause_menu_trace_icon_seen,0);" in probe
    assert "InterlockedIncrement(&pause_menu_trace_icon_seen)<=100" in probe
    assert "icon_region_candidate=1;" in probe
    assert "icon_region_sample=1;" in probe
    assert "type!=D3DPT_TRIANGLESTRIP" in probe
    assert "type!=D3DPT_TRIANGLELIST" in probe
    assert "(count==4 || count==6)" in probe
    assert "z>=-0.0010f && z<=0.065f" in probe
    assert "rhw>=0.45f && rhw<=30.0f" in probe
    assert "if(fvf==0x144UL)" in probe
    assert "memcpy(&diffuse,raw+16,sizeof(diffuse));" in probe
    assert "PAUSE-NATIVE-PROBE: %s frame=%ld flip=%ld draw=%ld" in probe
    assert "ICON-REGION" in probe and "IDENTITY UNVERIFIED" in probe
    assert "SPIDERMAN2K1FIX_PAUSE_MENU_TRACE" in probe
    assert "ICON-REGION" in DOC
    for forbidden in ("SetTexture(", "memcpy(copy", "SetRenderState(", "SendInput(", "PostMessage("):
        assert forbidden not in probe


def test_f9_screenshot_arms_post_capture_menu_sprite_sampling():
    probe=trace_block()
    assert "static void pause_menu_trace_on_f9(void)" in probe
    assert "pause_menu_trace_f9_events=0;" in probe
    assert "seen>4" in probe
    assert "pause_menu_trace_escape_edges,0,0)<1" in probe
    assert "pause_menu_trace_action_armed_until,0,0" in probe
    assert "pause_menu_trace_escape_until,flip+24" in probe
    assert "PAUSE-NATIVE-PROBE: F9-SNAPSHOT" in probe
    assert "spider_pose_candidate=1;" in probe
    assert "spider_pose_sample=1;" in probe
    assert "SPIDER-POSE" in probe
    assert "(count==3 || count==4 || count==6)" in probe
    assert "rhw>=5.0f && rhw<=25.0f" in probe
    hook=CORE.split("static void __cdecl hooked_f9_png_only(void)",1)[1].split(
        "static void install_f9_png_only_dispatch(",1)[0]
    assert "if(queued)pause_menu_trace_on_f9();" in hook
    assert hook.index("SpiderF9PNG_Queue(&desc,log_line)") < hook.index("pause_menu_trace_on_f9();")
    assert "F9-SNAPSHOT" in DOC
    assert "scrn0010.png" in DOC and "scrn0011.png" in DOC
    for forbidden in ("SetTexture(", "SendInput(", "PostMessage(", "SetRenderState(", "memcpy(copy"):
        assert forbidden not in probe


def test_native_mouse_cursor_transform_requires_exact_game_callstack():
    code=CORE.split("/* Mouse cursor hook experiment, OFF by default.",1)[1].split(
        "static void free_menu_vertices(",1)[0]
    assert "SPIDERMAN2K1FIX_NATIVE_CURSOR_43" in code
    assert "native_cursor43_enabled=0;" in code
    assert "native_cursor43_owner=(BYTE*)exe+0x0010C470u;" in code
    assert "PAGE_EXECUTE_READ" in code
    assert "RtlCaptureStackBackTrace" in code
    assert "native_cursor43_trace(0,20,frames,NULL)" in code
    assert "memcpy(&native_cursor43_trace,&symbol,sizeof(native_cursor43_trace));" in code
    assert "NATIVE-CURSOR-43: stack SAMPLE=" in code
    assert "native_cursor43_cap_logs" in code
    assert "(BYTE*)frames[i]>=native_cursor43_owner" in code
    assert "(BYTE*)frames[i]<native_cursor43_owner+0x40u" in code
    assert "if(!matched)" in code
    assert "native_pause43_banner_flip,0,0)!=flip" in code
    assert "attempts>180" in code
    assert "native_cursor43_enabled,0,0)!=1" in code
    assert "cx+0.75f*(v[i].x-cx)" in code
    assert "memcpy(copy,v,4*sizeof(*copy));" in code
    assert "SPIDERMAN2K1FIX_NATIVE_CURSOR_43" not in INI
    assert "NATIVE-CURSOR-43:" in DOC
    for unsafe in ("VirtualProtect(", "PostMessage(", "SendInput(", "SetMousePos(", "SetTexture("):
        assert unsafe not in code
    draw=CORE.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert "maybe_correct_native_cursor43(" in draw
    assert draw.index("maybe_correct_adaptive_hud(") < draw.index("maybe_correct_native_cursor43(")
    assert draw.index("maybe_correct_native_cursor43(") < draw.index("maybe_correct_native_pause43(")
    assert "native_cursor43_init();" in CORE


def test_cursor_width_transform_is_centered_only_and_opt_in():
    block=CORE.split("static const void *maybe_correct_native_cursor43(",1)[1].split(
        "static void free_menu_vertices(",1)[0]
    assert "for(i=0;i<4;i++)" in block
    assert "copy[i].x=cx+0.75f*(v[i].x-cx)" in block
    assert "copy[i].y=" not in block
    assert "copy[i].z=" not in block
    assert "copy[i].rhw=" not in block
    assert "SPIDERMAN2K1FIX_NATIVE_CURSOR_43" in DOC
