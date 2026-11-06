"""V16 clipped-native-geometry experiment: portable C mathematics and guards.

Uses the original 46-quad fixture from the V13 diagnostic. This validates
partial coverage and failure isolation; it does not certify Wine visual
behavior or coverage of every possible real-game culling pattern.
"""
import ctypes as ct
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "ChatGPT/tests/fixtures/spider_iris_native_20261106.csv"


class Quad(ct.Structure):
    _fields_ = [("x", ct.c_float * 4), ("y", ct.c_float * 4)]


class Report(ct.Structure):
    _fields_ = [
        ("reason", ct.c_uint), ("seen", ct.c_uint),
        ("expected", ct.c_uint), ("unique", ct.c_uint),
        ("traps", ct.c_uint), ("union_reason", ct.c_uint),
        ("union_event", ct.c_uint), ("union_object", ct.c_uint),
        ("union_a", ct.c_float), ("union_b", ct.c_float),
        ("max_vertex_deviation", ct.c_float),
        ("rms_vertex_deviation", ct.c_float),
    ]


@pytest.fixture(scope="module")
def api(tmp_path_factory):
    cc = shutil.which("cc") or shutil.which("gcc")
    if not cc:
        pytest.skip("No C compiler")
    folder = tmp_path_factory.mktemp("spider-v16-partial")
    src = folder / "partial.c"
    libpath = folder / "partial.so"
    src.write_text(
        '#include "ui_transition_live_union.h"\n'
        'static SpiderExactTemplate mesh;\n'
        'int prepare(const SpiderAtomicMaskQuad*q,unsigned n,'
        'unsigned expected,const SpiderAtomicMaskQuad*r,unsigned rn,'
        'float overhang,int partial,SpiderLiveUnionReport*d){'
        'return SpiderLiveUnion_PrepareBounded(q,n,expected,1920,1080,'
        'r,rn,1.0f,overhang,partial,&mesh,d);}\n'
    )
    subprocess.run(
        [cc, "-std=c11", "-Werror", "-Wall", "-Wextra", "-fPIC",
         "-shared", "-I", str(ROOT), str(src), "-o",
         str(libpath), "-lm"],
        check=True, text=True, capture_output=True,
    )
    dll = ct.CDLL(str(libpath))
    dll.prepare.argtypes = [
        ct.POINTER(Quad), ct.c_uint, ct.c_uint, ct.POINTER(Quad),
        ct.c_uint, ct.c_float, ct.c_int, ct.POINTER(Report),
    ]
    dll.prepare.restype = ct.c_int
    return dll


def fixture():
    data = []
    for line in FIXTURE.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        a = [float(x) for x in line.split(",")]
        pts = [(a[2*i], a[2*i+1]) for i in range(4)]
        data += [pts, [pts[i] for i in (0,3,2,1)]]
    assert len(data) == 92
    return data


def make(arr):
    return (Quad * len(arr))(*[
        Quad((ct.c_float * 4)(*[p[0] for p in q]),
             (ct.c_float * 4)(*[p[1] for p in q])) for q in arr
    ])


def test_strict_v15_declines_native_culling_but_v16_accepts_proven_union(api):
    source = fixture()
    reference = make(source)
    # Simulated culling: remove a few redundant draw orientations.
    kept = [q for i,q in enumerate(source) if i not in (3,11,19,25)]
    live = make(kept)
    before = bytes(live)
    diagnostic = Report()
    assert api.prepare(live,88,92,reference,92,0,0,ct.byref(diagnostic)) == 0
    assert diagnostic.reason == 2
    assert api.prepare(live,88,92,reference,92,.1,1,ct.byref(diagnostic)) == 1
    assert diagnostic.reason == 0
    assert diagnostic.seen == 88
    assert diagnostic.expected == 92
    assert diagnostic.unique == 46
    assert diagnostic.traps >= 152
    assert bytes(live) == before


def test_safely_accepted_original_outside_viewport_is_not_clamped(api):
    source = fixture()
    reference = make(source)
    moved = [[(x - 85 if x == 0 else x, y) for x,y in q] for q in source]
    live = make(moved)
    report = Report()
    assert api.prepare(live,92,92,reference,92,0,0,ct.byref(report)) == 0
    assert report.reason == 3 and report.union_reason == 1
    assert api.prepare(live,92,92,reference,92,.1,1,ct.byref(report)) == 1
    assert report.reason == 0 and report.traps > 0


def test_reject_beyond_bounded_overhang_and_incomplete_matte(api):
    data = fixture()
    reference = make(data)
    report = Report()
    broken = [[(-240 if x == 0 else x, y) for x,y in q] for q in data]
    assert api.prepare(make(broken),92,92,reference,92,.1,1,
                       ct.byref(report)) == 0
    assert report.reason == 3 and report.union_reason == 1
    # Drop full black left/right original wedges (the last two pairs).
    partial = make(data[:-4])
    assert api.prepare(partial,88,92,reference,92,.1,1,
                       ct.byref(report)) == 0
    assert report.reason == 3 and report.union_reason == 4
    assert api.prepare(partial,88,92,reference,92,.1,0,
                       ct.byref(report)) == 0
    assert report.reason == 2


def test_v16_gated_source_and_v15_unchanged():
    main = (ROOT / "spiderman2k1fix.c").read_text()
    live = (ROOT / "ui_transition_live_union.h").read_text()
    exact = (ROOT / "ui_transition_exact_union.h").read_text()
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_PARTIAL_TEST" in main
    assert "ui_atomic_partial_enabled=0" in main
    assert "ui_atomic_renderer_is_partial_native_black(" in main
    assert "ui_atomic_scene_mode!=1u" in main
    assert "ui_atomic_overflow==0u" in main
    assert "count==ui_atomic_scene_suppressed" in main
    assert "SpiderLiveUnion_PrepareBounded(" in main
    assert "UI-ATOMIC-PARTIAL: V16 VALID" in main
    assert "SpiderExact_PrepareBounded(" in live
    assert "static inline int SpiderExact_Prepare(" in exact
    assert "return SpiderExact_PrepareBounded(source,count,w,h,out,0.0f,diag);" in exact
    assert "return SpiderLiveUnion_PrepareBounded(live,live_count,expected_count" in live
    assert "0.0f,0,out,report" in live
    assert "return ui_atomic_renderer_draw_vector(device);" in main
