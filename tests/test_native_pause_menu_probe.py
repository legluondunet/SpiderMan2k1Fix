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
    assert "float mapped=center+(v[i].x-center)*aspect_scale;" in probe
    assert "copy[i].x=mapped;" in probe
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
    assert "float mapped=center+(v[i].x-center)*aspect_scale;" in source
    assert "copy[i].x=mapped;" in source
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
    assert "if(selector_shape_sample || selector_sample || icon_region_sample || spider_pose_sample)" in probe
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


def test_native_cursor_exact_quad_signature_replaces_unproven_stack_owner():
    code=CORE.split("/* Native gameplay ESC red selector:",1)[1].split(
        "static void free_menu_vertices(",1)[0]
    assert 'SPIDERMAN2K1FIX_NATIVE_CURSOR_43' in code
    assert "native_cursor43_enabled=0;" in code
    assert "native_pause43_enabled,0,0)!=1" in code
    assert "native_pause43_banner_flip,0,0)!=flip" in code
    assert "desktop_w!=1920 || desktop_h!=1080" in code
    assert "menu_upscale_active" in code
    assert "gameplay_4x3_active" in code
    assert "!geom_rva(ret,&rva) || rva!=0x00102C72UL" in code
    assert "type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL || count!=4" in code
    assert "(right-left)-96.0f" in code
    assert "(bottom-top)-72.0f" in code
    assert "fabsf(z)>0.00005f" in code
    assert "fabsf(rhw-27.519f)>0.035f" in code
    assert "diffuse!=0xFFFFFFFFUL" in code
    assert "GetTexture(" in code
    assert "GetSurfaceDesc(" in code
    assert "IDirectDrawSurface7_Release(tex)" in code
    assert "desc.dwWidth!=32 || desc.dwHeight!=32" in code
    assert "desc.ddpfPixelFormat.dwFlags!=0x41UL" in code
    assert "memcpy(copy,v,4*sizeof(*copy))" in code
    assert "copy[i].x=cx+0.75f*(v[i].x-cx)" in code
    assert "copy[i].y=" not in code
    assert "copy[i].z=" not in code
    assert "copy[i].rhw=" not in code
    assert "NATIVE-CURSOR-43: QUAD-MATCH" in code
    assert "RtlCaptureStackBackTrace" not in code
    assert "native_cursor43_scan_live_stack" not in code
    assert "native_cursor43_owner" not in code
    assert "SPIDERMAN2K1FIX_NATIVE_CURSOR_43" not in INI
    assert "96x72" in DOC and "QUAD-MATCH" in DOC
    for unsafe in ("VirtualProtect(", "PostMessage(", "SendInput(", "SetMousePos(", "SetTexture("):
        assert unsafe not in code
    draw=CORE.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert "maybe_correct_native_cursor43(" in draw
    assert draw.index("maybe_correct_adaptive_hud(") < draw.index("maybe_correct_native_cursor43(")
    assert draw.index("maybe_correct_native_cursor43(") < draw.index("maybe_correct_native_pause43(")
    assert "native_cursor43_init();" in CORE


def test_native_cursor_43_is_opt_in_and_safe_outside_gameplay_esc():
    block=CORE.split("static const void *maybe_correct_native_cursor43(",1)[1].split(
        "static void free_menu_vertices(",1)[0]
    assert "if(InterlockedCompareExchange(&native_cursor43_enabled,0,0)!=1" in block
    assert "native_pause43_banner_flip" in block
    assert "for(i=0;i<4;i++)" in block
    assert "copy[i].x=cx+0.75f*(v[i].x-cx)" in block
    assert "SPIDERMAN2K1FIX_NATIVE_CURSOR_43" in DOC


