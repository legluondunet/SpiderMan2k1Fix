"""V9 continuous iris mask: topology must remain connected under mapping.

Hardware V8 clip evidence: at 1920x1080 video frame 135-140 the
black mask has four large connected components (top, bottom, left and
right), unlike Dreamcast's connected outer matte. The V7 Y cover crop
and V8 detached per-tooth rectangular replays are unnecessary in V9.
This module compiles the real header C transform and tests its global
injectivity and pixel-edge continuity. It does NOT establish actual
Wine/Windows visual parity; a 60 fps video and full log are required.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT=Path(__file__).resolve().parents[2]


class Vertex(ct.Structure):
    _fields_=[("x",ct.c_float),("y",ct.c_float),("z",ct.c_float),
              ("rhw",ct.c_float),("diffuse",ct.c_uint32),
              ("u",ct.c_float),("v",ct.c_float)]


def quad(*points):
    assert len(points)==4
    arr=(Vertex*4)()
    for i,(x,y) in enumerate(points):
        arr[i]=Vertex(x,y,0,27.6,0,float(i%2),float(i>=2))
    return arr


@pytest.fixture(scope="module")
def topology(tmp_path_factory):
    cc=shutil.which("cc") or shutil.which("gcc")
    if not cc: pytest.skip("C compiler is required for geometry validation")
    d=tmp_path_factory.mktemp("iris_topology")
    p=d/"topo.c";so=d/"topo.so"
    p.write_text(
        '#include "ui_transition_gutters.h"\n'
        'int topo(const SpiderMenuVertex*a,SpiderMenuVertex*b,'
        'unsigned w,unsigned h){'
        'return SpiderTransition_MapTopology43(a,b,w,h);'
        '}\n'
        'void inner(const SpiderMenuVertex*a,SpiderMenuVertex*b,'
        'unsigned w,unsigned h){'
        'SpiderTransition_MapInner43(a,b,w,h);'
        '}\n'
    )
    subprocess.run([
        cc,"-std=c11","-Wall","-Wextra","-Werror","-fPIC","-shared",
        "-I",str(ROOT),str(p),"-o",str(so)
    ],check=True,capture_output=True,text=True)
    lib=ct.CDLL(str(so))
    lib.topo.argtypes=[ct.POINTER(Vertex),ct.POINTER(Vertex),ct.c_uint,ct.c_uint]
    lib.topo.restype=ct.c_int
    lib.inner.argtypes=[ct.POINTER(Vertex),ct.POINTER(Vertex),ct.c_uint,ct.c_uint]
    return lib


def map_x(lib,x,w,h):
    a=quad((x,0),(x,h/2),(x,h),(x,h/4))
    out=(Vertex*4)()
    assert lib.topo(a,out,w,h)==1
    return out[0].x


@pytest.mark.parametrize("w,h",[(1920,1080),(1920,1200),
                                 (2560,1080),(3440,1440)])
def test_horizontal_mapping_is_monotone_and_never_crops_screen_bounds(topology,w,h):
    seq=[map_x(topology,w*i/1024,w,h) for i in range(1025)]
    assert seq[0]==pytest.approx(0,abs=.02)
    assert seq[-1]==pytest.approx(w,abs=.02)
    assert all(b>a for a,b in zip(seq,seq[1:]))
    assert min(seq)>=-.02 and max(seq)<=w+.02
    assert map_x(topology,w/2,w,h)==pytest.approx(w/2,abs=.02)


@pytest.mark.parametrize("w,h",[(1920,1080),(1920,1200),
                                 (2560,1080),(3440,1440)])
def test_inner_canvas_preserves_original_4by3_geometry(topology,w,h):
    for factor in (.35,.40,.50,.60,.65):
        x=w*factor
        expected=w/2+(x-w/2)*(4*h/(3*w))
        assert map_x(topology,x,w,h)==pytest.approx(expected,abs=.03)


def test_vertical_positions_color_depth_alpha_and_texcoords_never_change(topology):
    src=quad((0,0),(1920,0),(1920,1080),(0,1080))
    dst=(Vertex*4)()
    assert topology.topo(src,dst,1920,1080)==1
    assert dst[0].x==pytest.approx(0,abs=.01)
    assert dst[1].x==pytest.approx(1920,abs=.01)
    for a,b in zip(src,dst):
        assert a.y==b.y
        assert a.z==b.z and a.rhw==b.rhw
        assert a.u==b.u and a.v==b.v
        assert a.diffuse==b.diffuse
    # Contrast: V7 cover did crop Y by 180 pixels at each side.
    # V9 must never introduce that loss of black exterior topology.
    assert dst[0].y==0 and dst[2].y==1080


def test_shared_native_vertices_remain_shared_after_smooth_mask_warp(topology):
    first=quad((0,0),(600,100),(600,550),(0,550))
    second=quad((600,100),(1000,200),(1000,500),(600,550))
    a=(Vertex*4)();b=(Vertex*4)()
    assert topology.topo(first,a,1920,1080)==1
    assert topology.topo(second,b,1920,1080)==1
    assert a[1].x==b[0].x and a[1].y==b[0].y
    assert a[2].x==b[3].x and a[2].y==b[3].y


@pytest.mark.parametrize("w,h",[(640,480),(1024,768),(800,1200),
                                 (0,0),(1920,0)])
def test_aspect_four_by_three_and_invalid_screens_fail_closed(topology,w,h):
    src=quad((0,0),(640,0),(640,480),(0,480))
    dst=(Vertex*4)()
    assert topology.topo(src,dst,w,h)==0


def test_v9_is_reversible_and_separate_from_esc_and_v8_replay():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    h=(ROOT/"ui_transition_gutters.h").read_text()
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_TOPOLOGY_TEST" in src
    assert "ui_transition_topology_trial_enabled=0;" in src
    assert "UI-TRANSITION-TOPOLOGY: TRIAL ON" in src
    assert "UI-TRANSITION-TOPOLOGY: mapped=" in src
    assert "SpiderTransition_MapTopology43(" in src
    assert "ui_transition_topology_trial_enabled,0,0)==1" in src
    # V8's extra black rectangles must be excluded in V9 mode, not
    # combined with the new coherent transform.
    v8=src.split("static void maybe_seal_native_transition_teeth(",1)[1].split(
        "/* Full-viewport native iris reconstruction",1)[0]
    assert "ui_transition_topology_trial_enabled,0,0)==1" in v8
    assert "original_draw_primitive(" in v8
    assert "original_draw_primitive(" not in src.split(
        "static const void *maybe_correct_native_transition_fullscreen(",1)[1].split(
        "/* V8 topology connector",1)[0]
    assert "if(transition_cover_draw && v!=verts)" in src
    assert "ui_comp_capture((IDirect3DDevice7*)self" in src
    assert "maybe_correct_native_pause43(" in src
    assert "maybe_correct_unified_ui_canvas(" in src
    assert "SpiderTransition_Smooth01(" in h
