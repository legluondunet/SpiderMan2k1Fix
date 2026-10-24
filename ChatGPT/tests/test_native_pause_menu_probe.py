"""Static regression checks for the Wine-validated native 4:3 ESC overlay.

Only verified D3D7 signatures may be transformed. The original full-screen
dim is replayed into the 16:9 gutters without modifying game-owned vertices.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
INI = (ROOT / "ChatGPT/tests/fixtures/SpiderMan2k1Fix.ini").read_text(encoding="utf-8")
DOC = (ROOT / "docs/references/NATIVE_PAUSE_MENU_43.md").read_text(encoding="utf-8")


def section(begin, end):
    return CORE.split(begin, 1)[1].split(end, 1)[0]


PAUSE = section("/* Native gameplay ESC 4:3 panel correction",
                "/* Native ESC translucent geometric motif")
MOTIF = section("/* Native ESC translucent geometric motif",
                "/* Native gameplay ESC red selector:")
CURSOR = section("/* Native gameplay ESC red selector:",
                 "static void free_menu_vertices(")
DRAW = section("static HRESULT WINAPI hooked_draw_primitive(",
               "static HRESULT WINAPI hooked_draw_indexed_primitive(")


def test_no_obsolete_research_trace_or_unverified_stack_scanning():
    # Research-only diagnostic removed after user validated the image.
    for obsolete in ("pause_menu_trace_", "PAUSE-NATIVE-PROBE",
                     "SPIDERMAN2K1FIX_PAUSE_MENU_TRACE",
                     "RtlCaptureStackBackTrace",
                     "native_cursor43_scan_live_stack",
                     "native_cursor43_owner"):
        assert obsolete not in CORE
    assert "static volatile LONG native_pause43_enabled=0" in PAUSE
    assert "static volatile LONG native_motif43_enabled=0" in MOTIF
    assert "static volatile LONG native_cursor43_enabled=0" in CURSOR


def test_native_pause_classifies_original_banner_border_and_text_without_touching_game_input():
    for token in ("SPIDERMAN2K1FIX_NATIVE_ESC_43", "menu_upscale_active",
                  "gameplay_4x3_active", "native_pause43_banner_flip",
                  "native_pause43_banner_top", "native_pause43_banner_bottom",
                  "rva!=0x00102C72UL", "D3DPT_TRIANGLEFAN",
                  "fvf!=0x144UL", "count!=4", "tw==8 && th==8",
                  "tw==16 || tw==32 || tw==64",
                  "copy[i].x=mapped", "1439.0f/1479.0f",
                  "1439.0f/1448.5f"):
        assert token in PAUSE
    for forbidden in ("PostMessage(", "SendInput(", "SetTexture(",
                      "keybd_event(", "src[i].x=", "copy[i].y="):
        assert forbidden not in PAUSE
    assert "8x8" in DOC and "ribbon" in DOC


def test_motif_requires_untextured_original_alpha_quads_at_exact_depth():
    for token in ("SPIDERMAN2K1FIX_NATIVE_MOTIF_43",
                  "native_pause43_enabled", "!native_esc43_supported_monitor()",
                  "gameplay_4x3_active",
                  "menu_upscale_active", "rva!=0x00102C72UL",
                  "fabsf(v[0].z-0.00146f)", "fabsf(v[0].rhw-9.988f)",
                  "(color&0xFF000000UL)!=0x80000000UL",
                  "vertex_color&0xFF000000UL",
                  "IDirect3DDevice7_GetTexture(", "IDirectDrawSurface7_Release(tex)",
                  "copy[i].x=center+(v[i].x-center)*native_esc43_scale()"):
        assert token in MOTIF
    assert "type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL || count!=4" in MOTIF


def test_gutter_replay_clips_original_triangles_without_center_overdraw():
    for token in ("native_motif43_restore_gutters(",
                  "left_edge=native_esc43_left_gutter()",
                  "right_edge=(float)desktop_w-left_edge",
                  "tri[0]=original[0];tri[1]=original[i+1u];tri[2]=original[i+2u]",
                  "native_motif43_clip_triangle(",
                  "original_draw_primitive(device,D3DPT_TRIANGLEFAN,0x144UL",
                  "NATIVE-MOTIF-43: GUTTER-REPLAY"):
        assert token in MOTIF
    for token in ("r.y=a->y+(b->y-a->y)*t",
                  "r.z=a->z+(b->z-a->z)*t",
                  "r.rhw=a->rhw+(b->rhw-a->rhw)*t",
                  "memcpy((BYTE*)&r+16u,&c,sizeof(c))",
                  "memcpy((BYTE*)&r+offs,&uv,sizeof(uv))"):
        assert token in MOTIF


def test_clipping_model_covers_both_gutters_never_middle():
    def clip(poly, edge, keep_left):
        out = []
        for i, b in enumerate(poly):
            a = poly[i - 1]
            ia = a[0] <= edge if keep_left else a[0] >= edge
            ib = b[0] <= edge if keep_left else b[0] >= edge
            if ia != ib:
                t = (edge - a[0]) / (b[0] - a[0])
                out.append((edge, a[1] + t * (b[1] - a[1])))
            if ib:
                out.append(b)
        return out

    for screen_w, screen_h, expected_gutter in (
        (1920, 1080, 240), (1920, 1200, 160)
    ):
        left = (screen_w - screen_h * (4.0 / 3.0)) / 2
        right = screen_w - left
        assert abs(left - expected_gutter) < 1.0e-4
        triangles = [
            ((0, 0), (screen_w, 0), (screen_w, screen_h)),
            ((0, 0), (screen_w, screen_h), (0, screen_h)),
        ]
        for lo, hi, keep_left in (
            (0, left, True), (right, screen_w, False)
        ):
            edges = [clip(t, left if keep_left else right, keep_left)
                     for t in triangles]
            assert all(lo <= x <= hi for p in edges for x, _ in p)
            assert sum(len(p) >= 3 for p in edges) == 2
            for x, y in ((lo, 0), (hi, 0), (lo, screen_h), (hi, screen_h)):
                assert any((x, y) in p for p in edges)


def test_native_esc_43_geometry_at_16_9_16_10_and_classic_4_3():
    # Widescreen Auto: full-size physical target, 4:3-only native UI geometry.
    for width, height, target_x, target_w, factor in (
        (1920, 1080, 240, 1440, 0.75),
        (1920, 1200, 160, 1600, 5.0 / 6.0),
    ):
        scale = (height * (4.0 / 3.0)) / width
        assert abs(scale - factor) < 1e-7
        x0 = width / 2 + (0 - width / 2) * scale
        x1 = width / 2 + (width - width / 2) * scale
        assert abs(x0 - target_x) < 1e-5
        assert abs(x1 - (target_x + target_w)) < 1e-5
        assert x1 - x0 == target_w
    # A genuine classic 4:3 output must not be transformed twice.
    assert (1440 * 3) == (1080 * 4)
    assert "InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0" in PAUSE
    assert "InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0" in MOTIF
    assert "InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=0" in CURSOR
    assert "return desktop_w==1920 && (desktop_h==1080 || desktop_h==1200)" in CORE
    assert "native_esc43_left_gutter(void)" in CORE
    assert "native_esc43_scale(void)" in CORE


def test_cursor_recognition_not_limited_to_old_screen_rectangle():
    for token in ("SPIDERMAN2K1FIX_NATIVE_CURSOR_43",
                  "native_pause43_banner_flip",
                  "!native_esc43_supported_monitor()",
                  "rva!=0x00102C72UL",
                  "expected_width", "expected_height",
                  "fabsf(z)>0.00005f", "fabsf(rhw-27.519f)",
                  "diffuse!=0xFFFFFFFFUL",
                  "desc.dwWidth!=32 || desc.dwHeight!=32",
                  "desc.ddpfPixelFormat.dwFlags!=0x41UL",
                  "copy[i].x=cx+icon_scale*(v[i].x-cx)"):
        assert token in CURSOR
    guard = CURSOR.split("  /* The selector can legitimately", 1)[1].split(
        "  memcpy(&diffuse", 1)[0]
    # Top-right movement previously reverted the native 96x72 sprite.
    for old in ("left<450.0f", "right>1480.0f",
                "top<325.0f", "bottom>800.0f"):
        assert old not in guard
    for forbidden in ("copy[i].y=", "SetTexture(", "SetMousePos("):
        assert forbidden not in CURSOR


def test_selector_preserves_square_aspect_in_both_widescreen_modes():
    # The expected 96px-wide sprite may retain its 72px height or scale
    # to 80px in 16:10; both should render square after correcting X only.
    for width, height, quad_height in (
        (1920, 1080, 72), (1920, 1200, 80), (1920, 1200, 72)
    ):
        assert width == 1920
        for x, y in ((911, 504), (1308, 260), (1466, 512),
                     (0, 0), (1824, height-72)):
            quad_width = 96
            center = x + quad_width / 2
            scale = quad_height / quad_width
            left = center + scale * (x - center)
            right = center + scale * (x + quad_width - center)
            assert abs((right-left) - quad_height) < 1e-9
            assert (left + right) / 2 == center
            assert (y + quad_height - y) == quad_height
    assert "icon_scale=(bottom-top)/(right-left);" in CURSOR
    assert "(double)(right-left)*icon_scale" in CURSOR


def test_hook_order_preserves_other_3d_2d_and_hud_transforms():
    order = ["maybe_correct_native_comic(", "maybe_correct_adaptive_hud(",
             "maybe_correct_native_motif43(", "native_motif43_restore_gutters(",
             "maybe_correct_native_cursor43(", "maybe_correct_native_pause43(",
             "maybe_correct_horplus(", "original_draw_primitive("]
    positions = [DRAW.index(x) for x in order]
    assert positions == sorted(positions)
    assert "if(v!=verts)" in DRAW
    assert "if(v==verts && count==4)" in DRAW
    assert "native_motif43_restore_gutters(self,(const SpiderMenuVertex*)verts,flags)" in DRAW


def test_native_esc_children_are_permanent_but_depend_on_validated_hud():
    src=CORE.split("static int native_esc43_hud_setting_enabled(",1)[1].split(
        "/* Native gameplay ESC 4:3 panel correction",1)[0]
    assert "SpiderHudWidescreen_Installed()!=0" in src
    assert 'GetPrivateProfileIntA("HUD"' not in src
    assert "return SpiderHudWidescreen_Installed()!=0;" in src
    hud=CORE.split("static void install_player_hud(",1)[1].split(
        "static DWORD initialize_body(",1)[0]
    assert 'GetPrivateProfileIntA("HUD","HUDWidescreenFix"' not in hud
    assert "if(!SpiderHudWidescreen_Install(exe_verified,TRUE))" in hud
    startup=CORE.split("static DWORD initialize_body(",1)[1].split(
        "BOOL WINAPI DllMain(",1)[0]
    assert startup.index("install_player_hud(TRUE);") < startup.index("native_pause43_init();")
    assert startup.index("native_pause43_init();") < startup.index("native_cursor43_init();")
    assert startup.index("native_cursor43_init();") < startup.index("native_motif43_init();")
    assert "supported_exe()" in startup
    assert "SpiderCfg_ValidateCurrentIni(log_line)" in startup

def test_native_esc_hidden_manual_ini_children_and_parent_chain():
    pause=CORE.split("static void native_pause43_init(void)",1)[1].split(
        "static const void *maybe_correct_native_pause43(",1)[0]
    motif=CORE.split("static void native_motif43_init(void)",1)[1].split(
        "static const void *maybe_correct_native_motif43(",1)[0]
    cursor=CORE.split("static void native_cursor43_init(void)",1)[1].split(
        "static const void *maybe_correct_native_cursor43(",1)[0]
    assert '"NativeESC4x3","SPIDERMAN2K1FIX_NATIVE_ESC_43"' in pause
    assert '"NativeMotif4x3","SPIDERMAN2K1FIX_NATIVE_MOTIF_43"' in motif
    assert '"NativeCursor4x3","SPIDERMAN2K1FIX_NATIVE_CURSOR_43"' in cursor
    assert "InterlockedCompareExchange(&native_pause43_enabled,0,0)==1" in motif
    assert "InterlockedCompareExchange(&native_pause43_enabled,0,0)==1" in cursor
    for name in ("NativeESC4x3","NativeMotif4x3","NativeCursor4x3"):
        assert name in DOC
        for ini in ("ChatGPT/tests/fixtures/SpiderMan2k1Fix.ini","ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini",
                    "ChatGPT/tests/fixtures/SpiderMan2k1Fix.user-clean.ini"):
            assert name not in (ROOT / ini).read_text(encoding="utf-8")
        assert f'"{name}=' not in (ROOT / "virtual_spidey_cfg.c").read_text(encoding="utf-8")
    for env in ("SPIDERMAN2K1FIX_NATIVE_ESC_43",
                "SPIDERMAN2K1FIX_NATIVE_MOTIF_43",
                "SPIDERMAN2K1FIX_NATIVE_CURSOR_43"):
        assert env not in INI
    assert "menu_upscale_active" in CURSOR and "gameplay_4x3_active" in CURSOR
    assert "menu_upscale_active" in MOTIF and "gameplay_4x3_active" in MOTIF
