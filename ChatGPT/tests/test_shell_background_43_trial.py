"""Opt-in source-family 4:3 shell background experiment.

The main gameplay/SDL3/Save Game logic must remain unchanged. This trial
must be a no-op unless both explicit environment and retail shell-state,
original D3D7 return-site and flat near-plane geometry checks succeed.
"""
from pathlib import Path
import shutil
import subprocess
import pytest

ROOT=Path(__file__).resolve().parents[2]
SOURCE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")


@pytest.mark.skipif(not any(shutil.which(x) for x in ("cc","gcc","clang")),
                    reason="C compiler required for geometry policy")
def test_original_shell_background_aspect_policy(tmp_path):
    src=tmp_path/"shell_background_test.c"
    exe=tmp_path/"shell_background_test"
    src.write_text(r'''
#include <assert.h>
#include <math.h>
#include <string.h>
#include "shell_background_43.h"
static int near(float a,float b) {return fabsf(a-b)<0.05f;}
static void check(unsigned w,unsigned h) {
    SpiderMenuVertex src[4]={
      {0.0f,0.0f,0.001f,11.176f,0x7F3232A0u,0.0f,0.0f},
      {(float)w,0.0f,0.001f,11.176f,0x7F3232A0u,1.0f,0.0f},
      {(float)w,(float)h,0.001f,11.176f,0x7F3232A0u,1.0f,1.0f},
      {0.0f,(float)h,0.001f,11.176f,0x7F3232A0u,0.0f,1.0f}};
    SpiderMenuVertex before[4],out[4];
    float expected_l=0.5f*((float)w-(4.0f/3.0f)*(float)h);
    float expected_r=(float)w-expected_l;
    memcpy(before,src,sizeof(src));
    memset(out,0x55,sizeof(out));
    assert(SpiderShellBackground43_Transform(src,out,6,0x144u,
      SPIDER_SHELL_BG43_CALLER_RVA,w,h,1));
    assert(near(out[0].x,expected_l)&&near(out[1].x,expected_r));
    assert(near(out[2].x,expected_r)&&near(out[3].x,expected_l));
    for(unsigned i=0;i<4;i++) {
        assert(out[i].y==src[i].y && out[i].z==src[i].z &&
               out[i].rhw==src[i].rhw &&
               out[i].diffuse==src[i].diffuse &&
               out[i].u==src[i].u && out[i].v==src[i].v);
    }
    assert(!memcmp(before,src,sizeof(src))); /* no game vertex mutation */
    assert(!SpiderShellBackground43_Transform(src,out,6,0x144u,
      SPIDER_SHELL_BG43_CALLER_RVA,w,h,0));
    assert(!SpiderShellBackground43_Transform(src,out,6,0x144u,
      0x00102C72u,w,h,1)); /* UI sprites already mapped by canvas */
    assert(!SpiderShellBackground43_Transform(src,out,4,0x144u,
      SPIDER_SHELL_BG43_CALLER_RVA,w,h,1));
    assert(!SpiderShellBackground43_Transform(src,out,6,0x140u,
      SPIDER_SHELL_BG43_CALLER_RVA,w,h,1));
    assert(!SpiderShellBackground43_Transform(src,src,6,0x144u,
      SPIDER_SHELL_BG43_CALLER_RVA,w,h,1));
    src[1].z=0.7f;
    assert(!SpiderShellBackground43_Transform(src,out,6,0x144u,
      SPIDER_SHELL_BG43_CALLER_RVA,w,h,1));
    src[1].z=0.001f;
    src[1].x=(float)w*0.25f;src[2].x=(float)w*0.25f;
    assert(!SpiderShellBackground43_Transform(src,out,6,0x144u,
      SPIDER_SHELL_BG43_CALLER_RVA,w,h,1)); /* small HUD world object */
}
int main(void) {
    check(1280u,800u);
    check(1920u,1080u);
    check(1200u,750u);
    check(1281u,800u);
    { SpiderMenuVertex src[4]={0},out[4]={0};
      assert(!SpiderShellBackground43_Transform(src,out,6,0x144u,
             SPIDER_SHELL_BG43_CALLER_RVA,1440u,1080u,1)); }
    return 0;
}
''',encoding="utf-8")
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    subprocess.run([cc,"-std=c11","-O2","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-lm","-o",str(exe)],
                   check=True,text=True,capture_output=True)
    subprocess.run([str(exe)],check=True)


def test_trial_is_opt_in_and_independent_of_sdl3_selection():
    assert "SPIDERMAN2K1FIX_SHELL_BG43_TRIAL" in SOURCE
    assert 'log_line("SHELL-BG43-TRIAL: OFF (default)")' in SOURCE
    assert "if(GetEnvironmentVariableA(" in SOURCE
    assert "shell_bg43_read_original_scalar" in SOURCE
    assert "request==3u && task==0x14u && input==1u" in SOURCE
    assert "!pause && menu && !dispatch && !transition" in SOURCE
    assert "SPIDER_SHELL_BG43_CALLER_RVA" in SOURCE
    assert "SpiderShellBackground43_Transform(" in SOURCE
    assert "if(v==verts && count==4)" in SOURCE
    assert "v=maybe_correct_shell_background43(" in SOURCE
    assert "if(v==verts) v=maybe_correct_horplus(" in SOURCE
    assert SOURCE.index("v=maybe_correct_shell_background43(") < (
           SOURCE.index("if(v==verts) v=maybe_correct_horplus("))
    # Do not introduce an input or save-time hook in this optics trial.
    start=SOURCE.index("static const void *maybe_correct_shell_background43(")
    end=SOURCE.index("/* Optional, bounded, READ-ONLY diagnostic",start)
    path=SOURCE[start:end]
    for forbidden in ("SendInput(", "VirtualProtect(", "SetRenderState(",
                      "DrawPrimitive(", "SetViewport("):
        assert forbidden not in path
