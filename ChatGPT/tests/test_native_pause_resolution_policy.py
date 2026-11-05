"""All-screen-size 4:3 native ESC mapping with strict native draw gates.

Screenshot geometry alone is not evidence that the game's D3D7 signature
matched: player validation is still required after CI.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

import pytest

ROOT = Path(__file__).resolve().parents[2]
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")


def test_original_pause_has_resolution_independent_four_by_three_geometry(tmp_path):
    cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        pytest.skip("C compiler not available")
    script = tmp_path / "native_pause_geometry.c"
    exe = tmp_path / "native_pause_geometry"
    script.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <math.h>
        #include "native_pause_43_policy.h"

        static float delta(float a,float b) { return fabsf(a-b); }
        static void widescreen(unsigned w,unsigned h) {
            const float f=(float)h;
            const float g=((float)w-f*(4.0f/3.0f))*0.5f;
            float scale=SpiderNativePause43_Scale(w,h);
            assert(SpiderNativePause43_Widescreen(w,h));
            assert(SpiderNativePause43_RibbonBounds(
                0.0f,(float)w,0.369f*f,0.631f*f,w,h));
            assert(SpiderNativePause43_RibbonBounds(
                0.0f,(float)w,0.37f*f,0.625f*f,w,h));
            assert(delta(SpiderNativePause43_MapX(0.0f,w,h),g)<0.10f);
            assert(delta(SpiderNativePause43_MapX((float)w,w,h),
                         (float)w-g)<0.10f);
            assert(delta(SpiderNativePause43_MapX((float)w*0.5f,w,h),
                         (float)w*0.5f)<0.10f);
            assert(delta(((float)w*scale)/f,4.0f/3.0f)<0.0001f);
            /* Native panel corners use the same aspect mapping as text;
             * never force a 1439px width on the violet border. */
            if(w==1920u && h==1080u) {
                float left=SpiderNativePause43_MapX(-22.5f,w,h);
                float right=SpiderNativePause43_MapX(1950.0f,w,h);
                assert(delta(left,223.125f)<0.001f);
                assert(delta(right,1702.5f)<0.001f);
                assert(delta(right-left,1479.375f)<0.001f);
                assert(delta(SpiderNativePause43_MapX(-7.5f,w,h),
                             234.375f)<0.001f);
            }
            /* No silent support for near-top 3D particles, small panels
             * or an arbitrary full-width geometry. */
            /* The old mistaken scaled-preview coordinates must not
             * be accepted as evidence for new geometry coverage. */
            assert(!SpiderNativePause43_RibbonBounds(
                0.0f,(float)w,0.316f*f,0.542f*f,w,h));
            assert(!SpiderNativePause43_RibbonBounds(
                0.0f,(float)w,0.02f*f,0.25f*f,w,h));
            assert(!SpiderNativePause43_RibbonBounds(
                0.20f*(float)w,0.80f*(float)w,0.369f*f,0.631f*f,w,h));
        }
        int main(void) {
            unsigned i;
            const unsigned widths[]={1920,1920,1600,2560,1280,3440,5120};
            const unsigned heights[]={1080,1200,900,1440,800,1440,1440};
            for(i=0;i<7;i++) widescreen(widths[i],heights[i]);
            assert(!SpiderNativePause43_Widescreen(1440,1080));
            assert(!SpiderNativePause43_Widescreen(640,480));
            assert(!SpiderNativePause43_Widescreen(1080,1920));
            assert(SpiderNativePause43_Scale(1440,1080)==1.0f);
            assert(SpiderNativePause43_MapX(190,1440,1080)==190.0f);
            assert(!SpiderNativePause43_RibbonBounds(
                   0,1440,341,585,1440,1080));
            /* Two observed source signatures: older accepted menu
             * and level-2 GE-Proton native ESC (2026-11-04 log). */
            assert(SpiderNativePause43_RibbonLayer(0.00122f,11.176f));
            assert(SpiderNativePause43_RibbonLayer(0.00122f,8.9615f));
            /* Rhino boss: same exact untextured banner/geometry
             * but new logged RHW 6.8041, no broad interpolation. */
            assert(SpiderNativePause43_RibbonLayer(0.00122f,6.8041f));
            assert(!SpiderNativePause43_RibbonLayer(0.00122f,7.7f));
            assert(!SpiderNativePause43_RibbonLayer(0.00122f,10.10f));
            assert(!SpiderNativePause43_RibbonLayer(0.00122f,8.70f));
            assert(!SpiderNativePause43_RibbonLayer(0.98f,8.9615f));
            assert(SpiderNativePause43_MotifLayer(0.00146f,9.988f));
            assert(SpiderNativePause43_MotifLayer(0.00146f,7.8952f));
            assert(SpiderNativePause43_MotifLayer(0.00146f,5.9130f));
            assert(!SpiderNativePause43_MotifLayer(0.00146f,6.9f));
            /* Face and shadow glyph layers from the supplied runtime
             * journal: FF000000 at .00122 / 8.9615 and FF8A78D6 or
             * FFFE3242 at .00098 / 10.3609. Preserve older pairs. */
            assert(SpiderNativePause43_TextLayer(0.00122f,11.176f));
            assert(SpiderNativePause43_TextLayer(0.00098f,12.686f));
            assert(SpiderNativePause43_TextLayer(0.00122f,8.9615f));
            assert(SpiderNativePause43_TextLayer(0.00122f,6.8041f));
            /* F9 scrn0045 / Log=1: the third Rhino native ESC
             * foreground layer is .00098/8.0113, texture 16/32x32,
             * color FFFE3242 or FF8A78D6. Its 32/80 uncorrected
             * detail samples prove it must be included separately
             * from the already-correct black shadow 6.8041. */
            assert(SpiderNativePause43_TextLayer(0.00098f,8.0113f));
            assert(!SpiderNativePause43_TextLayer(0.00098f,6.8041f));
            assert(!SpiderNativePause43_TextLayer(0.00098f,8.40f));
            assert(!SpiderNativePause43_TextLayer(0.00122f,8.0113f));
            assert(SpiderNativePause43_TextLayer(0.00098f,10.3609f));
            assert(!SpiderNativePause43_TextLayer(0.00122f,10.3609f));
            assert(!SpiderNativePause43_TextLayer(0.00098f,8.9615f));
            assert(!SpiderNativePause43_TextLayer(0.00122f,10.0f));
            assert(!SpiderNativePause43_TextLayer(0.00098f,11.5f));
            assert(!SpiderNativePause43_TextLayer(0.5f,10.3609f));

            assert(!SpiderNativePause43_MotifLayer(0.00146f,8.90f));
            assert(!SpiderNativePause43_MotifLayer(0.45f,7.8952f));
            assert(!SpiderNativePause43_Widescreen(0,0));
            assert(!SpiderNativePause43_Widescreen(20000,1080));
            return 0;
        }
    """), encoding="utf-8")
    subprocess.run([
        cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I",
        str(ROOT), str(script), "-lm", "-o", str(exe)
    ], check=True, capture_output=True, text=True)
    subprocess.run([str(exe)], check=True, capture_output=True, text=True)


