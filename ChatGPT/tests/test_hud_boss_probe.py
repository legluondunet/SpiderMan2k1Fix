"""Read-only, bounded original D3D7 health-bar/portrait source identification.

The new Rhino screenshots prove horizontal stretching of the *entire*
boss UI, but existing logs only expose one of its 32x32 frame sprites.
Do not apply a guessed live transform to fill or portrait until their
texture, source coordinates, RHW and draw caller are observed.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

import pytest

ROOT = Path(__file__).resolve().parents[2]
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")


def test_boss_probe_classifies_all_three_elements_without_modifying_them(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        pytest.skip("C compiler unavailable")
    source=tmp_path/"boss_probe.c"
    exe=tmp_path/"boss_probe"
    source.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <stdint.h>
        #include <string.h>
        #include "hud_boss_probe.h"

        static void quad(SpiderMenuVertex *v,float l,float t,float r,
                         float b,float z,float rhw,uint32_t color){
            unsigned i;
            memset(v,0,4*sizeof(*v));
            for(i=0;i<4;i++){
                v[i].x=(i==1 || i==2)?r:l;
                v[i].y=(i>=2)?b:t;
                v[i].z=z;v[i].rhw=rhw;v[i].diffuse=color;
            }
        }
        static int k(const SpiderMenuVertex *v,unsigned tw,unsigned th,
                     unsigned w,unsigned h,uint32_t caller) {
            return SpiderHudBossProbe_Classify(v,4,6,0x144,caller,
                                               tw,th,w,h);
        }
        int main(void) {
            SpiderMenuVertex src[4];
            /* Exactly observed Rhino frame/edge sprite (old broad AIM
             * sampler): 32x32, RHW 17.1292, 1590..1687.5. */
            quad(src,1590,117,1687.5f,189,0.00024f,17.1292f,0xFFFFFFFFu);
            assert(k(src,32,32,1920,1080,0x00102C72)
                   ==SPIDER_HUD_BOSS_PROBE_FRAME);
            /* Unverified boss health/portrait *candidate geometries*
             * are collected, not automatically rescaled. */
            quad(src,1080,145,1383.8f,185,0.0010f,9.0f,0xFF00FF00u);
            assert(k(src,0,0,1920,1080,0x00102C72)
                   ==SPIDER_HUD_BOSS_PROBE_FILL);
            quad(src,1680,80,1848.8f,235,0.00024f,17.1292f,0xFFFFFFFFu);
            assert(k(src,64,64,1920,1080,0x00102C72)
                   ==SPIDER_HUD_BOSS_PROBE_PORTRAIT);
            assert(k(src,64,64,1920,1080,0x00102C73)==0);
            assert(k(src,64,64,1440,1080,0x00102C72)==0);
            assert(k(src,32,32,1920,1080,0x00102C72)==0);
            quad(src,180,80,350,235,0.00024f,17.1292f,0xFFFFFFFFu);
            assert(k(src,64,64,1920,1080,0x00102C72)==0);
            quad(src,1680,80,1848.8f,235,0.998f,0.022f,0xFFFFFFFFu);
            assert(k(src,64,64,1920,1080,0x00102C72)==0);
            quad(src,1680,80,1848.8f,235,0.00024f,17.1292f,0x80FFFFFFu);
            assert(k(src,64,64,1920,1080,0x00102C72)==0);
            quad(src,1590,117,1687.5f,189,0.00024f,17.1292f,0xFFFFFFFFu);
            assert(SpiderHudBossProbe_Classify(src,4,5,0x144,0x00102C72,
                                                32,32,1920,1080)==0);
            return 0;
        }
    """), encoding="utf-8")
    subprocess.run([cc,"-std=c11","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(exe)],
                   check=True,capture_output=True,text=True)
    subprocess.run([str(exe)],check=True,capture_output=True,text=True)


def test_probe_is_bounded_and_read_only_and_follows_other_hud_corrections():
    assert '#include "hud_boss_probe.h"' in CORE
    part=CORE.split("static void maybe_log_boss_hud_candidate(",1)[1].split(
        "/* Read-only HUD-visual diagnostics",1)[0]
    for key in ("debug_log_enabled", "HUD-BOSS-PROBE:",
                "boss_hud_probe_hits", "hit>18", "flip-last<45",
                "rva!=0x00102C72UL",
                "SpiderHudBossProbe_Classify(", "GetTexture(",
                "IDirectDrawSurface7_Release(texture)", "[READ-ONLY]"):
        assert key in part
    for forbidden in ("SetTexture(", "SetRenderState(", "SendInput(",
                      "original_draw_primitive(", "dst[i].x="):
        assert forbidden not in part
    draw=CORE.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert draw.index("maybe_correct_adaptive_hud(") < draw.index(
        "maybe_log_boss_hud_candidate(") < draw.index(
        "maybe_correct_aim_reticle(")
    assert "maybe_log_boss_hud_candidate(self,type,fvf,verts,count," in draw
