"""V8 optional source-based black-tooth connectors in physical gutters.

Compile the exact C helper used by the game. The tests verify that
only existing flat black exterior polygon edges may be connected to
the physical screen border; a detached center object or pointed edge
is never turned into a wide opaque strip. Visual fidelity remains a
hardware acceptance gate, not an implication of unit-test success.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT=Path(__file__).resolve().parents[2]


class Vertex(ct.Structure):
    _fields_=[
        ("x",ct.c_float),("y",ct.c_float),
        ("z",ct.c_float),("rhw",ct.c_float),
        ("diffuse",ct.c_uint32),
        ("u",ct.c_float),("v",ct.c_float),
    ]


def quad(x0,y0,x1,y1,*,diffuse=0x00000000,z=0.0,rhw=27.6):
    verts=(Vertex*4)()
    for i,(x,y) in enumerate(((x0,y0),(x1,y0),(x1,y1),(x0,y1))):
        verts[i]=Vertex(x,y,z,rhw,diffuse,float(i%2),float(i>=2))
    return verts


@pytest.fixture(scope="module")
def seal(tmp_path_factory):
    cc=shutil.which("cc") or shutil.which("gcc")
    if not cc:
        pytest.skip("native C compiler unavailable")
    d=tmp_path_factory.mktemp("tooth_edge")
    src=d/"seal.c"
    lib=d/"seal.so"
    src.write_text(
        '#include "ui_transition_gutters.h"\n'
        'unsigned seal(const SpiderMenuVertex*s,SpiderMenuVertex*d,'
        'unsigned side,unsigned w,unsigned h){'
        'return SpiderTransition_SealOuterToothEdge(s,d,side,w,h);'
        '}\n'
    )
    subprocess.run([
        cc,"-std=c11","-Wall","-Wextra","-Werror",
        "-fPIC","-shared","-I",str(ROOT),str(src),"-o",str(lib)
    ],check=True,capture_output=True,text=True)
    dll=ct.CDLL(str(lib))
    dll.seal.argtypes=[
        ct.POINTER(Vertex),ct.POINTER(Vertex),
        ct.c_uint,ct.c_uint,ct.c_uint
    ]
    dll.seal.restype=ct.c_uint
    return dll


def result(seal,source,side,w=1920,h=1080):
    dest=(Vertex*4)()
    before=bytes(source)
    count=seal.seal(source,dest,side,w,h)
    assert bytes(source)==before  # Original mask vertices unchanged.
    assert count in (0,4)
    return list(dest)[:count]


@pytest.mark.parametrize("w,h",[
    (1920,1080),(1920,1200),(2560,1080),(3440,1440)
])
def test_verified_outer_flat_teeth_connect_to_corresponding_screen_edge(seal,w,h):
    g=(w-h*4/3)*.5
    left=quad(g*.45,h*.30,g*.75,h*.60)
    right=quad(w-g*.75,h*.30,w-g*.45,h*.60)
    l=result(seal,left,0,w,h)
    r=result(seal,right,1,w,h)
    assert len(l)==len(r)==4
    assert {p.x for p in l}==pytest.approx({0.0,g*.45},abs=.05)
    assert {p.x for p in r}==pytest.approx({w-g*.45,float(w)},abs=.05)
    assert {p.y for p in l}==pytest.approx({h*.30,h*.60},abs=.05)
    assert {p.y for p in r}==pytest.approx({h*.30,h*.60},abs=.05)
    for v in l+r:
        assert v.diffuse==0 and v.z==0.0 and v.rhw==pytest.approx(27.6)


def test_other_screen_edge_never_sealed(seal):
    assert result(seal,quad(105,170,207,370),1)==[]
    assert result(seal,quad(1710,170,1810,370),0)==[]


def test_already_connected_or_center_native_masks_are_not_extended(seal):
    assert result(seal,quad(0,175,200,360),0)==[]
    assert result(seal,quad(1710,175,1920,360),1)==[]
    assert result(seal,quad(400,175,700,360),0)==[]
    assert result(seal,quad(400,175,700,360),1)==[]
    assert result(seal,quad(1000,100,1500,900),0)==[]


def test_pointed_teeth_fail_closed_without_flat_extremal_edge(seal):
    vertices=(Vertex*4)()
    for i,(x,y) in enumerate([(110,260),(180,170),(205,340),(180,360)]):
        vertices[i]=Vertex(x,y,0,27.6,0,0,0)
    assert result(seal,vertices,0)==[]


def test_wrong_alpha_depth_rhw_or_aspect_fails_closed(seal):
    for wrong in [
        {"diffuse":0xFF000000},
        {"diffuse":0x80000000},
        {"diffuse":0xFFFFFFFF},
        {"z":.001},
        {"rhw":9.49},
    ]:
        assert result(seal,quad(110,175,207,360,**wrong),0)==[]
    assert result(seal,quad(110,175,207,360),0,640,480)==[]


def test_seal_must_be_strict_separate_opt_in_after_v7_material_checks():
    s=(ROOT/"spiderman2k1fix.c").read_text()
    helper=s.split("static void maybe_seal_native_transition_teeth(",1)[1].split(
        "/* Full-viewport native iris reconstruction",1)[0]
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_EDGE_SEAL_TEST" in s
    assert "ui_transition_seal_trial_enabled=0" in s
    assert "ui_transition_fullscreen_trial_enabled,0,0)!=1" in helper
    assert "SpiderTransition_SealOuterToothEdge(" in helper
    assert "original_draw_primitive(" in helper
    assert "SetRenderState" not in helper
    assert "SetTexture" not in helper
    assert "UI-TRANSITION-SEAL: connector=" in helper
    assert "if(transition_cover_draw && v!=verts)" in s
    assert "maybe_seal_native_transition_teeth(" in s
    assert "ui_transition_fullscreen_trial_enabled,0,0)==1 &&" in s
    assert "UI-TRANSITION-COVER: unified-mask=" in s