def test_native_pause_still_uses_exact_render_signature_not_only_bounds():
    block=CORE.split("static const void *maybe_correct_native_pause43(",1)[1].split(
        "/* Native ESC translucent geometric motif",1)[0]
    assert '#include "native_pause_43_policy.h"' in CORE
    assert "SpiderNativePause43_RibbonBounds(" in block
    assert "SpiderNativePause43_RibbonLayer(z,rhw)" in block
    assert "copy[i].x=center+(v[i].x-center)*aspect_scale" in block
    assert "1439.0f/1479.0f" not in block
    assert "1439.0f/1448.5f" not in block
    assert "SpiderNativePause43_TextLayer(z,rhw)" in block
    assert "SpiderNativePause43_MotifLayer(v[0].z,v[0].rhw)" in CORE
    assert "rva!=0x00102C72UL" in block
    assert "type!=D3DPT_TRIANGLEFAN" in block
    assert "count!=4" in block
    assert "if(SUCCEEDED(IDirect3DDevice7_GetTexture(" in block
    assert "native_pause43_banner_flip,flip" in block
    assert "copy[i].x=center+(v[i].x-center)*aspect_scale" in block
    assert "gameplay_4x3_active" in block
    assert "menu_upscale_active" in block
    assert "native_esc43_supported_monitor()" in CORE


def test_3d_scene_letterboxes_and_native_bink_paths_remain_untouched():
    # Existing separated 3D/FOV, HUD and movie paths must not be unified
    # into a global x-compression or an unguarded DrawPrimitive rewrite.
    draw=CORE.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert draw.index("maybe_correct_adaptive_hud(") < draw.index(
        "maybe_correct_native_pause43(")
    assert "if(v==verts && count==4)" in draw
    assert "if(v==verts) v=maybe_correct_horplus(" in draw
    assert "original_draw_primitive(self,type,fvf,(LPVOID)v,count,flags)" in draw
