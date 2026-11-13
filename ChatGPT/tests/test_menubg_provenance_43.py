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
    v[1].x=(float)w*0.70f;v[2].x=v[1].x;
    assert(!SpiderMenubg43_Transform(v,out,4,6,0x144u,w,h));
    v[0].x=0.0f;v[3].x=0.0f;
    v[1].x=INFINITY;
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
    /* Second native original draw path: authenticated SAME-material
     * fullscreen background, not a generic 512x512 texture exception. */
    {
      SpiderMenuVertex v[4]={
        {0.0f,0.0f,0.99976f,0.0229f,0xFFFFFFFFu,0.0f,0.0f},
        {1920.0f,0.0f,0.99976f,0.0229f,0xFFFFFFFFu,1.0f,0.0f},
        {1920.0f,1080.0f,0.99976f,0.0229f,0xFFFFFFFFu,1.0f,1.0f},
        {0.0f,1080.0f,0.99976f,0.0229f,0xFFFFFFFFu,0.0f,1.0f}
      };
      assert(SpiderMenubg43_IsCompanionBackground(v,4u,6u,0x144u,
        SPIDER_MENUBG43_COMPANION_CALLER,1920u,1080u,
        0x12345678u,0x12345678u,300u,299u));
      assert(!SpiderMenubg43_IsCompanionBackground(v,4u,6u,0x144u,
        SPIDER_MENUBG43_COMPANION_CALLER,1920u,1080u,
        0x12345678u,0x87654321u,300u,299u));
      assert(!SpiderMenubg43_IsCompanionBackground(v,4u,6u,0x144u,
        SPIDER_MENUBG43_COMPANION_CALLER,1920u,1080u,
        0x12345678u,0x12345678u,303u,299u));
      assert(!SpiderMenubg43_IsCompanionBackground(v,4u,6u,0x144u,
        SPIDER_MENUBG43_COMPANION_CALLER,1920u,1080u,
        0x12345678u,0x12345678u,298u,299u));
      assert(!SpiderMenubg43_IsCompanionBackground(v,4u,6u,0x144u,
        SPIDER_MENUBG43_TITLE_CALLER,1920u,1080u,
        0x12345678u,0x12345678u,300u,299u));
      v[0].z=0.00122f;
      assert(!SpiderMenubg43_IsCompanionBackground(v,4u,6u,0x144u,
        SPIDER_MENUBG43_COMPANION_CALLER,1920u,1080u,
        0x12345678u,0x12345678u,300u,299u));
    }
    /* The SOURCE-identified top title is a family of individual glyph
     * quads with original 32/64x32 textures, not the menubg fullscreen. */
    {
      SpiderMenuVertex v[4]={
        {170.0f,99.0f,0.99927f,0.0229f,0xFFFFFFFFu,0.0f,0.0f},
        {230.0f,99.0f,0.99927f,0.0229f,0xFFFFFFFFu,1.0f,0.0f},
        {230.0f,166.0f,0.99927f,0.0229f,0xFFFFFFFFu,1.0f,1.0f},
        {170.0f,166.0f,0.99927f,0.0229f,0xFFFFFFFFu,0.0f,1.0f}
      },out[4];
      assert(SpiderMenubg43_TitleGlyphTransform(v,out,4u,6u,0x144u,
        SPIDER_MENUBG43_TITLE_CALLER,1920u,1080u,32u,32u));
      assert(near(out[0].x,367.5f));
      assert(near(out[1].x,412.5f));
      for(unsigned i=0u;i<4u;i++){
        assert(out[i].y==v[i].y && out[i].z==v[i].z &&
          out[i].rhw==v[i].rhw && out[i].diffuse==v[i].diffuse &&
          out[i].u==v[i].u && out[i].v==v[i].v);
      }
      assert(!SpiderMenubg43_TitleGlyphTransform(v,out,4u,6u,0x144u,
        SPIDER_MENUBG43_TITLE_CALLER,1920u,1080u,512u,512u));
      assert(!SpiderMenubg43_TitleGlyphTransform(v,out,4u,6u,0x144u,
        SPIDER_MENUBG43_COMPANION_CALLER,1920u,1080u,32u,32u));
      v[0].z=0.00122f;
      assert(!SpiderMenubg43_TitleGlyphTransform(v,out,4u,6u,0x144u,
        SPIDER_MENUBG43_TITLE_CALLER,1920u,1080u,32u,32u));
    }
    verify(1920u,1080u);
    verify(1280u,800u);
    verify(1200u,750u);
    verify(1281u,800u);
    /* The same source must remain centred until the PAUSED Flip captures it,
     * but ONLY during the brief external focus-loss transition. */
    assert(SpiderMenubg43_FocusTailAllowed(100u,84u,0x1234u,0x1234u,1));
    assert(SpiderMenubg43_FocusTailAllowed(101u,101u,0x1234u,0x1234u,1));
    assert(!SpiderMenubg43_FocusTailAllowed(101u,84u,0x1234u,0x1234u,1));
    assert(!SpiderMenubg43_FocusTailAllowed(100u,84u,0x1234u,0x1234u,0));
    assert(!SpiderMenubg43_FocusTailAllowed(100u,84u,0x1235u,0x1234u,1));
    assert(!SpiderMenubg43_FocusTailAllowed(100u,84u,0u,0u,1));
    assert(!SpiderMenubg43_FocusTailAllowed(84u,100u,0x1234u,0x1234u,1));
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


