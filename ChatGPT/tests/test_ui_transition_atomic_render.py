"""V10 one-image spider-iris experiment: native-C geometry + host policy.

The source mask is rasterized once in a transparent texture from the
original, **complete** source quads (not per-level coordinates).
Subsequent frames update ONE UV transform for that texture, recovered
from the game's actual original polygon geometry, not from wall time.
CI cannot prove Wine/Windows visual acceptance; real gameplay needed.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT=Path(__file__).resolve().parents[2]


class Quad(ct.Structure):
    _fields_=[("x",ct.c_float*4),("y",ct.c_float*4)]


def quad(points):
    assert len(points)==4
    q=Quad()
    for i,(x,y) in enumerate(points):
        q.x[i]=x;q.y[i]=y
    return q


@pytest.fixture(scope="module")
def native(tmp_path_factory):
    cc=shutil.which("cc") or shutil.which("gcc")
    if not cc:pytest.skip("compiler unavailable")
    d=tmp_path_factory.mktemp("atomic_mask")
    c=d/"atomic.c";so=d/"atomic.so"
    c.write_text(
        '#include "ui_transition_atomic_mask.h"\n'
        'unsigned raster(const SpiderAtomicMaskQuad*q,unsigned count,'
        'unsigned w,unsigned h,uint32_t*px,unsigned tw,unsigned th){'
        'return SpiderAtomicMask_Rasterize(q,count,w,h,px,tw,th);'
        '}\n'
        'int estimate(const SpiderAtomicMaskQuad*q,unsigned n,'
        'const SpiderAtomicMaskQuad*r,unsigned w,unsigned h,'
        'float*scale,float*error){'
        'return SpiderAtomicMask_EstimateScale(q,n,r,w,h,scale,error);'
        '}\n'
        'unsigned covered(const uint32_t*px,unsigned w,unsigned h){'
        'return SpiderAtomicMask_CountOpaquePixels(px,w,h);'
        '}\n'
        'int uv(float s,float*u0,float*v0,float*u1,float*v1){'
        'return SpiderAtomicMask_ScreenUV(s,u0,v0,u1,v1);'
        '}\n'
    )
    subprocess.run([
        cc,"-std=c11","-Wall","-Wextra","-Werror","-fPIC","-shared",
        "-I",str(ROOT),str(c),"-o",str(so),"-lm"
    ],capture_output=True,text=True,check=True)
    lib=ct.CDLL(str(so))
    p=ct.POINTER(Quad)
    lib.raster.argtypes=[p,ct.c_uint,ct.c_uint,ct.c_uint,
                         ct.POINTER(ct.c_uint32),ct.c_uint,ct.c_uint]
    lib.raster.restype=ct.c_uint
    lib.covered.argtypes=[ct.POINTER(ct.c_uint32),ct.c_uint,ct.c_uint]
    lib.covered.restype=ct.c_uint
    lib.estimate.argtypes=[p,ct.c_uint,p,ct.c_uint,ct.c_uint,
                           ct.POINTER(ct.c_float),ct.POINTER(ct.c_float)]
    lib.estimate.restype=ct.c_int
    lib.uv.argtypes=[ct.c_float]+[ct.POINTER(ct.c_float)]*4
    lib.uv.restype=ct.c_int
    return lib


def test_complete_mask_is_one_texture_with_black_exterior_and_transparent_aperture(native):
    # The original transition consists of black exterior, central
    # transparent aperture and black "teeth" connected to that exterior.
    shapes=[
        quad([(0,0),(640,0),(640,100),(0,100)]),
        quad([(0,380),(640,380),(640,480),(0,480)]),
        quad([(0,100),(160,100),(160,380),(0,380)]),
        quad([(480,100),(640,100),(640,380),(480,380)]),
        quad([(160,195),(235,195),(235,265),(160,265)]),
    ]
    source=(Quad*len(shapes))(*shapes)
    out=(ct.c_uint32*(128*96))()
    assert native.raster(source,len(shapes),640,480,out,128,96)>0
    def pixel(x,y):return out[y*128+x]
    assert pixel(1,1)==0xFF000000
    assert pixel(40,50)==0xFF000000
    assert pixel(64,48)==0x00000000
    assert pixel(34,45)==0xFF000000
    assert pixel(126,94)==0xFF000000


def test_rasterizes_unmodified_triangle_fan_not_rectangular_bounds(native):
    triangle=quad([(0,0),(640,0),(0,480),(0,480)])
    source=(Quad*1)(triangle)
    out=(ct.c_uint32*(128*96))()
    assert native.raster(source,1,640,480,out,128,96)>0
    assert out[1*128+1]==0xFF000000
    assert out[92*128+126]==0x00000000


def test_native_geometry_drives_one_positive_zoom(native):
    cx,cy=960,540
    ref=quad([(960,553.5),(967.5,553.5),(967.5,549),(960,549)])
    original=(Quad*1)(ref)
    for factor in (.25,1.,2.,3.4,5.8):
        observed=quad([(cx+(x-cx)*factor,cy+(y-cy)*factor)
                       for x,y in zip(ref.x,ref.y)])
        scale=ct.c_float()
        error=ct.c_float()
        assert native.estimate(original,1,ct.byref(observed),1920,1080,
                               ct.byref(scale),ct.byref(error))==1
        assert scale.value==pytest.approx(factor,abs=.004)
        assert error.value==pytest.approx(0,abs=.015)


def test_single_screen_quad_uvs_scale_uniformly_in_x_and_y(native):
    for factor in (.1,.3,.75,1.,2.,4.,8.,12.):
        coords=[ct.c_float() for _ in range(4)]
        assert native.uv(factor,*[ct.byref(x) for x in coords])==1
        u0,v0,u1,v1=[x.value for x in coords]
        assert u0==pytest.approx(v0,abs=1e-6)
        assert u1==pytest.approx(v1,abs=1e-6)
        assert (u0+u1)/2==pytest.approx(.5,abs=1e-6)
        assert u1-u0==pytest.approx(1/factor,rel=1e-5)


def test_bad_scale_and_raster_inputs_fail_closed(native):
    ref=(Quad*1)(quad([(100,100),(200,100),(200,200),(100,200)]))
    out=(ct.c_uint32*(128*96))()
    assert native.raster(ref,0,640,480,out,128,96)==0
    assert native.raster(ref,1,640,480,out,0,0)==0
    scale,error=ct.c_float(),ct.c_float()
    assert native.estimate(ref,0,ref,1920,1080,
                           ct.byref(scale),ct.byref(error))==0
    for value in (0.,-.2,50.):
        coords=[ct.c_float() for _ in range(4)]
        assert native.uv(value,*[ct.byref(x) for x in coords])==0


def test_gpu_host_is_off_by_default_and_stores_only_native_black_mask():
    source=(ROOT/"spiderman2k1fix.c").read_text()
    assert "ui_atomic_renderer_enabled=0" in source
    assert 'SPIDERMAN2K1FIX_UI_TRANSITION_ATOMIC_RENDER_TEST' in source
    assert 'UI-ATOMIC: TRIAL ON' in source
    assert 'UI-ATOMIC: OFF' in source
    assert 'UI-ATOMIC: TEXTURE READY' in source
    assert 'UI-ATOMIC: GAME SCALE=' in source
    assert 'UI-ATOMIC: COMPOSED' in source
    ref=source.split("static void ui_atomic_renderer_acquire_reference(",1)[1].split(
        "static int ui_atomic_renderer_skip_native(",1)[0]
    assert "color==0x00000000UL" in ref
    assert "q->caller!=SPIDER_TRANSITION_CALLER_RVA" in ref
    assert "SpiderAtomicMask_Rasterize(" in source
    assert "SpiderAtomicMask_EstimateScale(" in source
    assert "SpiderAtomicMask_ScreenUV(" in source
    assert "ui_atomic_scene_suppressed" in source


def test_native_esc_and_hud_preserved_and_one_gpu_quad_drawn_per_frame():
    source=(ROOT/"spiderman2k1fix.c").read_text()
    block=source.split("static int ui_atomic_renderer_draw(",1)[1].split(
        "static void ui_atomic_renderer_finish_scene(",1)[0]
    assert block.count("original_draw_primitive(")==1
    assert "D3DPT_TRIANGLEFAN,0x144UL,v,4u,0u" in block
    assert "D3DSBT_ALL" in block
    assert "IDirect3DDevice7_ApplyStateBlock" in block
    assert "IDirect3DDevice7_GetTexture(" in block
    assert "IDirect3DDevice7_SetTexture(device,0,bound)" in block
    assert "D3DBLEND_SRCALPHA" in block
    assert "D3DBLEND_INVSRCALPHA" in block
    assert "D3DTADDRESS_CLAMP" in block
    assert "D3DRENDERSTATE_ZENABLE" in block
    assert "ui_atomic_renderer_finish_scene((IDirect3DDevice7*)self);" in source
    assert source.index("ui_atomic_renderer_finish_scene((IDirect3DDevice7*)self);") < source.index(
        "ui_comp_flush((IDirect3DDevice7*)self);",source.index(
        "static HRESULT WINAPI ui_compositor_scene_finish("))
    assert "maybe_correct_native_pause43(" in source
    assert "maybe_correct_unified_ui_canvas(" in source
    assert "if(ui_atomic_renderer_skip_native(" in source


def test_no_level_specific_mask_hacks_and_ablate_to_v9():
    source=(ROOT/"spiderman2k1fix.c").read_text()
    region=source.split("/* V10 renderer: D3D7 managed A8R8G8B8 texture",1)[1].split(
        "static HRESULT WINAPI ui_compositor_scene_start(",1)[0]
    assert "level_number" not in region
    assert "boss_name" not in region
    assert "timeGetTime(" not in region
    assert "GetTickCount(" not in region
    assert "maybe_seal_native_transition_teeth(" not in region
    assert "SpiderTransition_MapTopology43(" not in region
    assert "SpiderTransition_MapFullscreenCover43(" not in region
    assert "if(!ui_atomic_texture)ui_atomic_renderer_acquire_reference();" in region
    assert "ui_atomic_scene_mode=2u" in region
    assert "native iris retained" in region


def test_duplicated_winding_quads_do_not_inflate_mask_coverage(native):
    # The Wine hardware native-quad probe saw 92 zero-black draw calls,
    # with 46 pairs of identical polygons in opposite winding order.
    # Raster writes can exceed the whole texture even though its
    # DISTINCT opaque pixels are valid and the aperture is visible.
    w,h=640,480
    shapes=[
        quad([(0,0),(w,0),(w,90),(0,90)]),
        quad([(0,390),(w,390),(w,h),(0,h)]),
        quad([(0,90),(275,90),(275,390),(0,390)]),
        quad([(365,90),(w,90),(w,390),(365,390)]),
    ]
    duplicates=[]
    for q in shapes:
        duplicates.extend([q,quad([(q.x[i],q.y[i]) for i in (0,3,2,1)])])
    source=(Quad*len(duplicates))(*duplicates)
    tw,th=128,96
    pixels=(ct.c_uint32*(tw*th))()
    writes=native.raster(source,len(duplicates),w,h,pixels,tw,th)
    unique=native.covered(pixels,tw,th)
    assert writes>tw*th     # V10 used to reject this valid game rendering.
    assert (tw*th)//10 < unique < (tw*th)*99//100
    assert pixels[(th//2)*tw+(tw//2)]==0   # central opening is clear
    assert pixels[0]==0xFF000000          # exterior is black
    assert sum(bool(p&0xFF000000) for p in pixels)==unique


def test_repeated_quads_do_not_change_unique_pixel_count(native):
    q=quad([(0,0),(640,0),(640,200),(0,200)])
    tw,th=128,96
    a=(ct.c_uint32*(tw*th))()
    b=(ct.c_uint32*(tw*th))()
    one=native.raster((Quad*1)(q),1,640,480,a,tw,th)
    two=native.raster((Quad*2)(q,q),2,640,480,b,tw,th)
    assert two==2*one
    assert native.covered(a,tw,th)==native.covered(b,tw,th)
    assert bytes(a)==bytes(b)
    assert native.covered(a,0,0)==0


def test_real_v10_texture_gate_uses_unique_pixels_not_overdraw():
    s=(ROOT/"spiderman2k1fix.c").read_text()
    function=s.split("static int ui_atomic_renderer_create_texture(void)",1)[1].split(
        "static void ui_atomic_renderer_acquire_reference(void)",1)[0]
    assert "covered=SpiderAtomicMask_CountOpaquePixels(pixels,tw,th)" in function
    assert "if(covered<(tw*th)/10u || covered>=(tw*th)*99u/100u)" in function
    assert "if(painted<(tw*th)/10u" not in function
    assert "UI-ATOMIC: MASK COVERAGE" in function
    assert "triangle-writes=" in function
    assert "UI-ATOMIC: TEXTURE READY" in function
