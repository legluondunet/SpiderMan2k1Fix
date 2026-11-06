"""Source-verified D3D7 red/green 3D aiming rings keep native circular shape.

The GE-Proton Log=1 records below are aligned with original
F9 shots scrn0028/scrn0029 (first scene), scrn0034 (second),
and scrn0036 (Rhino). The 2026-11-06 scrn0090 red ring also
has RHW=9.4993 and was missed by the old fixed-depth whitelist.
The source-verified tint/texture/Z/caller/geometry combination, rather
than the variable RHW, must distinguish the ring from native pause motifs,
other HUD widgets or general-purpose 3D world sprites.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

import pytest

ROOT = Path(__file__).resolve().parents[2]
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")


def test_source_verified_reticles_preserve_other_vertex_attributes(tmp_path):
    cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        pytest.skip("host C compiler unavailable")
    source = tmp_path / "verified_aim.c"
    exe = tmp_path / "verified_aim"
    source.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <math.h>
        #include <stddef.h>
        #include <string.h>
        #include "hud_aim_reticle.h"

        static float delta(float a,float b){return fabsf(a-b);}

        static void quad(SpiderMenuVertex *v,float l,float t,float r,
                         float b,uint32_t color) {
            unsigned i;
            memset(v,0,4*sizeof(*v));
            for(i=0;i<4;i++) {
                v[i].x=(i==1 || i==2)?r:l;
                v[i].y=(i>=2)?b:t;
                v[i].z=0.00146f;
                v[i].rhw=9.9878f;
                v[i].diffuse=color;
                v[i].u=(i==1 || i==2)?1.0f:0.0f;
                v[i].v=(i>=2)?1.0f:0.0f;
            }
        }

        static void set_rhw(SpiderMenuVertex *v,float rhw) {
            unsigned i;
            for(i=0;i<4u;i++)v[i].rhw=rhw;
        }

        static int run(const SpiderMenuVertex *v,SpiderMenuVertex *dst,
                       unsigned w,unsigned h,unsigned tw,unsigned th,
                       unsigned primitive,unsigned fvf,uint32_t caller) {
            return SpiderHudAimReticle_Transform(
               v,dst,4,primitive,fvf,caller,tw,th,w,h);
        }

        static void check(const SpiderMenuVertex *src,const SpiderMenuVertex *dst,
                          unsigned width,unsigned height) {
            unsigned i;
            float x0=(src[0].x+src[2].x)*0.5f;
            float x1=(dst[0].x+dst[2].x)*0.5f;
            float scale=SpiderHudAim_AspectFactor(width,height);
            assert(delta(x0,x1)<0.002f);
            for(i=0;i<4u;i++) {
                assert(delta(dst[i].x,x0+(src[i].x-x0)*scale)<0.003f);
                assert(memcmp((const char*)&src[i]+offsetof(SpiderMenuVertex,y),
                              (const char*)&dst[i]+offsetof(SpiderMenuVertex,y),
                              sizeof(SpiderMenuVertex)-offsetof(SpiderMenuVertex,y))==0);
            }
            assert(delta((dst[1].x-dst[0].x)/
                         (dst[2].y-dst[1].y),1.0f)<0.03f);
        }

        int main(void) {
            SpiderMenuVertex src[4],dst[4];
            quad(src,566.2f,679.5f,723.8f,796.5f,0xB0FF4040u);
            assert(SpiderHudAimReticle_MightMatch(src));
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)
                   ==SPIDER_HUD_AIM_RED);
            check(src,dst,1920,1080);

            quad(src,930.0f,346.5f,1027.5f,418.5f,0xB040FF40u);
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)
                   ==SPIDER_HUD_AIM_GREEN);
            check(src,dst,1920,1080);

            /* Level 2: separate, log-proven RHW=7.8952 layer. Both
             * ring colors use the SAME texture/Z/caller/FVF/UV as
             * level 1. Screenshot scrn0034 captures the red ring,
             * near Log=1 sample=39 at Flip 4687. */
            quad(src,1110.0f,265.5f,1357.5f,450.0f,0xB0FF4040u);
            set_rhw(src,7.8952f);
            assert(SpiderHudAimReticle_MightMatch(src));
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)
                   ==SPIDER_HUD_AIM_RED);
            check(src,dst,1920,1080);

            quad(src,832.5f,310.5f,1080.0f,495.0f,0xB040FF40u);
            set_rhw(src,7.8952f);
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)
                   ==SPIDER_HUD_AIM_GREEN);
            check(src,dst,1920,1080);

            /* Spidey vs Rhino: matching 01:01:23 F9 scrn0036 log,
             * red source quad at RHW=5.9130, diffuse B0FF4040.
             * Identity must be sprite-specific, not level-name-specific. */
            quad(src,637.5f,652.5f,832.5f,796.5f,0xB0FF4040u);
            set_rhw(src,5.9130f);
            assert(SpiderHudAimReticle_MightMatch(src));
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)
                   ==SPIDER_HUD_AIM_RED);
            check(src,dst,1920,1080);


            /* New source-backed fullscreen example (scrn0090):
             * at RHW=9.4993 the red ring was 213.7x162 px before
             * correction and stayed oval in main's first F9 capture.
             * Both red and green share the geometry-based policy. */
            quad(src,858.8f,468.0f,1072.5f,630.0f,0xB0FF4040u);
            set_rhw(src,9.4993f);
            assert(SpiderHudAimReticle_MightMatch(src));
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)
                   ==SPIDER_HUD_AIM_RED);
            check(src,dst,1920,1080);
            for(unsigned i=0;i<4u;i++)src[i].diffuse=0xB040FF40u;
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)
                   ==SPIDER_HUD_AIM_GREEN);
            check(src,dst,1920,1080);

            /* Perspective is continuous: these values are valid when
             * all OTHER original reticle identity gates are satisfied. */
            set_rhw(src,6.90f);
            assert(SpiderHudAimReticle_MightMatch(src));
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)
                   ==SPIDER_HUD_AIM_GREEN);
            set_rhw(src,8.90f);
            assert(SpiderHudAimReticle_MightMatch(src));
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)
                   ==SPIDER_HUD_AIM_GREEN);

            /* Rhino-shared depth is never sufficient by itself:
             * the Pause motif at 5.9130 has alpha 0x80 and different
             * color/texture, and must remain untouched. */
            for(unsigned i=0;i<4u;i++)src[i].diffuse=0x80208686u;
            assert(!SpiderHudAimReticle_MightMatch(src));
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)==0);
            for(unsigned i=0;i<4u;i++)src[i].diffuse=0xB0FF4040u;
            set_rhw(src,0.0f); /* invalid perspective */
            assert(!SpiderHudAimReticle_MightMatch(src));
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)==0);
            set_rhw(src,-1.0f);
            assert(!SpiderHudAimReticle_MightMatch(src));
            set_rhw(src,129.0f); /* unrelated extreme perspective */
            assert(!SpiderHudAimReticle_MightMatch(src));
            set_rhw(src,7.8952f);
            src[0].rhw=9.9878f; /* inconsistent corner depth */
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)==0);

            /* Position independent: a moving ring stays centered on
             * whichever object it targets, never on the screen. */
            quad(src,701.2f,675.0f,855.0f,792.0f,0xB0FF4040u);
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)
                   ==SPIDER_HUD_AIM_RED);
            check(src,dst,1920,1080);

            /* 16:10: same original 4:3 texture aspect, different
             * horizontal scale 5/6. Reject in original 4:3. */
            quad(src,800,400,920,500,0xB040FF40u);
            assert(run(src,dst,1920,1200,64,64,6,0x144,0x00102C72)
                   ==SPIDER_HUD_AIM_GREEN);
            check(src,dst,1920,1200);
            assert(run(src,dst,1440,1080,64,64,6,0x144,0x00102C72)
                   ==SPIDER_HUD_AIM_NONE);

            quad(src,930.0f,346.5f,1027.5f,418.5f,0xB040FF40u);
            assert(run(src,dst,1920,1080,32,32,6,0x144,0x00102C72)==0);
            assert(run(src,dst,1920,1080,0,0,6,0x144,0x00102C72)==0);
            assert(run(src,dst,1920,1080,64,64,5,0x144,0x00102C72)==0);
            assert(run(src,dst,1920,1080,64,64,6,0x142,0x00102C72)==0);
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C73)==0);
            src[0].z=0.25f;
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)==0);
            src[0].z=0.00146f;
            /* The same RHW=7.8952 is ALSO used by the native Pause
             * motif: reject its tint even when all vertices match. */
            set_rhw(src,7.8952f);
            for(unsigned i=0;i<4;i++)src[i].diffuse=0x80208686u;
            assert(!SpiderHudAimReticle_MightMatch(src));
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)==0);
            for(unsigned i=0;i<4;i++)src[i].diffuse=0xB040FF40u;
            set_rhw(src,9.9878f);
            src[0].rhw=11.176f; /* different HUD depth */
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)==0);
            src[0].rhw=9.9878f;
            src[2].diffuse=0xB0FF4040u; /* gradient world sprite */
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)==0);
            src[2].diffuse=src[0].diffuse;
            src[0].u=0.25f;
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)==0);
            src[0].u=0.0f;
            src[1].x=src[0].x;
            src[2].x=src[0].x; /* degenerate/invalid quad */
            assert(run(src,dst,1920,1080,64,64,6,0x144,0x00102C72)==0);
            return 0;
        }
    """), encoding="utf-8")
    subprocess.run(
        [cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
         "-I", str(ROOT), str(source), "-lm", "-o", str(exe)],
        check=True, capture_output=True, text=True
    )
    subprocess.run([str(exe)], check=True, capture_output=True, text=True)


