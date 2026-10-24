"""Static regression checks for the Wine-validated native 4:3 ESC overlay.

Only verified D3D7 signatures may be transformed. The original full-screen
dim is replayed into the 16:9 gutters without modifying game-owned vertices.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
INI = (ROOT / "ChatGPT/tests/fixtures/SpiderMan2k1Fix.ini").read_text(encoding="utf-8")
DOC = (ROOT / "docs/NATIVE_PAUSE_MENU_43.md").read_text(encoding="utf-8")


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
                  "native_pause43_enabled", "desktop_w!=1920",
                  "desktop_h!=1080", "gameplay_4x3_active",
                  "menu_upscale_active", "rva!=0x00102C72UL",
                  "fabsf(v[0].z-0.00146f)", "fabsf(v[0].rhw-9.988f)",
                  "(color&0xFF000000UL)!=0x80000000UL",
                  "vertex_color&0xFF000000UL",
                  "IDirect3DDevice7_GetTexture(", "IDirectDrawSurface7_Release(tex)",
                  "copy[i].x=center+(v[i].x-center)*0.75f"):
        assert token in MOTIF
    assert "type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL || count!=4" in MOTIF


def test_gutter_replay_clips_original_triangles_without_center_overdraw():
    for token in ("native_motif43_restore_gutters(",
                  "left_edge=240.0f,right_edge=1680.0f",
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
    triangles = [
        ((0, 0), (1920, 0), (1920, 1080)),
        ((0, 0), (1920, 1080), (0, 1080)),
    ]
    for lo, hi, keep_left in ((0, 240, True), (1680, 1920, False)):
        edges = [clip(t, 240 if keep_left else 1680, keep_left)
                 for t in triangles]
        assert all(lo <= x <= hi for p in edges for x, _ in p)
        assert sum(len(p) >= 3 for p in edges) == 2
        for x, y in ((lo, 0), (hi, 0), (lo, 1080), (hi, 1080)):
            assert any((x, y) in p for p in edges)


def test_cursor_recognition_not_limited_to_old_screen_rectangle():
    for token in ("SPIDERMAN2K1FIX_NATIVE_CURSOR_43",
                  "native_pause43_banner_flip",
                  "desktop_w!=1920 || desktop_h!=1080",
                  "rva!=0x00102C72UL",
                  "(right-left)-96.0f", "(bottom-top)-72.0f",
                  "fabsf(z)>0.00005f", "fabsf(rhw-27.519f)",
                  "diffuse!=0xFFFFFFFFUL",
                  "desc.dwWidth!=32 || desc.dwHeight!=32",
                  "desc.ddpfPixelFormat.dwFlags!=0x41UL",
                  "copy[i].x=cx+0.75f*(v[i].x-cx)"):
        assert token in CURSOR
    guard = CURSOR.split("  /* The selector can legitimately", 1)[1].split(
        "  memcpy(&diffuse", 1)[0]
    # Top-right movement previously reverted the native 96x72 sprite.
    for old in ("left<450.0f", "right>1480.0f",
                "top<325.0f", "bottom>800.0f"):
        assert old not in guard
    for forbidden in ("copy[i].y=", "SetTexture(", "SetMousePos("):
        assert forbidden not in CURSOR


def test_selector_width_is_72_on_center_top_and_screen_edges():
    # Native D3D7 quad 96x72; its transparent sprite edges explain ~67
    # visible red pixels high in F9 images.
    for x, y in ((911, 504), (1308, 260), (1466, 512), (0, 0), (1824, 1008)):
        center = x + 48.0
        left = center + 0.75 * (x - center)
        right = center + 0.75 * (x + 96 - center)
        assert right - left == 72
        assert (left + right) / 2 == center
        assert y + 72 - y == 72


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


def test_native_esc_children_default_on_but_depend_on_installed_hud_master():
    src=CORE.split("/* Native ESC subfeatures are hidden HUD settings",1)[1].split(
        "/* Native gameplay ESC 4:3 panel correction",1)[0]
    assert "SpiderHudWidescreen_Installed()" in src
    assert 'GetPrivateProfileIntA("HUD",key,1,path)!=1' in src
    assert "GetEnvironmentVariableA(legacy_env,override,sizeof(override))" in src
    assert "override[0]=='0'" in src  # optional compatibility/debug opt-out
    assert 'GetPrivateProfileIntA("HUD",key,0,path)' not in src
    assert "return 1;" in src
    assert "static int native_esc43_hud_setting_enabled(" in CORE
    hud=CORE.split("static void install_player_hud(",1)[1].split(
        "static DWORD WINAPI initialize(",1)[0]
    assert 'option=GetPrivateProfileIntA("HUD","HUDWidescreenFix",1,ini);' in hud
    assert "if (option==0) {" in hud
    assert "if (!SpiderHudWidescreen_Install(exe_verified,TRUE))" in hud
    startup=CORE.split("static DWORD WINAPI initialize(",1)[1].split(
        "BOOL WINAPI DllMain(",1)[0]
    assert startup.index("install_player_hud(TRUE);") < startup.index("native_pause43_init();")
    assert startup.index("native_pause43_init();") < startup.index("native_cursor43_init();")
    assert startup.index("native_cursor43_init();") < startup.index("native_motif43_init();")
    assert "supported_exe()" in startup
    assert "SpiderCfg_ValidateCurrentIni(log_line)" in startup
    assert "if(!SpiderHudWidescreen_Installed())return 0;" in src


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
        assert name not in (ROOT / "virtual_spidey_cfg.c").read_text(encoding="utf-8")
    for env in ("SPIDERMAN2K1FIX_NATIVE_ESC_43",
                "SPIDERMAN2K1FIX_NATIVE_MOTIF_43",
                "SPIDERMAN2K1FIX_NATIVE_CURSOR_43"):
        assert env not in INI
    assert "menu_upscale_active" in CURSOR and "gameplay_4x3_active" in CURSOR
    assert "menu_upscale_active" in MOTIF and "gameplay_4x3_active" in MOTIF
