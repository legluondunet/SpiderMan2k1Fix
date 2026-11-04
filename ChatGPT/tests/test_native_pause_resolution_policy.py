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
    assert "fabsf(z-0.00122f)<0.00010f" in block
    assert "fabsf(rhw-11.176f)<0.25f" in block
    assert "rva!=0x00102C72UL" in block
    assert "type!=D3DPT_TRIANGLEFAN" in block
    assert "count!=4" in block
    assert "if(SUCCEEDED(IDirect3DDevice7_GetTexture(" in block
    assert "native_pause43_banner_flip,flip" in block
    assert "copy[i].x=mapped" in block
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
