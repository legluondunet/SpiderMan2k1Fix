"""One-piece proportional fullscreen spider-iris compositor experiment.

These tests compile the real portable C transform used by Direct3D7.
No renderer is available in CI, so visual fidelity and game-owned
render-state validation still need real Wine/Windows hardware tests.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT=Path(__file__).resolve().parents[2]


class V(ct.Structure):
    _fields_=[
        ("x",ct.c_float),("y",ct.c_float),("z",ct.c_float),
        ("rhw",ct.c_float),("diffuse",ct.c_uint32),
        ("u",ct.c_float),("v",ct.c_float)
    ]


def quad(*points):
    assert len(points)==4
    v=(V*4)()
    for i,(x,y) in enumerate(points):
        v[i]=V(x,y,0,27.6,0,0.2+i*.12,0.4+i*.09)
    return v


@pytest.fixture(scope="module")
def transform(tmp_path_factory):
    cc=shutil.which("cc") or shutil.which("gcc")
    if not cc:
        pytest.skip("C compiler needed for real source-transform tests")
    folder=tmp_path_factory.mktemp("fullscreen_iris")
    source=folder/"unit.c"
    output=folder/"unit.so"
    source.write_text(
        '#include "ui_transition_gutters.h"\n'
        'int cover(const SpiderMenuVertex*s,SpiderMenuVertex*d,'
        'unsigned w,unsigned h){'
        'return SpiderTransition_MapFullscreenCover43(s,d,w,h);'
        '}\n'
        'void contain(const SpiderMenuVertex*s,SpiderMenuVertex*d,'
        'unsigned w,unsigned h){'
        'SpiderTransition_MapInner43(s,d,w,h);'
        '}\n'
    )
    subprocess.run([
        cc,"-std=c11","-Wall","-Wextra","-Werror","-fPIC","-shared",
        "-I",str(ROOT),str(source),"-o",str(output)
    ],check=True,capture_output=True,text=True)
    lib=ct.CDLL(str(output))
    lib.cover.argtypes=[ct.POINTER(V),ct.POINTER(V),ct.c_uint,ct.c_uint]
    lib.cover.restype=ct.c_int
    lib.contain.argtypes=[ct.POINTER(V),ct.POINTER(V),ct.c_uint,ct.c_uint]
    return lib


@pytest.mark.parametrize("width,height",[
    (1920,1080),(1920,1200),(2560,1080),(3440,1440)
])
def test_one_uniform_similarity_transform_covers_full_display(transform,width,height):
    source=quad((0,0),(width,0),(width,height),(0,height))
    centered=(V*4)()
    actual=(V*4)()
    assert transform.cover(source,actual,width,height)==1
    transform.contain(source,centered,width,height)
    zoom=3*width/(4*height)
    assert zoom>1
    for a,b,c in zip(source,centered,actual):
        # The exact same proportional 4:3 picture, globally zoomed
        # about the physical screen center until its width fills screen.
        expected_x=width/2+(b.x-width/2)*zoom
        expected_y=height/2+(b.y-height/2)*zoom
        assert c.x==pytest.approx(expected_x,abs=.05)
        assert c.y==pytest.approx(expected_y,abs=.05)
        assert c.x==pytest.approx(a.x,abs=.05)
        assert c.z==a.z and c.rhw==a.rhw
        assert c.diffuse==a.diffuse
        assert c.u==a.u and c.v==a.v
    # Never leave physical side gutters exposed at the initial iris:
    assert min(p.x for p in actual)==pytest.approx(0,abs=.05)
    assert max(p.x for p in actual)==pytest.approx(width,abs=.05)


def test_fullscreen_iris_keeps_shared_polygon_vertices_identical(transform):
    # In the old 4:3+gutter method, these touching polygons could become
    # visually disconnected from the separately extended black bands.
    left=quad((0,100),(300,100),(300,650),(0,650))
    right=quad((300,100),(550,200),(550,560),(300,650))
    dst_l=(V*4)();dst_r=(V*4)()
    assert transform.cover(left,dst_l,1920,1080)==1
    assert transform.cover(right,dst_r,1920,1080)==1
    assert dst_l[1].x==dst_r[0].x
    assert dst_l[1].y==dst_r[0].y
    assert dst_l[2].x==dst_r[3].x
    assert dst_l[2].y==dst_r[3].y


def test_last_frame_disjoint_mask_pieces_are_not_invented(transform):
    # A source quad that occupies only the middle of the frame must
    # remain there. Cover never synthesizes two full-height outside
    # black strips or detached side triangles.
    src=quad((500,400),(850,400),(850,700),(500,700))
    dst=(V*4)()
    assert transform.cover(src,dst,1920,1080)==1
    assert min(p.x for p in dst)==500
    assert max(p.x for p in dst)==850
    assert min(p.y for p in dst)>0
    assert max(p.y for p in dst)<1080


def test_cover_crops_vertical_edges_symmetrically_not_distorts_shape(transform):
    src=quad((0,0),(1920,0),(1920,1080),(0,1080))
    dest=(V*4)()
    assert transform.cover(src,dest,1920,1080)==1
    assert dest[0].y==pytest.approx(-180,abs=.02)
    assert dest[2].y==pytest.approx(1260,abs=.02)
    assert dest[0].x==0 and dest[1].x==1920
    assert dest[3].y==dest[2].y


@pytest.mark.parametrize("width,height",[(640,480),(1024,768),(1280,1024),
                                          (800,1200),(0,0),(1920,0)])
def test_original_four_three_or_invalid_geometry_fails_closed(transform,width,height):
    src=quad((0,0),(640,0),(640,480),(0,480))
    dst=(V*4)()
    assert transform.cover(src,dst,width,height)==0


def test_host_is_separate_opt_in_and_falls_back_to_v6_without_extra_draws():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    start=src.index("static const void *maybe_correct_native_transition_fullscreen(")
    end=src.index("/* Full-viewport native iris reconstruction",start)
    hook=src[start:end]
    assert 'SPIDERMAN2K1FIX_UI_TRANSITION_FULLSCREEN_TEST' in src
    assert "static volatile LONG ui_transition_fullscreen_trial_enabled=0" in src
    assert "ui_transition_trial_enabled,0,0)==1" in src
    assert "SpiderTransition_IsNativeBlackMask(" in hook
    assert "diffuse!=0x00000000UL" in hook
    assert "SpiderUICanvas_43IsFullScreenEffect(" in hook
    assert "IDirect3DDevice7_GetTexture(" in hook
    assert "D3DRENDERSTATE_ALPHABLENDENABLE" in hook
    assert "D3DRENDERSTATE_ALPHATESTENABLE" in hook
    assert "FAILED(tex_hr) || texture || FAILED(hb) || FAILED(ht) || blend || atest" in hook
    assert "IDirectDrawSurface7_Release(texture)" in hook
    assert "SpiderTransition_MapFullscreenCover43(" in hook
    assert "original_draw_primitive(" not in hook
    assert "SetRenderState" not in hook
    assert "SetTexture" not in hook
    assert "UI-TRANSITION-COVER: unified-mask=" in hook
    assert "transition_cover_draw=1;" in src
    assert "&& !transition_cover_draw)" in src
    # Existing native ESC and HUD presentation pipeline must remain.
    assert "ui_comp_capture((IDirect3DDevice7*)self" in src
    assert "maybe_correct_native_pause43(" in src
    assert "maybe_correct_unified_ui_canvas(" in src
    assert "maybe_restore_native_transition_gutters(" in src


def test_hardware_validation_pending_and_no_per_level_gameplay_hacks():
    code=(ROOT/"spiderman2k1fix.c").read_text().split(
        "static const void *maybe_correct_native_transition_fullscreen(",1)[1].split(
        "/* Full-viewport native iris reconstruction",1)[0]
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_FULLSCREEN_TEST" not in code
    assert "level_number" not in code
    assert "level_name" not in code
    assert "boss_name" not in code