def test_d3d7_ring_hook_is_after_native_ui_and_before_horplus():
    assert '#include "hud_aim_reticle.h"' in CORE
    fn=CORE.split("static const void *maybe_correct_aim_reticle(",1)[1].split(
        "/* Read-only HUD-visual diagnostics",1)[0]
    for required in (
        "SpiderHudWidescreen_Installed()",
        "SpiderHudAimReticle_MightMatch(",
        "rva!=0x00102C72UL",
        "type!=D3DPT_TRIANGLEFAN", "fvf!=0x144UL",
        "tw!=64u || th!=64u",
        "SpiderHudAimReticle_Transform(",
        "IDirectDrawSurface7_Release(texture)",
        "HUD-AIM:", "hit<=4 || hit%240==0",
        "menu_upscale_active", "gameplay_4x3_active",
    ):
        assert required in fn
    assert "SetTexture(" not in fn
    assert "SetRenderState(" not in fn
    assert "original_draw_primitive(" not in fn
    draw=CORE.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert draw.index("maybe_correct_adaptive_hud(") < draw.index(
        "maybe_correct_native_pause43(") < draw.index(
        "maybe_correct_aim_reticle(") < draw.index(
        "maybe_correct_horplus(")
    assert "if(v==verts && count==4)\n      v=maybe_correct_aim_reticle(" in draw
    assert "original_draw_primitive(self,type,fvf,(LPVOID)v,count,flags)" in draw