def test_x86_rel32_displacements_wrap_modulo_2_32():
    """A native 32-bit CALL can target a DLL above the signed-2GB boundary.

    Do not regress to a signed-difference range check: the existing F9
    hook already relies on the same modulo-32 arithmetic.
    """
    assert "DWORD rel1,rel2;" in SOURCE
    assert "rel1=(DWORD)((uintptr_t)&spider_menubg43_source_wrapper -" in SOURCE
    assert "rel2=(DWORD)((uintptr_t)&spider_menubg43_source_wrapper -" in SOURCE
    assert "d1<INT32_MIN" not in SOURCE
    assert "MENUBG43-PREFLIGHT: PASS original CALLs" in SOURCE
    for callsite in (0x0049FC98, 0x0048DACC):
        for target in (0x00600000, 0x7FFFFFFF, 0xF1234567):
            relative = (target - (callsite + 5)) & 0xFFFFFFFF
            assert ((callsite + 5 + relative) & 0xFFFFFFFF) == target


def test_preflight_reports_exact_guard_without_relaxing_signatures():
    assert "menubg43_validate_bytes(" in SOURCE
    assert "FAIL SAVE CALL 0x0049FC98" in SOURCE
    assert "FAIL COMMON CALL 0x0048DACC" in SOURCE
    assert "QUEUE BUILDER 0x0050768F" in SOURCE
    assert "QUEUE FLUSH 0x00502C63" in SOURCE
    assert "actual=%s expected=%s" in SOURCE
    assert "FAIL post-write CALL verification" in SOURCE
    assert "VirtualQuery(site,&mem,sizeof(mem))" in SOURCE
    assert "memcmp(site,sig,n)==0" in SOURCE



def test_queue_flush_matches_observed_original_push_sequence():
    """Wine's SHA-pinned retail binary has PUSH EDX before PUSH 0x144.

    The earlier guessed 10-byte signature incorrectly omitted byte 0x52,
    so no menubg source hooks were installed in the user's SAVE GAME run.
    """
    assert (
        "static const BYTE draw_signature[11]={"
        "0x8D,0x57,0x10,0x52,0x68,0x44,0x01,0x00,0x00,0x6A,0x06};"
    ) in SOURCE
    assert "draw_signature,sizeof(draw_signature)" in SOURCE
    observed = bytes.fromhex("8D 57 10 52 68 44 01 00 00 6A 06")
    expected = bytes([0x8D, 0x57, 0x10, 0x52, 0x68, 0x44, 0x01,
                      0x00, 0x00, 0x6A, 0x06])
    assert observed == expected



def test_real_queue_flush_signature_fits_preflight_and_error_dump_buffers():
    """Regression: an 11-byte native signature used to be rejected as unreadable.

    The former n>10 check triggered before VirtualQuery. Byte dump buffers
    were also only 32 bytes, too small for 11 formatted hex values.
    """
    assert "enum { MAX_SIGNATURE_BYTES=16 };" in SOURCE
    assert "n>MAX_SIGNATURE_BYTES" in SOURCE
    assert "n>10u" not in SOURCE
    assert "actual[3*MAX_SIGNATURE_BYTES+1]" in SOURCE
    assert "expected[3*MAX_SIGNATURE_BYTES+1]" in SOURCE
    assert "invalid signature length=%lu" in SOURCE
    assert "memory not readable" in SOURCE
    assert "const BYTE draw_signature[11]" in SOURCE
    assert 11 <= 16
    for n in (5, 6, 10, 11, 16):
        # Every byte is logged as 'HH ' and a final NUL terminator.
        assert 3 * n + 1 <= 3 * 16 + 1



