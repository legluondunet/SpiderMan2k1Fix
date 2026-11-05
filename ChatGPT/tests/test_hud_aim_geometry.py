"""Measured 4:3 reticle aspect math, kept separate from live D3D7 matching."""
from pathlib import Path
import shutil
import subprocess
import textwrap

import pytest

ROOT=Path(__file__).resolve().parents[2]


def test_reticles_stay_circular_without_relocating_or_affecting_43(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        pytest.skip("C compiler unavailable")
    file=tmp_path/"aim_aspect.c"
    exe=tmp_path/"aim_aspect"
    file.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <math.h>
        #include "hud_aim_geometry.h"
        static float absdiff(float a,float b){return fabsf(a-b);}
        static void check(float width,float height,
                          unsigned screen_w,unsigned screen_h){
            float center=960.0f;
            float x0=center-width*0.5f,x1=center+width*0.5f;
            float a=SpiderHudAim_MapX(x0,center,screen_w,screen_h);
            float b=SpiderHudAim_MapX(x1,center,screen_w,screen_h);
            float scale=SpiderHudAim_AspectFactor(screen_w,screen_h);
            assert(absdiff((a+b)*0.5f,center)<0.0002f);
            assert(absdiff((b-a),width*scale)<0.0002f);
            assert(absdiff((b-a)/height,1.0f)<0.03f);
        }
        int main(void){
            /* Original PC 1920x1080 F9 screenshot samples. */
            check(94.0f,70.0f,1920,1080); /* green */
            check(153.0f,113.0f,1920,1080); /* red */
            /* Other widescreen outputs retain the original 4:3
             * content, regardless of monitor width. */
            assert(absdiff(SpiderHudAim_AspectFactor(1920,1080),0.75f)<1e-6f);
            assert(absdiff(SpiderHudAim_AspectFactor(1920,1200),5.0f/6.0f)<1e-6f);
            assert(absdiff(SpiderHudAim_AspectFactor(2560,1440),0.75f)<1e-6f);
            assert(SpiderHudAim_AspectFactor(1440,1080)==1.0f);
            assert(SpiderHudAim_AspectFactor(640,480)==1.0f);
            assert(SpiderHudAim_AspectFactor(0,0)==1.0f);
            assert(SpiderHudAim_AspectFactor(1080,1920)==1.0f);
            assert(SpiderHudAim_MapX(390.0f,640.0f,1440,1080)==390.0f);
            assert(absdiff(SpiderHudAim_MapX(490,640,1920,1080),527.5f)<0.001f);
            return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c11","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(file),"-lm","-o",str(exe)],
                   check=True,capture_output=True,text=True)
    subprocess.run([str(exe)],check=True,capture_output=True,text=True)


def test_aspect_policy_is_not_globally_installed_as_an_unverified_hud_hook():
    # Do not mutate unrelated D3D7 quads until the real crosshair
    # callback, texture and depth layer are measured from gameplay.
    core=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    hdr=(ROOT/"hud_aim_geometry.h").read_text(encoding="utf-8")
    assert "SpiderHudAim_MapX(" in hdr
    assert "SpiderHudAim_MapX(" not in core
