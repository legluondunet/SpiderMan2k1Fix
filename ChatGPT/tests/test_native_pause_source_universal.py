"""Universal native ESC/Pause source-space policy, not a per-level repair.

Tests compile the actual renderer policy as portable C, then assert the
runtime bridge only observes SHA-gated native ownership and remaps 4:3
physical-source UI, not arbitrary gameplay geometry or controller actions.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

import pytest

ROOT = Path(__file__).resolve().parents[2]
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
BRIDGE = (ROOT / "native_pause_source_bridge.inc").read_text(encoding="utf-8")


def test_pause_source_space_invariant_across_levels_resolutions(tmp_path):
    cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        pytest.skip("portable C compiler unavailable")
    source = tmp_path / "pause_space.c"
    binary = tmp_path / "pause_space"
    source.write_text(textwrap.dedent(r"""
    #include <assert.h>
    #include <math.h>
    #include <stdint.h>
    #include "native_pause_source_policy.h"
    #define NONE SPIDER_PAUSE_SOURCE_NONE
    #define LOG SPIDER_PAUSE_SOURCE_LOGICAL
    #define PHY SPIDER_PAUSE_SOURCE_PHYSICAL
    static int near(float a,float b) {return fabsf(a-b)<.001f;}
    static void test_view(unsigned logical,unsigned physical,
                          unsigned height,float left,float right) {
        float h=(float)height;
        /* Source 3D Pause art may already use the physical screen width,
         * independently of its camera-dependent RHW and current level. */
        assert(SpiderPauseSource_Space(0,(float)physical,.37f*h,.625f*h,
               logical,height,physical,height)==PHY);
        assert(SpiderPauseSource_Space(0,(float)logical,.37f*h,.625f*h,
               logical,height,physical,height)==LOG);
        assert(near(SpiderPauseSource_MapPhysicalX(0,physical,height),left));
        assert(near(SpiderPauseSource_MapPhysicalX((float)physical,
                                                 physical,height),right));
        assert(near(SpiderPauseSource_MapPhysicalX((float)physical/2.0f,
                                                 physical,height),
                    (float)physical/2.0f));
        assert(!SpiderPauseSource_Space(.20f*physical,.80f*physical,
                .37f*h,.625f*h,logical,height,physical,height));
        assert(!SpiderPauseSource_Space(0,(float)physical,
                .02f*h,.25f*h,logical,height,physical,height));
    }
    int main(void) {
        test_view(1440,1920,1080,240,1680);
        test_view(1600,1920,1200,160,1760);
        test_view(1067,1280,800,106.666667f,1173.333333f);
        test_view(1920,2560,1440,320,2240);
        test_view(2560,3440,1920,440,3000);
        /* Genuine 4:3 physical output already uses correct geometry. */
        assert(SpiderPauseSource_Space(0,1440,400,680,
                        1440,1080,1440,1080)==NONE);
        assert(SpiderPauseSource_RibbonBounds(0,1440,400,680,1440,1080));
        /* 4:3 physical Pause remains valid without any X scaling. */
        assert(near(SpiderPauseSource_MapPhysicalX(0,1440,1080),0));
        assert(near(SpiderPauseSource_MapPhysicalX(1440,1440,1080),1440));
        /* An invalid/NaN source vertex must never reach a D3D7 draw. */
        assert(SpiderPauseSource_ValidVertex(0,400,.00122f,11.176f,1440,1080));
        assert(SpiderPauseSource_ValidVertex(1600,750,.00122f,8.9615f,1920,1200));
        assert(!SpiderPauseSource_ValidVertex(NAN,500,.00122f,11.176f,1920,1080));
        assert(!SpiderPauseSource_ValidVertex(100,NAN,.00122f,11.176f,1920,1080));
        assert(!SpiderPauseSource_ValidVertex(100,500,NAN,11.176f,1920,1080));
        assert(!SpiderPauseSource_ValidVertex(100,500,.00122f,NAN,1920,1080));
        assert(!SpiderPauseSource_ValidVertex(INFINITY,500,.00122f,11.176f,1920,1080));
        assert(!SpiderPauseSource_ValidVertex(100,-200,.00122f,11.176f,1920,1080));
        assert(!SpiderPauseSource_ValidVertex(100,500,.00122f,11.176f,20000,1080));
        /* No magic level/RHW list; generic original purple material gate. */
        assert(SpiderPauseSource_RibbonMaterial(.00122f,6.8041f,0x806050d6u));
        assert(SpiderPauseSource_RibbonMaterial(.00122f,17.413f,0x806050d6u));
        assert(!SpiderPauseSource_RibbonMaterial(.00122f,17.413f,0x80d65060u));
        assert(!SpiderPauseSource_RibbonMaterial(.22f,17.413f,0x806050d6u));
        assert(!SpiderPauseSource_RibbonMaterial(.00122f,200.0f,0x806050d6u));
        assert(SpiderPauseSource_Material(.00122f,17.413f,
               0xFF000000u,8,8,1)); /* 8x8 frame border */
        assert(SpiderPauseSource_Material(.00098f,17.413f,
               0xFFFFFFFFu,32,32,1)); /* native text */
        assert(SpiderPauseSource_Material(.00146f,17.413f,
               0x80000000u,0,0,0)); /* original translucent motif */
        assert(!SpiderPauseSource_Material(.02f,17.413f,
               0xFFFFFFFFu,8,8,1)); /* ordinary HUD cannot pass */
        assert(!SpiderPauseSource_Material(.00122f,17.413f,
               0xFFFFFFFFu,256,256,1)); /* unknown texture */
        assert(!SpiderPauseSource_Material(.00122f,17.413f,
               0xFFFFFFFFu,0,0,0)); /* wrong untextured material */
        return 0;
    }
    """), encoding="utf-8")
    subprocess.run([cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT), str(source), "-lm", "-o", str(binary)],
                   check=True, capture_output=True, text=True)
    subprocess.run([str(binary)], check=True)


def test_native_pause_observer_is_readonly_shapegated_and_level_independent():
    assert '#include "native_pause_source_policy.h"' in CORE
    assert '#include "native_pause_source_bridge.inc"' in CORE
    assert "rva!=0x00102C72UL" in BRIDGE
    assert BRIDGE.count("IDirect3DDevice7_GetTexture(") == 2
    assert "SpiderPauseSource_Space(" in BRIDGE
    assert "SpiderPauseSource_RibbonMaterial(" in BRIDGE
    assert "SpiderNativePause43_RibbonLayer(" in BRIDGE
    assert "native_pause_source_visible_near_flip(" in BRIDGE
    assert "native_pause_source_reset(" in BRIDGE
    assert "native_pause_source_reset(); /* no stale Pause owner" in CORE
    assert "native_pause_source_reset();" in CORE
    assert "native_pause_source_visible_near_flip(count)" in CORE
    assert "native_pause43_visible_near_flip(count) ||" in CORE
    assert "gameplay_4x3_active" in BRIDGE
    assert "menu_upscale_active" in BRIDGE
    assert "D3DPT_TRIANGLEFAN" in BRIDGE
    assert "fvf!=0x144UL" in BRIDGE
    assert "original UI physical->4:3 remap" in BRIDGE
    assert "SpiderPauseSource_MapPhysicalX(" in BRIDGE
    assert "memcpy(copy,v,4u*sizeof(*copy))" in BRIDGE
    assert "native_pause_source_remap_original_4x3(" in CORE
    assert "if(native_pause_source_remap_original_4x3(" in CORE
    assert "else\n      v=scale_menu_vertices(" in CORE
    assert BRIDGE.count("GetSurfaceDesc(")==1
    assert "texture))return 0;" not in BRIDGE  # must release COM texture first
    for forbidden in ("SendInput(", "PostMessage(", "WriteProcessMemory(",
                      "VirtualProtect(", "IDirectInputDevice8_"):
        assert forbidden not in BRIDGE
    assert "GameFMV_Active" in CORE
    assert "SpiderSDL3Universal_OnFlip(game_hwnd," in CORE


def test_true_native_43_pause_detected_without_unnecessary_remap():
    """Detection and scaling have separate proof requirements.

    A 1440x1080 monitor must recognize its native Pause without creating
    artificial side margins, especially when the optional widescreen HUD
    hook is unavailable. Failed material authentication still leaves the
    original renderer untouched.
    """
    observe = BRIDGE.split("static void native_pause_source_observe_ribbon(", 1)[1].split(
        "static int native_pause_source_remap_original_4x3(", 1)[0]
    visible = BRIDGE.split("static int native_pause_source_visible_near_flip(", 1)[1].split(
        "static void native_pause_source_reset(", 1)[0]
    remap = BRIDGE.split("static int native_pause_source_remap_original_4x3(", 1)[1]
    assert "(uint64_t)desktop_w*3u==(uint64_t)desktop_h*4u" in observe
    assert "SpiderPauseSource_RibbonBounds(left,right,top,bottom" in observe
    assert "native-4:3" in observe
    assert "native_pause_source_space" not in visible
    assert "SPIDER_PAUSE_SOURCE_PHYSICAL" in remap
    assert "native_pause_source_visible_near_flip(flip)" in remap
    assert "SpiderPauseSource_ValidVertex" in observe
    assert "SpiderPauseSource_ValidVertex" in remap
    assert "if(bound || !SpiderNativePause43_RibbonLayer" in remap
    assert "SpiderPauseSource_RibbonBounds(l,r,t,bt" in remap
    assert "copy[i].x=SpiderPauseSource_MapPhysicalX" in remap


def test_detected_pause_reaches_the_real_sdl3_gameplay_input_owner():
    """A detected native Pause must stop 3D A/Space and route to A/Enter.

    The earlier prototype updated only SpiderGamepadUI_OnFlip, which
    is NOT the final SDL3 keyboard injector. This catches that exact
    regression across all four consumers of native pause eligibility.
    """
    for consumer in (
        "SpiderGamepadUI_OnFlip(game_hwnd,",
        "SpiderAltLatch_OnFlip(game_hwnd,",
        "SpiderSDL3GameplayPlan_OnFlip(game_hwnd,",
        "SpiderSDL3Universal_OnFlip(game_hwnd,",
    ):
        assert CORE.count(consumer) == 1, consumer
        args = CORE.split(consumer, 1)[1].split("));", 1)[0]
        assert "native_pause43_visible_near_flip(count)" in args, consumer
        assert "native_pause_source_visible_near_flip(count)" in args, consumer
        assert "0x00682950u" in args, consumer
    assert CORE.count("native_pause_source_visible_near_flip(count)") == 4
    universal = CORE.split("SpiderSDL3Universal_OnFlip(game_hwnd,", 1)[1].split(
        "));", 1)[0]
    assert universal.index("native_pause43_visible_near_flip(count)") < (
        universal.index("native_pause_source_visible_near_flip(count)"))


def test_native_pause_does_not_inject_second_transform_in_auto_or_known_logical_mode():
    # Precedence: source observation before per-primitive mapping, then
    # old original menu and gameplay render pipeline remains fallback.
    draw = CORE.split("static HRESULT WINAPI hooked_draw_primitive(", 1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(", 1)[0]
    assert draw.index("native_pause_source_observe_ribbon(") < (
        draw.index("native_pause_source_remap_original_4x3("))
    assert draw.index("native_pause_source_remap_original_4x3(") < (
        draw.index("scale_menu_vertices("))
    assert "SPIDER_PAUSE_SOURCE_PHYSICAL" in BRIDGE
    assert "if(InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=1" in BRIDGE
    assert "source-space" in (ROOT / "native_pause_source_policy.h").read_text()
    assert "native_pause43_visible_near_flip(" in CORE
