"""V10 prerequisite: native whole-frame black-mask diagnostic only.

No attempt to reconnect a silhouette from independent rectangles is
acceptable as a final fix. Before GPU offscreen composition, identify
all native screen-space black draw families in the SAME original frame.
This deliberately changes no pixels and needs real Wine log evidence.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def probe_source():
    return (ROOT/"spiderman2k1fix.c").read_text()


def function(source,start,end):
    assert source.count(start)==1
    assert source.count(end)>=1
    return source.split(start,1)[1].split(end,1)[0]


def test_atomic_probe_remains_off_by_default_and_read_only():
    src=probe_source()
    capture=function(src,"static void ui_atomic_probe_raw_draw(",
                     "static void ui_atomic_probe_finish_scene(")
    report=function(src,"static void ui_atomic_probe_finish_scene(",
                    "static const void *maybe_correct_unified_ui_canvas(")
    assert "SPIDERMAN2K1FIX_UI_TRANSITION_ATOMIC_PROBE" in src
    assert "static volatile LONG ui_atomic_probe_trial_enabled=0" in src
    assert "UI-ATOMIC-PROBE: ON read-only" in src
    assert "UI-ATOMIC-PROBE: OFF" in src
    assert "SpiderTransition_IsNativeBlackMask" not in capture
    assert "SPIDER_TRANSITION_CALLER_RVA" in capture
    assert "0x00000000UL" in capture
    assert "0xFF000000UL" in capture
    assert "geom_rva(caller,&rva)" in capture
    assert "p->x[i]=v[i].x;p->y[i]=v[i].y" in capture
    assert "ui_atomic_count>=SPIDER_ATOMIC_PROBE_MAX" in capture
    assert "ui_atomic_logged_scenes>=75u" in report
    assert "frame=%u quad=%u argb=" in report
    for fragment in ("original_draw_primitive(", "SetRenderState(",
                     "SetTexture(", "SetRenderTarget(", "memcpy(mapped"):
        assert fragment not in capture
        assert fragment not in report


def test_probe_is_called_before_any_primitives_are_remapped():
    s=probe_source()
    draw=function(s,"static HRESULT WINAPI hooked_draw_primitive(",
                  "static HRESULT WINAPI hooked_draw_indexed_primitive(")
    assert draw.index("ui_atomic_probe_raw_draw(") < draw.index(
        "const void *v=scale_menu_vertices(")
    assert "ui_atomic_probe_begin_scene();" in function(
        s,"static HRESULT WINAPI ui_compositor_scene_start(",
        "static HRESULT WINAPI ui_compositor_scene_finish(")
    assert "ui_atomic_probe_finish_scene();" in function(
        s,"static HRESULT WINAPI ui_compositor_scene_finish(",
        "static HRESULT WINAPI hooked_draw_primitive(")


def test_no_accidental_new_fragment_level_mask_warp_or_level_specific_hacks():
    s=probe_source()
    capture=function(s,"static void ui_atomic_probe_raw_draw(",
                     "static void ui_atomic_probe_finish_scene(")
    assert "level_number" not in capture
    assert "boss_name" not in capture
    assert "level_id" not in capture
    assert "return source" not in capture
    assert "UI-TRANSITION-TOPOLOGY: TRIAL ON" in s  # previous A/B remains
    assert "UI-TRANSITION-SEAL: TRIAL ON" in s
    assert "UI-ATOMIC-PROBE: CAP COMPLETE" in s
