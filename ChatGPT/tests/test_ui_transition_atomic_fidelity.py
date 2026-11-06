"""V11: atomic reference 4:3 silhouette, then strictly uniform game zoom.

The original PC's full-HD source aperture is about 4:3 wider than
the Dreamcast reference when drawn across the 16:9 gameplay target.
Correct the WHOLE alpha texture once, not individual black primitives.
The native game still provides the only animation scale. A high-detail
texture is attempted first with automatic DDraw texture fallback.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT=Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module")
def correction(tmp_path_factory):
    cc=shutil.which("cc") or shutil.which("gcc")
    if not cc:pytest.skip("native C compiler missing")
    d=tmp_path_factory.mktemp("atomic-fidelity")
    src=d/"shape.c"
    so=d/"shape.so"
    src.write_text(
        '#include "ui_transition_atomic_mask.h"\n'
        'int correct(const uint32_t*a,uint32_t*b,'
        'unsigned tw,unsigned th,unsigned sw,unsigned sh){'
        'return SpiderAtomicMask_CorrectReference43(a,b,tw,th,sw,sh);'
        '}\n'
        'int zoom(float scale,float*a,float*b,float*c,float*d){'
        'return SpiderAtomicMask_ScreenUV(scale,a,b,c,d);'
        '}\n'
    )
    subprocess.run([
        cc,"-std=c11","-Wall","-Wextra","-Werror","-fPIC","-shared",
        "-I",str(ROOT),str(src),"-o",str(so),"-lm"
    ],check=True,capture_output=True,text=True)
    lib=ct.CDLL(str(so))
    lib.correct.argtypes=[ct.POINTER(ct.c_uint32)]*2+[ct.c_uint]*4
    lib.correct.restype=ct.c_int
    lib.zoom.argtypes=[ct.c_float]+[ct.POINTER(ct.c_float)]*4
    lib.zoom.restype=ct.c_int
    return lib


def input_alpha(w,h):
    # A complete original opaque outer black matte and a centered,
    # slightly horizontally stretched open 8-limbed spider aperture.
    values=[0xFF000000]*(w*h)
    for y in range(h):
        for x in range(w):
            # Hole's central body: 80px native width x 40px height.
            inside=(w//2-40 <= x < w//2+40 and h//2-20 <= y < h//2+20)
            # Native spider shape has eight attached opening legs.
            if h//2-20 <= y < h//2-11 or h//2+11 <= y < h//2+20:
                if w//2-55<=x<w//2-27 or w//2+27<=x<w//2+55:
                    inside=True
            if inside:values[y*w+x]=0x00000000
    return values


@pytest.mark.parametrize("sw,sh,factor",[
    (1920,1080,.75),
    (1920,1200,5/6),
    (2560,1080,.5625),
    (3440,1440,5760/10320),
    (1280,960,1.0),
])
def test_one_canonical_4by3_transform_scales_x_only_and_preserves_all_y(
    correction,sw,sh,factor
):
    tw,th=256,128
    source_vals=input_alpha(tw,th)
    source=(ct.c_uint32*(tw*th))(*source_vals)
    output=(ct.c_uint32*(tw*th))()
    assert correction.correct(source,output,tw,th,sw,sh)==1
    assert list(source)==source_vals    # source remains identical
    src_points=[(i%tw,i//tw) for i,p in enumerate(source) if p==0]
    out_points=[(i%tw,i//tw) for i,p in enumerate(output) if p==0]
    src_min=min(x for x,_ in src_points);src_max=max(x for x,_ in src_points)
    dst_min=min(x for x,_ in out_points);dst_max=max(x for x,_ in out_points)
    assert dst_max-dst_min==pytest.approx(
        (src_max-src_min)*factor,abs=2
    )
    assert min(y for _,y in out_points)==min(y for _,y in src_points)
    assert max(y for _,y in out_points)==max(y for _,y in src_points)
    assert output[0]==0xFF000000
    assert output[tw-1]==0xFF000000
    assert output[(th-1)*tw]==0xFF000000
    assert output[tw*th-1]==0xFF000000


def test_original_4by3_reference_remains_bit_identical(correction):
    tw,th=256,128
    source=(ct.c_uint32*(tw*th))(*input_alpha(tw,th))
    output=(ct.c_uint32*(tw*th))()
    assert correction.correct(source,output,tw,th,1024,768)==1
    assert bytes(source)==bytes(output)


def test_stable_one_object_only_one_animated_uniform_zoom(correction):
    tw,th=256,128
    source=(ct.c_uint32*(tw*th))(*input_alpha(tw,th))
    output=(ct.c_uint32*(tw*th))()
    assert correction.correct(source,output,tw,th,1920,1080)==1
    before=bytes(output)
    for scale in (.1,.5,1,2.5,6,12):
        u0,v0,u1,v1=[ct.c_float() for _ in range(4)]
        assert correction.zoom(scale,ct.byref(u0),ct.byref(v0),
                               ct.byref(u1),ct.byref(v1))==1
        assert u0.value==pytest.approx(v0.value,abs=.000001)
        assert u1.value==pytest.approx(v1.value,abs=.000001)
        assert bytes(output)==before  # no frame-dependent sprite edits


def test_invalid_inputs_fail_closed(correction):
    source=(ct.c_uint32*(256*128))(*input_alpha(256,128))
    dest=(ct.c_uint32*(256*128))()
    assert correction.correct(source,dest,256,128,800,1200)==0
    assert correction.correct(source,dest,256,128,0,0)==0
    assert correction.correct(source,dest,0,0,1920,1080)==0
    assert correction.correct(source,source,256,128,1920,1080)==0


def test_v11_only_changes_texture_reference_and_not_3d_or_hud():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    create=src.split("static int ui_atomic_renderer_create_texture(void)",1)[1].split(
        "static void ui_atomic_renderer_acquire_reference(void)",1)[0]
    assert "ui_atomic_fidelity_enabled=0" in src
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_ATOMIC_FIDELITY_TEST" in src
    assert "UI-ATOMIC-FIDELITY: V11 TRIAL ON" in src
    assert "UI-ATOMIC-FIDELITY: OFF" in src
    assert "SpiderAtomicMask_CorrectReference43(" in create
    assert "unsigned sizes[3][2]={{2048u,1024u},{1024u,512u},{512u,256u}}" in create
    assert "for(n=fidelity?0u:1u;n<3u;n++)" in create
    assert "UI-ATOMIC: TEXTURE READY" in create
    assert "ui_atomic_renderer_draw" in src
    assert "SpiderAtomicMask_ScreenUV(" in src
    # V8/V9, HUD and native ESC remain untouched by the new branch.
    assert "maybe_correct_native_pause43(" in src
    assert "maybe_correct_unified_ui_canvas(" in src
    assert "ui_comp_flush((IDirect3DDevice7*)self);" in src
    assert src.count("SPIDERMAN2K1FIX_UI_TRANSITION_ATOMIC_FIDELITY_TEST")==1
