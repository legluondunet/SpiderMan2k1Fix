"""Read-only HUD research samples; no guessed reticle/pause transformations."""
from pathlib import Path
import shutil
import subprocess
import textwrap

import pytest

ROOT = Path(__file__).resolve().parents[2]
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
CANDIDATES = (ROOT / "hud_visual_candidates.h").read_text(encoding="utf-8")


def test_candidate_classifier_matches_reference_screen_geometry(tmp_path):
    cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        pytest.skip("C compiler unavailable")
    program = tmp_path / "hud_visual_sample.c"
    binary = tmp_path / "hud_visual_sample"
    program.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <string.h>
        #include "hud_visual_candidates.h"
        static void q(SpiderMenuVertex *v,
                      float l,float t,float r,float b) {
            unsigned i;
            memset(v,0,4u*sizeof(*v));
            for(i=0;i<4u;i++) {
                v[i].x=(i==1 || i==2)?r:l;
                v[i].y=(i>=2)?b:t;
                v[i].z=0.00015f;
                v[i].rhw=20.0f;
                v[i].diffuse=0xFFFFFFFFu;
            }
        }
        int main(void) {
            SpiderMenuVertex v[4], before[4];
            q(v,590,210,738,316); /* green 3D aim ring */
            memcpy(before,v,sizeof(v));
            assert(SpiderHudVisual_Classify(v,4,6,0x144,1920,1080)
                   ==SPIDER_HUD_VISUAL_AIM);
            assert(memcmp(v,before,sizeof(v))==0);
            q(v,563,550,735,685); /* red 3D aim ring */
            assert(SpiderHudVisual_Classify(v,4,6,0x144,1920,1080)
                   ==SPIDER_HUD_VISUAL_AIM);
            q(v,0,400,1920,675); /* native level-2 pause ribbon */
            assert(SpiderHudVisual_Classify(v,4,6,0x144,1920,1080)
                   ==SPIDER_HUD_VISUAL_PAUSE_RIBBON);
            assert(SpiderHudVisual_Classify(v,4,6,0x144,1440,1080)
                   ==SPIDER_HUD_VISUAL_NONE);
            assert(SpiderHudVisual_Classify(v,4,6,0x142,1920,1080)
                   ==SPIDER_HUD_VISUAL_NONE);
            /* These log-observed samples are the native ESC
             * translucent motif, not aiming rings. */
            q(v,1245,904,1511.2f,1053);
            v[0].z=0.00146f;
            v[0].rhw=7.8952f;
            v[0].diffuse=0x80208686u;
            assert(SpiderHudVisual_Classify(v,4,6,0x144,1920,1080)
                   ==SPIDER_HUD_VISUAL_NONE);
            v[0].diffuse=0x80041010u;
            assert(SpiderHudVisual_Classify(v,4,6,0x144,1920,1080)
                   ==SPIDER_HUD_VISUAL_NONE);
            q(v,960,846,1038.8f,945);
            v[0].z=0.0f;
            v[0].rhw=27.6f;
            v[0].diffuse=0u;
            assert(SpiderHudVisual_Classify(v,4,6,0x144,1920,1080)
                   ==SPIDER_HUD_VISUAL_NONE);
            /* Exact false-positive sprite from the level-2 pause log:
             * this health/web HUD quad is NOT a red/green aim ring. */
            q(v,292.5f,94.5f,345.9f,162.0f);
            v[0].z=0.00024f;
            v[0].rhw=19.4920f;
            v[0].diffuse=0xFFFFFFFFu;
            assert(SpiderHudVisual_Classify(v,4,6,0x144,1920,1080)
                   ==SPIDER_HUD_VISUAL_NONE);
            q(v,20,125,270,310); /* player health, top-left */
            assert(SpiderHudVisual_Classify(v,4,6,0x144,1920,1080)
                   ==SPIDER_HUD_VISUAL_NONE);
            q(v,710,310,716,317); /* tiny particle */
            assert(SpiderHudVisual_Classify(v,4,6,0x144,1920,1080)
                   ==SPIDER_HUD_VISUAL_NONE);
            q(v,0,0,1920,1080); /* full-screen 3D/comic */
            assert(SpiderHudVisual_Classify(v,4,6,0x144,1920,1080)
                   ==SPIDER_HUD_VISUAL_NONE);
            return 0;
        }
    """), encoding="utf-8")
    subprocess.run([
        cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-I", str(ROOT), str(program), "-o", str(binary),
    ], check=True, capture_output=True, text=True)
    subprocess.run([str(binary)], check=True, capture_output=True, text=True)


def test_visual_trace_is_bounded_read_only_and_only_logs_uncorrected():
    assert '#include "hud_visual_candidates.h"' in CORE
    assert "if(!debug_log_enabled || !device || !v ||" in CORE
    assert "hit>(candidate==SPIDER_HUD_VISUAL_AIM?200:80)" in CORE
    assert "IDirect3DDevice7_GetTexture(" in CORE
    sampler = CORE.split("static void maybe_log_hud_visual_candidate(", 1)[1].split(
        "static void free_menu_vertices(", 1)[0]
    draw = CORE.split("static HRESULT WINAPI hooked_draw_primitive(", 1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(", 1)[0]
    assert "IDirectDrawSurface7_Release(tex);" in sampler
    assert "log_line(line);" in sampler
    assert "[READ-ONLY; CANDIDATE]" in sampler
    assert "SetTexture(" not in sampler
    assert "original_draw_primitive(" not in sampler
    assert "SendInput(" not in sampler
    assert "copy[i].x=" not in sampler
    assert draw.index("maybe_correct_adaptive_hud(") < draw.index(
        "maybe_correct_native_pause43(") < draw.index(
        "maybe_log_hud_visual_candidate(") < draw.index(
        "maybe_correct_horplus(")
    assert "if(v==verts && count==4)\n      maybe_log_hud_visual_candidate(" in draw
    assert "SpiderHudVisual_Classify" in CANDIDATES
    assert "return SPIDER_HUD_VISUAL_NONE;" in CANDIDATES
