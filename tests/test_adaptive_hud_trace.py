"""Safeguards for opt-in, read-only D3D7 HUD measurement.

The original F12 sampler helped identify compass/instruction quads. Its
next read-only investigation detects uncorrected BOSS portraits/health bars
in the upper-right quadrant. Default rendering remains unchanged.
"""
from pathlib import Path

SRC = Path("spiderman2k1fix.c").read_text(encoding="utf-8")
HUD = Path("hud_widescreen.c").read_text(encoding="utf-8")
INI = Path("SpiderMan2k1Fix.ini").read_text(encoding="utf-8")


def test_sampler_is_opt_in_and_never_forces_hud_hook():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_HUD_TRACE"' in SRC
    assert "if(debug_log_enabled &&" in SRC
    assert "InterlockedExchange(&hud_probe_enabled,1);" in SRC
    assert 'HUDWidescreenFix=1' in INI
    assert 'GetPrivateProfileIntA("HUD","HUDWidescreenFix",1,ini)' in SRC


def test_sampler_captures_only_bounded_gameplay_quads_on_f12():
    assert "GetAsyncKeyState(VK_F12)&1" in SRC
    assert "InterlockedExchange(&hud_probe_frames,6)" in SRC
    assert "InterlockedDecrement(&hud_probe_frames)" in SRC
    assert "InterlockedExchange(&hud_probe_seen,0)" in SRC
    assert "n>192" in SRC
    assert "slot>(region==4?32:16)" in SRC
    assert "InterlockedCompareExchange(&menu_upscale_active,0,0)!=0" in SRC
    assert "InterlockedCompareExchange(&fov_game_height,0,0)<=0" in SRC
    assert "type!=D3DPT_TRIANGLEFAN || count!=4" in SRC
    assert 'fvf!=0x144UL' in SRC
    assert 'region==4?"boss":region==1?"compass":region==2?"instructions":"player"' in SRC


def test_probe_observes_real_texture_and_preserves_original_geometry():
    assert "hud_probe_draw(self,type,fvf,verts,count,__builtin_return_address(0));" in SRC
    assert "original_draw_primitive(self,type,fvf,(LPVOID)v,count,flags)" in SRC
    assert "IDirect3DDevice7_GetTexture(" in SRC
    assert "IDirectDrawSurface7_Release(texture);" in SRC
    assert "geom_rva(caller,&rva)" in SRC
    assert "HUD-PROBE: region=%s" in SRC
    probe = SRC.split("static void hud_probe_draw(", 1)[1].split(
        "static const void *scale_menu_vertices", 1
    )[0]
    assert "VirtualProtect" not in probe
    assert "v[i].x=" not in probe
    assert "v[i].y=" not in probe




def test_boss_probe_is_readonly_and_top_right_only():
    assert "hud_probe_boss=0" in SRC
    assert "InterlockedExchange(&hud_probe_boss,0);" in SRC
    assert "region==4?&hud_probe_boss" in SRC
    assert "cy<(float)desktop_h*0.35f && cx>(float)desktop_w*0.50f" in SRC
    assert "v[0].z>=-0.01f && v[0].z<0.10f && v[0].rhw>0.5f" in SRC
    assert 'region==4?"boss"' in SRC
    assert "HUD-PROBE: region=%s" in SRC
    # Preserve scope safety: this feature only collects positions/metadata
    # while F12 trace is running and does not pass corrected vertices onward.
    probe = SRC.split("static void hud_probe_draw(", 1)[1].split(
        "static int geom_rva(", 1
    )[0]
    assert "memcpy(" not in probe
    assert "VirtualProtect" not in probe
    assert "hud_probe_frames" in probe


def test_damaged_boss_probe_sees_other_primitive_shapes_and_blt():
    # First F12 sampler only sees FVF=0x144 triangle-fan quads. The
    # unscaled black health-loss block was absent from those signatures.
    # Extra diagnostics extend read-only tracing to other D3D kinds
    # and explicit color fills without any new render-state hooks.
    assert "static void hud_probe_boss_unknown(" in SRC
    assert "hud_probe_boss_unknown(self,type,fvf,verts,count,0," in SRC
    assert "hud_probe_boss_unknown(self,type,fvf,verts,count,1," in SRC
    assert 'fvf!=0x44UL && fvf!=0x144UL' in SRC
    assert 'stride=(fvf==0x44UL)?20u:sizeof(SpiderMenuVertex)' in SRC
    assert "if(slot>64)return;" in SRC
    assert "HUD-PROBE-EXTRA: source=%s" in SRC
    assert "HUD-PROBE-BLT: slot=%ld" in SRC
    assert "InterlockedExchange(&hud_probe_boss_extra,0);" in SRC
    assert "InterlockedExchange(&hud_probe_boss_blt,0);" in SRC
    assert "InterlockedCompareExchange(&hud_probe_frames,0,0)>0" in SRC
    # No extra hooks or replacements: probes never produce a replacement
    # vertex array, and the indexed path still submits its original draw.
    body = SRC.split("static void hud_probe_boss_unknown(", 1)[1].split(
        "static void hud_probe_draw(", 1
    )[0]
    assert "VirtualProtect" not in body
    assert "original_draw" not in body
    assert "hud_probe_frames" in body
    assert "memcpy(&x," in body
    assert "memcpy(&y," in body


def test_existing_hud_is_sha_gated_and_original_43_is_identity():
    assert "if (!image_verified" in HUD
    assert "if ((int64_t)width * 3 <= (int64_t)height * 4)" in HUD
    assert "return 1.0f;" in HUD
    assert 'option=GetPrivateProfileIntA("HUD","HUDWidescreenFix",1,ini)' in SRC
    assert 'HUD: disabled automatically for original 4:3 mode' in SRC


def test_full_game_pause_and_production_display_are_unchanged():
    assert "hud_probe_flip();" in SRC
    assert "SpiderDI8Retry_OnFlip(count);" in SRC
    assert "pause_at_flip_if_background();" in SRC
    assert "SpiderPauseSurface_Begin(" in SRC