def test_focus_loss_background_retention_is_source_verified_and_bounded():
    """A lost native shell during Alt-Tab must not stretch the frozen frame."""
    hdr=(ROOT / "menubg_provenance_43.h").read_text(encoding="utf-8")
    assert "SPIDER_MENUBG43_FOCUS_TAIL_FLIPS 16u" in hdr
    assert "focus_lost && verified_texture!=0u" in hdr
    assert "texture==verified_texture" in hdr
    assert "frame-last_shell_frame<=SPIDER_MENUBG43_FOCUS_TAIL_FLIPS" in hdr
    assert "if(applied && shell)" in SOURCE
    assert "if(!shell && InterlockedCompareExchange(&menubg43_last_shell_frame" in SOURCE
    assert "pause_x11_bridge_state()==2" in SOURCE
    assert "MENUBG43-FOCUS-TAIL: MAPPED" in SOURCE
    assert "eligible=(shell || focus_tail)" in SOURCE
    # All earlier original-image / render-geometry guards must survive.
    assert "SpiderMenubg43_RecordIndex((uintptr_t)source,&i)" in SOURCE
    assert "texture!=menubg43_texture_tag[i]" in SOURCE
    assert "SpiderHudWidescreen_Installed()" in SOURCE



def test_name_entry_and_title_diagnostics_cannot_modify_rendered_geometry():
    """Probe native menu callsite/material before attempting a font fix.

    A title offset based on screen coordinates alone is unsafe; INPUT NAME
    may leave the verified SAVE GAME shell and use another draw path.
    """
    start=SOURCE.index("static void menubg43_probe_unmapped_art(")
    end=SOURCE.index("/* The observed menu draw path",start)
    probe=SOURCE[start:end]
    assert "if(!debug_log_enabled || !menubg43_trial_enabled" in probe
    assert "menubg43_native_shell_state()" in probe
    assert "frame-last<=2400" in probe
    assert "GetForegroundWindow()==game_hwnd" in probe
    assert "MENUBG43-ART-PROBE: %s api=%s" in probe
    assert "TOP-TITLE" in probe and "WIDE-BACKGROUND" in probe
    assert "DrawIndexedPrimitive" in SOURCE
    assert 'menubg43_probe_unmapped_art(self,"DrawPrimitive"' in SOURCE
    assert 'menubg43_probe_unmapped_art(self,"DrawIndexedPrimitive"' in SOURCE
    assert "IDirect3DDevice7_GetTexture(" in probe
    assert "geom_rva(ret,&caller_rva)" in probe
    for forbidden in ("VirtualProtect(", "memcpy(", "SetViewport(",
                      "SetRenderState(", "DrawPrimitive(", "DrawIndexedPrimitive("):
        assert forbidden not in probe



def test_new_menu_background_and_top_title_use_only_authenticated_provenance():
    hdr=(ROOT / "menubg_provenance_43.h").read_text(encoding="utf-8")
    assert "SPIDER_MENUBG43_COMPANION_CALLER 0x00103389u" in hdr
    assert "material!=verified_material" in hdr
    assert "frame-verified_frame>2u" in hdr
    assert "SPIDER_MENUBG43_TITLE_CALLER 0x00102C72u" in hdr
    assert "SpiderMenubg43_TitleGlyphTransform" in hdr
    assert "if(!menubg43_trial_enabled || !menubg43_call_hooks_installed" in SOURCE
    assert "if(!SpiderMenubg43_Transform((const SpiderMenuVertex *)source,copy" in SOURCE
    assert "MENUBG43-COMPANION: MAPPED" in SOURCE
    assert "MENUBG43-COMPANION: REJECT" in SOURCE
    assert "MENUBG43-TITLE: MAPPED" in SOURCE
    assert "if(v==verts) v=maybe_correct_menubg43_title(" in SOURCE
    draw=SOURCE.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert draw.index("maybe_correct_menubg43_source(") < draw.index(
        "maybe_correct_menubg43_companion(") < draw.index(
        "maybe_correct_menubg43_title(") < draw.index(
        "maybe_correct_horplus(")
    # There is no content override, glyph-specific name/position hack, or
    # native shell bypass. Only the X of out-of-place vertices is changed.
    companion=SOURCE.split("static const void *maybe_correct_menubg43_companion(",1)[1].split(
        "static const void *maybe_correct_menubg43_title(",1)[0]
    assert "menubg43_native_shell_state()" in companion
    assert "material,(uint32_t)known" in companion
    title=SOURCE.split("static const void *maybe_correct_menubg43_title(",1)[1].split(
        "/* READ-ONLY reconnaissance",1)[0]
    assert "menubg43_native_shell_state()" in title
    assert "SpiderMenubg43_RecordIndex(" in title
    assert "IDirect3DDevice7_GetTexture(" in title
    for fn in (companion,title):
        assert "VirtualProtect(" not in fn
        assert "SetViewport(" not in fn
        assert "SetRenderState(" not in fn
