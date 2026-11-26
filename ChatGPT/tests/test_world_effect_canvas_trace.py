"""Opt-in observation of common 4:3 canvas transforms; no renderer behavior change."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = (ROOT / "spiderman2k1fix.c").read_text()


def test_canvas_trace_is_opt_in_and_bounded():
    assert 'static int render_canvas_trace_enabled=0;' in SRC
    assert 'render_canvas_trace_enabled=debug_log_enabled &&' in SRC
    assert 'GetPrivateProfileIntA("Debug","RenderCanvasTrace",0,path)==1' in SRC
    assert '#define CANVAS_TRACE_CAP 128u' in SRC
    assert '#define CANVAS_TRACE_SHOTS 8u' in SRC
    assert 'canvas_trace_frames[2]' in SRC
    observe = SRC.split("static void canvas_trace_observe(", 1)[1].split(
        "static void canvas_trace_f9(", 1)[0]
    assert 'frame->stored>=CANVAS_TRACE_CAP' in observe
    assert 'canvas_trace_shots>=CANVAS_TRACE_SHOTS' in observe
    for forbidden in ("log_line(", "SetRenderState(", "GetRenderState(",
                      "DrawPrimitive(", "GetTexture(", "malloc(", "dst[i].x="):
        assert forbidden not in observe


def test_trace_records_only_successful_canvas_mapping_and_queued_f9():
    mapping = SRC.split("static const void *maybe_correct_unified_ui_canvas(", 1)[1].split(
        "/* Source-verified gameplay HUD quads only.", 1)[0]
    assert mapping.index("if(!SpiderUICanvas_43Transform(") < mapping.index("canvas_trace_observe(")
    assert mapping.count("canvas_trace_observe(") == 1
    assert "return copy;" in mapping
    dump = SRC.split("static void canvas_trace_f9(", 1)[1].split(
        "static const void *maybe_correct_unified_ui_canvas(", 1)[0]
    assert 'frame->flip!=epoch' in dump
    assert 'frame->total-frame->stored' in dump
    assert 'canvas_trace_shots>=CANVAS_TRACE_SHOTS' in dump
    assert "if(queued)canvas_trace_f9();" in SRC
