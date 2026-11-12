"""Source-authenticated original menubg queue geometry trial.

No general HUD, SDL3, 3D, pause, or save input behavior is modified.
A new experimental path must be explicit opt-in and fail closed if original
callsite signatures or original source-record identity are invalid.
"""
from pathlib import Path
import shutil
import subprocess
import pytest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")

@pytest.mark.skipif(not any(shutil.which(t) for t in ("cc", "gcc", "clang")),
                    reason="native compiler not present")
def test_source_record_index_and_aspect_geometry(tmp_path):
    src=tmp_path/"menubg_source_test.c"
    exe=tmp_path/"menubg_source_test"
    src.write_text(r'''
#include <assert.h>
#include <math.h>
#include <string.h>
#include "menubg_provenance_43.h"

static int near(float a,float b) { return fabsf(a-b)<0.06f; }
static void verify(unsigned w,unsigned h) {
    SpiderMenuVertex v[4] = {
      {-0.5f,0.0f,0.7f,12.0f,0x7f3232a0u,0.0f,0.0f},
      {(float)w+0.5f,0.0f,0.7f,12.0f,0x7f3232a0u,1.0f,0.0f},
      {(float)w+0.5f,(float)h,0.7f,12.0f,0x7f3232a0u,1.0f,1.0f},
      {-0.5f,(float)h,0.7f,12.0f,0x7f3232a0u,0.0f,1.0f}
    };
    SpiderMenuVertex before[4],out[4];
    float scalar=4.0f*(float)h/(3.0f*(float)w);
    memcpy(before,v,sizeof v);
    assert(SpiderMenubg43_Transform(v,out,4,6,0x144u,w,h));
    for(unsigned i=0;i<4;i++) {
      assert(near(out[i].x,0.5f*w+(v[i].x-0.5f*w)*scalar));
      assert(out[i].y==v[i].y && out[i].z==v[i].z &&
             out[i].rhw==v[i].rhw && out[i].diffuse==v[i].diffuse &&
             out[i].u==v[i].u && out[i].v==v[i].v);
    }
    assert(!memcmp(before,v,sizeof v));
    assert(!SpiderMenubg43_Transform(v,out,4,4,0x144u,w,h));
    assert(!SpiderMenubg43_Transform(v,out,4,6,0x140u,w,h));
    assert(!SpiderMenubg43_Transform(v,v,4,6,0x144u,w,h));
    assert(!SpiderMenubg43_Transform(v,out,3,6,0x144u,w,h));
    v[0].z=0.5f;
    assert(!SpiderMenubg43_Transform(v,out,4,6,0x144u,w,h));
    v[0].z=0.7f;
    v[0].x=(float)w*0.31f;v[3].x=v[0].x;
    assert(!SpiderMenubg43_Transform(v,out,4,6,0x144u,w,h));
    v[0].x=0.0f;v[3].x=0.0f;
    v[1].x=(float)w/0.0f;
    assert(!SpiderMenubg43_Transform(v,out,4,6,0x144u,w,h));
}
int main(void) {
    uint32_t idx=999;
    assert(SpiderMenubg43_RecordIndex((uintptr_t)SPIDER_MENUBG43_RECORD_BASE+16u,&idx));
    assert(idx==0u);
    assert(SpiderMenubg43_RecordIndex(
      (uintptr_t)SPIDER_MENUBG43_RECORD_BASE+16u+240u*12u,&idx));
    assert(idx==12u);
    assert(!SpiderMenubg43_RecordIndex((uintptr_t)SPIDER_MENUBG43_RECORD_BASE+12u,&idx));
    assert(!SpiderMenubg43_RecordIndex(
      (uintptr_t)SPIDER_MENUBG43_RECORD_BASE+16u+240u*SPIDER_MENUBG43_RECORD_CAP,&idx));
    assert(!SpiderMenubg43_RecordIndex(
      (uintptr_t)SPIDER_MENUBG43_RECORD_BASE+17u,&idx));
    verify(1920u,1080u);
    verify(1280u,800u);
    verify(1200u,750u);
    verify(1281u,800u);
    {
      SpiderMenuVertex src[4]={0},dst[4]={0};
      assert(!SpiderMenubg43_Transform(src,dst,4,6,0x144u,1440u,1080u));
    }
    return 0;
}
''',encoding="utf-8")
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    subprocess.run([cc,"-std=c11","-O2","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-lm","-o",str(exe)],
                   text=True,capture_output=True,check=True)
    subprocess.run([str(exe)],check=True)


def test_original_menubg_callsite_hooks_are_strictly_gated():
    assert "SPIDERMAN2K1FIX_MENUBG43_TRIAL" in SOURCE
    assert "MENUBG43: OFF by default (no game CALL patches)" in SOURCE
    assert "static const BYTE orig_save[5]={0xE8,0x83,0xA0,0x06,0x00};" in SOURCE
    assert "static const BYTE orig_common[5]={0xE8,0x4F,0xC2,0x07,0x00};" in SOURCE
    assert "0x0049FC98u" in SOURCE and "0x0048DACCu" in SOURCE
    assert "0x0050768Fu" in SOURCE and "0x00502C63u" in SOURCE
    assert "0x00509D20u" in SOURCE
    assert "SPIDER_MENUBG43_QUEUE_COUNT" in SOURCE
    assert "SPIDER_MENUBG43_RECORD_STRIDE" in SOURCE
    assert "SPIDER_MENUBG43_RECORD_BASE" in SOURCE
    assert "sizeof(void*)!=4" in SOURCE
    assert "GetModuleHandleA(NULL)!=0x00400000u" in SOURCE
    assert "menubg43_validate_callsite(save,orig_save)" in SOURCE
    assert "menubg43_validate_callsite(common,orig_common)" in SOURCE
    assert "menubg43_call_hooks_installed=" in SOURCE
    assert "menubg43_texture_tag[i]=texture;" in SOURCE
    assert "menubg43_frame_tag[i]" in SOURCE
    assert "MENUBG43-SOURCE: CALL" in SOURCE
    assert "MENUBG43-DRAW: %s" in SOURCE


def test_tagged_draw_has_precedence_guard_and_never_writes_game_vertices():
    start=SOURCE.index("static const void *maybe_correct_menubg43_source(")
    stop=SOURCE.index("static int menubg43_validate_callsite(",start)
    tagged=SOURCE[start:stop]
    assert "SpiderMenubg43_RecordIndex(" in tagged
    assert "texture!=menubg43_texture_tag[i]" in tagged
    assert "v[0].x!=menubg43_x_tag[i]" in tagged
    assert "v[0].y!=menubg43_y_tag[i]" in tagged
    assert "menubg43_native_shell_state()" in tagged
    assert "mapped_by_earlier_stage" in tagged
    assert "if(eligible && !mapped_by_earlier_stage)" in tagged
    assert "SpiderMenubg43_Transform(v,copy" in tagged
    for forbidden in ("SendInput(", "VirtualProtect(", "SetRenderState(",
                      "DrawPrimitive(", "SetViewport(", "memcpy(v,"):
        assert forbidden not in tagged
    draw=SOURCE.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert draw.index("maybe_correct_unified_ui_canvas(") < draw.index(
        "maybe_correct_menubg43_source(") < draw.index(
        "maybe_correct_horplus(")
