"""V18 Dreamcast-like 4:3 iris: preserve original game polygon draws.

Pure C geometry verification plus static routing assertions. No claim that
the PC/Dreamcast renderers are identical; real Wine and Windows footage
must validate the visible iris and side strips.
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
        ("diffuse",ct.c_uint32),("u",ct.c_float),("v",ct.c_float),
    ]


def verts(xs):
    return (Vertex*4)(*[
        Vertex(x,200+31*i,0,27.60,0x00000000,
               0.125*i,0.875-i*.125)
        for i,x in enumerate(xs)
    ])


@pytest.fixture(scope="module")
def api(tmp_path_factory):
    cc=shutil.which("cc") or shutil.which("gcc")
    if not cc:
        pytest.skip("C compiler unavailable")
    folder=tmp_path_factory.mktemp("iris-native43")
    src=folder/"map.c"
    lib=folder/"map.so"
    src.write_text(
        '#include "ui_transition_native43.h"\n'
        'int map(const SpiderMenuVertex*source,SpiderMenuVertex*out,'
        'unsigned count,unsigned w,unsigned h){'
        'return SpiderNative43_Map(source,out,count,w,h);}\n'
    )
    subprocess.run(
        [cc,"-std=c11","-Wall","-Wextra","-Werror","-fPIC",
         "-shared","-I",str(ROOT),str(src),"-o",str(lib),"-lm"],
        capture_output=True,check=True,text=True
    )
    dll=ct.CDLL(str(lib))
    dll.map.argtypes=[
        ct.POINTER(Vertex),ct.POINTER(Vertex),ct.c_uint,
        ct.c_uint,ct.c_uint,
    ]
    dll.map.restype=ct.c_int
    return dll


@pytest.mark.parametrize("width,height,k",[
    (1920,1080,.75),(2560,1080,.5625),
    (1920,1200,5/6),(3440,1440,5760/10320),
])
def test_one_uniform_native_game_polygon_x_transform(api,width,height,k):
    original=verts([0,width/2,width,width*.8])
    untouched=bytes(original)
    mapped=(Vertex*4)()
    assert api.map(original,mapped,4,width,height)==1
    mid=width/2
    for a,b in zip(original,mapped):
        assert b.x==pytest.approx(mid+(a.x-mid)*k,abs=.001)
        assert b.y==a.y
        assert b.z==a.z and b.rhw==a.rhw
        assert b.diffuse==a.diffuse
        assert b.u==a.u and b.v==a.v
    assert bytes(original)==untouched


def test_central_4by3_canvas_has_visible_widescreen_side_world(api):
    native=verts([0,960,1920,960])
    mapped=(Vertex*4)()
    assert api.map(native,mapped,4,1920,1080)==1
    assert [mapped[i].x for i in range(4)]==pytest.approx([
        240,960,1680,960
    ])
    # The renderer does NOT fill the remaining 240px each side.
    assert mapped[0].x==240 and mapped[2].x==1680


def test_4by3_is_game_owned_without_any_mapping(api):
    original=verts([0,640,1280,640])
    target=(Vertex*4)()
    assert api.map(original,target,4,1280,960)==0


def test_invalid_or_incomplete_source_is_never_modified(api):
    original=verts([0,960,1920,960])
    mapped=(Vertex*4)()
    for width,height,count in [
        (1920,1080,3),(640,1200,4),
        (1920,1080,5),
    ]:
        assert api.map(original,mapped,count,width,height)==0
    bad=verts([-300,960,1920,960])
    assert api.map(bad,mapped,4,1920,1080)==0


def test_original_direct3d_call_precedes_all_v6_to_v17_replay_paths():
    src=(ROOT/"spiderman2k1fix.c").read_text()
    header=(ROOT/"ui_transition_native43.h").read_text()
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_TEST" in src
    assert "ui_transition_native43_enabled=0" in src
    assert "UI-TRANSITION-NATIVE43: V18 TRIAL ON" in src
    assert "UI-TRANSITION-NATIVE43: DRAW=" in src
    assert "UI-TRANSITION-NATIVE43: NATIVE-UNCHANGED=" in src
    hook=src[src.index("static HRESULT WINAPI hooked_draw_primitive("):]
    assert hook.index("ui_transition_native43_handle_draw(") < \
        hook.index("ui_atomic_renderer_skip_native(") < \
        hook.index("maybe_correct_native_transition_fullscreen(")
    assert "valid?(LPVOID)mapped:verts,count,flags" in src
    assert "original_draw_primitive(device,type,fvf," in src
    assert "ui_transition_native43_enabled,0,0)==1)" in src
    assert "return; /* No EndScene compositor/reference upload for V18. */" in src
    assert "return 0; /* Native game owns the iris in V18" in src
    assert "No synthetic outer matte" in header
