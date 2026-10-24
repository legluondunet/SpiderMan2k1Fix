"""Release-specific structural guards; not a replacement for Windows/Wine tests."""
from pathlib import Path

BASE = Path(__file__).resolve().parents[2]
CORE = (BASE / 'spiderman2k1fix.c').read_text()
VALIDATOR = (BASE / 'ini_validation.h').read_text()
BUILD = (BASE / 'build.sh').read_text()


def test_required_graphic_fix_and_input_hooks_still_present():
    for call in (
        'maybe_correct_native_comic(',
        'scale_menu_vertices(',
        'SpiderFOV_Init(',
        'SpiderInputFocus_OnFlip(count);',
        'SpiderInputFocus_Install(log_line);',
        'SpiderCfg_Install(ini_path,log_line);',
        'SpiderDisplayMenu_Install(',
        'SpiderFrameLimiter_Wait(',
        'borderless_intro_fill_black_bars(',
    ):
        assert call in CORE


def test_no_unverified_raster_experiments_or_pixel_witnesses():
    for symbol in (
        'edge_witness_emit', 'edge_ab_draw_primitive', 'maybe_edge_raster_bias',
        'edge_pixel_readback', 'edge_original_note', 'hooked_d3d_clear',
        'hooked_end_scene', 'ExperimentalEdgeDrawWitness',
        'ExperimentalEdgePixelTrace', 'sprite_entry_hook',
    ):
        assert symbol not in CORE
        assert symbol not in VALIDATOR


def test_no_forwarding_only_render_hooks_or_hot_path_collectors():
    for symbol in (
        'hooked_bltfast', 'hooked_lock', 'hooked_draw_primitive_vb',
        'hooked_draw_indexed_primitive_vb', 'collect_geometry',
        'collect_callsite', 'collect_xy', 'check_phase_hotkeys',
        'comic_trace_quad', 'intro_trace_report', 'hooked_set_transform',
    ):
        assert symbol not in CORE
    assert 'patched_surface_vtable[5]=(void *)hooked_blt;' in CORE
    assert 'patched_surface_vtable[11]=(void *)hooked_flip;' in CORE
    assert 'patched_device_vtable[25]=(void*)hooked_draw_primitive;' in CORE


def test_logging_disabled_in_factory_and_no_unneeded_periodic_trace():
    ini = (BASE / 'SpiderMan2k1Fix.ini').read_text()
    assert '\nLog=0\n' in ini
    assert 'if(debug_log_enabled && count%120==0)' in CORE
    assert 'if(debug_log_enabled && count<=24)' in CORE


def test_windows_build_keeps_runtime_checks():
    for token in ('CRT-PROBE:', 'msvcrt.dll', 'pei-i386', 'winmm-exports.txt'):
        assert token.lower() in BUILD.lower()
