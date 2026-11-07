"""Regression: opening-movie controller readiness and user-facing INI help.

These are source/data checks, not a substitute for an actual Bink playback run.
"""
from pathlib import Path
import configparser

ROOT=Path(__file__).resolve().parents[2]
PROBE=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
UNPLUG=(ROOT/"native_joystick_unplug.c").read_text(encoding="utf-8")
GEN=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
INI=(ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text(encoding="utf-8")


def test_sdl3_is_loaded_on_first_safe_foreground_flip_not_after_120():
    start=PROBE.index("void SpiderSDL3Probe_OnFlip(")
    end=PROBE.index("  /* The setup modal never rebinds",start)
    callback=PROBE[start:end]
    assert "if(!requested || flip<1)return;" in callback
    assert "flip<120" not in callback
    assert "if(sdl3_game_thread && sdl3_game_thread!=GetCurrentThreadId())return;" in callback
    assert "GetForegroundWindow()!=game_hwnd" in callback
    assert callback.index("GetForegroundWindow()!=game_hwnd") < callback.index(
        "if(!sdl3_game_thread)sdl3_game_thread=GetCurrentThreadId();")
    assert callback.index("if(!sdl3_game_thread)sdl3_game_thread=GetCurrentThreadId();") < callback.index(
        "if(!attempted) {")
    assert callback.index("if(!sdl_load())return;") < callback.index("sdl_pump();")
    assert "sdl_update();" in callback
    assert 'SpiderSDL3Probe_OnFlip(count,game_hwnd);' in CORE
    # Native safety stays independently SHA-gated at original stable frame.
    assert "if(flip_index==120)install_guard();" in UNPLUG


def test_early_start_selection_still_requires_neutral_fresh_press():
    assert "spider_sdl3_selection_start_edge(&selection,pads[i].id,pressed)" in PROBE
    assert "matches==1u" in PROBE
    assert "generic_selected_id=pads[i].id;" in PROBE
    assert "selection.active=previous;" in PROBE
    assert "if(!requested || flip<1)return;" in PROBE
    assert "SDL3-PROBE: first safe foreground game Flip" in PROBE


def test_generated_ini_explains_only_real_keys():
    cfg=configparser.ConfigParser(interpolation=None)
    cfg.read_string(INI)
    assert set(cfg.sections())=={"Debug","Display","Input","Audio","Keyboard","Internal"}
    assert list(cfg["Input"])==["keyboardlayout"]
    assert "KeyboardLayout=Auto\n\n[Audio]" in INI
    assert "SDL3" not in INI
    assert "Left Alt" not in INI
    assert "volume" in INI.lower()
    assert "SPIDERMAN2K1FIX_VERSION_LABEL" in GEN
    assert "static const char *const audio_help[3]" in GEN
    assert "static const char *const keyboard_help[11]" in GEN
    assert 'write_ini_text(file,keyboard_header)' in GEN
    assert 'snprintf(line,sizeof(line),"; %s\\r\\n",audio_help[i]);' in GEN
    assert 'snprintf(line,sizeof(line),"; %s\\r\\n",keyboard_help[i]);' in GEN
    assert "Built-in video/HUD/focus/audio fixes" not in GEN
    assert '"; SDL3 detection, setup and universal gameplay' not in GEN
    assert '"; Original Left Alt state is repaired' not in GEN
    for section in ("Debug","Display","Input","Audio","Keyboard","Internal"):
        text=INI.split("["+section+"]",1)[1].split("\n[",1)[0]
        last_comment=""
        for raw in text.splitlines():
            line=raw.strip()
            if line.startswith(";"):
                last_comment=line
            elif "=" in line and not line.startswith("#"):
                name=line.split("=",1)[0]
                assert last_comment, (section,name)
                # Each setting has a directly preceding explanatory comment.
                assert text.splitlines()[text.splitlines().index(raw)-1].lstrip().startswith(";"), (section,name)
                last_comment=""
    for k in ("MusicSFXLevel","VoiceLevel","MovieLevel"):
        assert "; "+k+":" in INI
    for k in ("Up","Down","Left","Right","Jump","Kick","Punch",
              "Web","Aim","Zipline","Swing"):
        assert "; "+k+":" in INI