def test_build_767_selector_shape_probe_is_read_only_and_not_glyph_quota():
    probe = trace_block()
    assert "pause_menu_trace_shape_seen=0;" in probe
    assert "InterlockedExchange(&pause_menu_trace_shape_seen,0)" in probe
    assert "selector_shape_candidate=0,selector_shape_sample=0" in probe
    assert "xmax-xmin>=78.0f && xmax-xmin<=170.0f" in probe
    assert "ymax-ymin>=52.0f && ymax-ymin<=130.0f" in probe
    assert "InterlockedIncrement(&pause_menu_trace_shape_seen)<=32" in probe
    assert "selector_shape_sample=1" in probe
    assert "SELECTOR-SHAPE" in probe
    assert "selector_shape_sample?" in probe
    assert "!selector_shape_sample)return;" in probe
    assert "SPIDER-POSE" in probe
    assert "CURSOR-CANDIDATE" in probe
    assert "32/frame" in probe
    assert "96x65" in DOC and "SELECTOR-SHAPE" in DOC
    # The diagnostic must never modify vertices or device state.
    for forbidden in ("VirtualProtect(", "DrawPrimitive(", "SetTexture(", "SendInput(", "PostMessage(", "keybd_event(", "SpiderMenu_TransformVertices("):
        assert forbidden not in probe


def test_esc_panel_original_43_outline_is_pixel_calibrated_without_touching_glyphs():
    block=CORE.split("static const void *maybe_correct_native_pause43(",1)[1].split(
        "/* Native gameplay ESC red selector:",1)[0]
    assert "if(desktop_w==1920 && desktop_h==1080)" in block
    assert "if(border)" in block
    assert "else if(ribbon)" in block
    assert "1439.0f/1479.0f" in block
    assert "1439.0f/1448.5f" in block
    assert "mapped-963.0f" in block
    assert "mapped-958.6f" in block
    assert "copy[i].x=mapped" in block
    assert "copy[i].y=" not in block
    # The original screenshot: border x=241..1679 (1439-pixel row).
    # Current wide screenshot: border x=224..1702 (1479 pixels).
    # Center-of-extent correction is intentionally separated from
    # the untextured fill and from all text glyphs.
    calibrate_border=lambda x: 960.0+(x-963.0)*(1439.0/1479.0)
    assert abs(calibrate_border(224)-241)<=1
    assert abs(calibrate_border(1702)-1679)<=1
    # Native untextured ribbon x=-7.5..1923.8 maps differently from
    # the outer corners; shrinking it by the border factor would clip
    # its interior too aggressively.
    calibrate_fill=lambda x:960.0+(x-958.6)*(1439.0/1448.5)
    for original,target in [(-7.5,241),(1923.8,1679)]:
        mapped=960+(original-960)*0.75
        assert abs(calibrate_fill(mapped)-target)<=2
    assert "if(border)" in block and "else if(ribbon)" in block


def test_backdrop_motif_probe_is_passive_and_not_world_scaling():
    probe=trace_block()
    assert "pause_menu_trace_motif_seen=0;" in probe
    assert "InterlockedExchange(&pause_menu_trace_motif_seen,0);" in probe
    assert "MOTIF-CANDIDATE" in probe
    assert "SCENE OR UI UNKNOWN; READ ONLY" in probe
    assert "InterlockedIncrement(&pause_menu_trace_motif_seen)<=28" in probe
    assert "(draw<=16 || (draw%67)==0)" in probe
    assert "(frame==1 || frame==5 || frame==12 || frame==20)" in probe
    assert "GetTexture(" in probe and "IDirectDrawSurface7_Release(surf)" in probe
    assert "memcpy(&color,raw+16,sizeof(color));" in probe
    assert "if(fvf==0x144UL" in probe
    assert "MOTIF-CANDIDATE" in DOC
    assert "not yet" in DOC.lower() and "identified" in DOC.lower()
    for forbidden in ("VirtualProtect(", "memcpy(copy", "SetTexture(", "DrawPrimitive(", "SendInput(", "PostMessage("):
        assert forbidden not in probe
