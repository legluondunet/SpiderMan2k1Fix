"""Original Dreamcast aspect for second source-verified boss life HUD variant.

This test uses actual 1920x1080 Rhino F9/Log=1 D3D7 quads and
compares the projected health bar/portrait to native Dreamcast pixels.
Earlier Venom/Scorpion HUD, world effects, mouse cursor and Pause
must never match this new depth/texture/tint composite signature.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT=Path(__file__).resolve().parents[2]
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")


def test_rhino_boss_ui_centered_43_reprojection_matches_dreamcast(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        pytest.skip("host C compiler unavailable")
    source=tmp_path/"boss_native.c"
    exe=tmp_path/"boss_native"
    source.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <math.h>
        #include <stddef.h>
        #include <stdint.h>
        #include <string.h>
        #include "hud_boss_native.h"

        static float abs_diff(float a,float b){return fabsf(a-b);}
        static void quad(SpiderMenuVertex *v,float l,float t,float r,
                         float b,float z,float rhw,uint32_t color){
            unsigned i;
            memset(v,0,4*sizeof(*v));
            for(i=0;i<4u;i++){
                v[i].x=(i==1 || i==2)?r:l;
                v[i].y=(i>=2)?b:t;
                v[i].z=z;v[i].rhw=rhw;v[i].diffuse=color;
                v[i].u=(i==1 || i==2)?1.0f:0.0f;
                v[i].v=(i>=2)?1.0f:0.0f;
            }
        }
        static int run(SpiderMenuVertex *src,SpiderMenuVertex *dst,
                       unsigned tw,unsigned th,unsigned w,unsigned h,
                       uint32_t caller) {
            return SpiderHudBossNative_Transform(src,dst,4,6,0x144,
                        caller,tw,th,w,h);
        }
        static void keep_3d_and_y(const SpiderMenuVertex *src,
                                  const SpiderMenuVertex *dst,
                                  unsigned w,unsigned h) {
            unsigned i;
            float scale=4.0f*(float)h/(3.0f*(float)w);
            float center=(float)w*0.5f;
            for(i=0;i<4u;i++){
                assert(abs_diff(dst[i].x,
                    center+(src[i].x-center)*scale)<0.002f);
                assert(memcmp((const char*)&src[i]+offsetof(SpiderMenuVertex,y),
                              (const char*)&dst[i]+offsetof(SpiderMenuVertex,y),
                              sizeof(SpiderMenuVertex)-
                              offsetof(SpiderMenuVertex,y))==0);
            }
        }
        int main(void){
            SpiderMenuVertex src[4],dst[4];
            /* Rhino bar right-hand gradient: the actual original
             * uncorrected 307.4x36px F9+Log=1 sprite. */
            quad(src,1383.8f,130.5f,1691.2f,166.5f,
                 0.00098f,8.0113f,0xFFFFFF00u);
            src[1].diffuse=0xFFFFA000u; /* allow gradient corners */
            assert(run(src,dst,0,0,1920,1080,0x00102C72)==
                   SPIDER_HUD_BOSS_PROBE_FILL);
            keep_3d_and_y(src,dst,1920,1080);
            assert(abs_diff(dst[0].x,1277.85f)<0.02f);
            assert(abs_diff(dst[1].x,1508.40f)<0.02f);
            /* Green left half of same bar/layer: do NOT require
             * the previously probed right-half color. */
            quad(src,1080.0f,130.5f,1383.8f,166.5f,
                 0.00098f,8.0113f,0xFF00FF00u);
            assert(run(src,dst,0,0,1920,1080,0x00102C72)==
                   SPIDER_HUD_BOSS_PROBE_FILL);
            keep_3d_and_y(src,dst,1920,1080);
            assert(abs_diff(dst[0].x,1050.0f)<0.02f);
            assert(abs_diff(dst[1].x,1277.85f)<0.02f);
            /* Source-logged 32x32 edge pieces and 64x64 portrait. */
            quad(src,1590.0f,108.0f,1687.5f,180.0f,
                 0.00024f,17.1292f,0xFFFFFFFFu);
            assert(run(src,dst,32,32,1920,1080,0x00102C72)==
                   SPIDER_HUD_BOSS_PROBE_FRAME);
            keep_3d_and_y(src,dst,1920,1080);
            assert(abs_diff(dst[0].x,1432.5f)<0.02f);
            assert(abs_diff(dst[1].x,1505.625f)<0.02f);
            quad(src,1680.0f,72.0f,1848.8f,211.5f,
                 0.00024f,17.1292f,0xFFFFFFFFu);
            assert(run(src,dst,64,64,1920,1080,0x00102C72)==
                   SPIDER_HUD_BOSS_PROBE_PORTRAIT);
            keep_3d_and_y(src,dst,1920,1080);
            assert(abs_diff(dst[0].x,1500.0f)<0.02f);
            assert(abs_diff(dst[1].x,1626.6f)<0.02f);
            /* 16:10 still returns the geometry-preserving native 4:3
             * aspect, while true 4:3 does not touch original pixels. */
            assert(run(src,dst,64,64,1920,1200,0x00102C72)==
                   SPIDER_HUD_BOSS_PROBE_PORTRAIT);
            keep_3d_and_y(src,dst,1920,1200);
            assert(run(src,dst,64,64,1440,1080,0x00102C72)==0);
            /* Different source caller, FVF, primitive, texture,
             * non-opaque tint, RHW, Z or player-HUD bounds all fail. */
            assert(run(src,dst,64,64,1920,1080,0x00102C73)==0);
            assert(run(src,dst,32,32,1920,1080,0x00102C72)==0);
            assert(SpiderHudBossNative_Transform(src,dst,4,5,0x144,
                  0x00102C72,64,64,1920,1080)==0);
            assert(SpiderHudBossNative_Transform(src,dst,4,6,0x142,
                  0x00102C72,64,64,1920,1080)==0);
            quad(src,1680.0f,72.0f,1848.8f,211.5f,
                 0.00024f,21.3309f,0xFFFFFFFFu); /* earlier boss */
            assert(!SpiderHudBossNative_MightMatch(src));
            assert(run(src,dst,64,64,1920,1080,0x00102C72)==0);
            quad(src,1680.0f,72.0f,1848.8f,211.5f,
                 0.00122f,17.1292f,0xFFFFFFFFu);
            assert(run(src,dst,64,64,1920,1080,0x00102C72)==0);
            quad(src,1680.0f,72.0f,1848.8f,211.5f,
                 0.00024f,17.1292f,0x80FFFFFFu);
            assert(run(src,dst,64,64,1920,1080,0x00102C72)==0);
            quad(src,180.0f,72.0f,348.8f,211.5f,
                 0.00024f,17.1292f,0xFFFFFFFFu);
            assert(run(src,dst,64,64,1920,1080,0x00102C72)==0);
            quad(src,1680.0f,72.0f,1848.8f,211.5f,
                 0.00024f,17.1292f,0xFFFFFFFFu);
            src[3].rhw=8.0113f; /* inconsistent quad */
            assert(run(src,dst,64,64,1920,1080,0x00102C72)==0);
            return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c11","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-lm","-o",str(exe)],
                   check=True,capture_output=True,text=True)
    subprocess.run([str(exe)],check=True,capture_output=True,text=True)


def test_native_boss_hook_has_precedence_and_is_not_a_level_switch():
    assert '#include "hud_boss_native.h"' in CORE
    code=CORE.split("static const void *maybe_correct_native_boss_hud(",1)[1].split(
        "static volatile LONG boss_hud_probe_last_flip",1)[0]
    for marker in (
        "SpiderHudWidescreen_Installed()",
        "gameplay_4x3_active", "menu_upscale_active",
        "SpiderHudBossNative_MightMatch(", "rva!=0x00102C72UL",
        "SpiderHudBossNative_Transform(", "IDirectDrawSurface7_Release(texture)",
        "HUD-BOSS-NATIVE:", "hit<=4 || hit%240==0",
    ):
        assert marker in code
    for forbidden in ("SetTexture(","SetRenderState(","SendInput(",
                      "original_draw_primitive(", "SPEL", "RINO"):
        assert forbidden not in code
    draw=CORE.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert draw.index("maybe_correct_adaptive_hud(") < draw.index(
        "maybe_correct_native_boss_hud(") < draw.index(
        "maybe_correct_native_motif43(")
    assert "if(v==verts && count==4)\n      v=maybe_correct_native_boss_hud(" in draw
    assert "maybe_log_boss_hud_candidate(self,type,fvf,verts,count," in draw
    assert "original_draw_primitive(self,type,fvf,(LPVOID)v,count,flags)" in draw
