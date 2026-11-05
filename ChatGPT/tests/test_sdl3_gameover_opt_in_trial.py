"""Fail-closed and explicitly opt-in GAME OVER Enter trial.

Native request 2 is a *pending transition request*, not a proven scene ID.
These tests protect neutral/edge/focus behavior but DO NOT constitute
retail Windows/Wine or GAME OVER success validation.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
BACKEND=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
POLICY=(ROOT/"sdl3_universal_policy.h").read_text(encoding="utf-8")
VALIDATION=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
GENERATED=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")


def test_experimental_trial_is_absent_from_generated_defaults_and_does_not_override_3d():
    assert "ExperimentalSDL3GameOverConfirm=1" not in GENERATED
    assert '"ExperimentalSDL3GameOverConfirm"' in VALIDATION
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3GameOverConfirm",0,ini)==1' in BACKEND
    assert "if(gameover_trial_enabled)" in BACKEND
    assert "gameover_trial_candidate(context,menu,gameplay,bink,native_pause)" in BACKEND
    assert "if(context!=SPIDER_UNIVERSAL_CTX_3D)" in BACKEND
    assert 'pulse(0x1cu,"opt-in GAME OVER Return trial");' in BACKEND
    assert "if(pulse_scan!=0x1cu)" in BACKEND
    assert 'if(pause_edge)pulse(0x01u,"3D Pause");' in BACKEND
    assert "if(!update_keys(desired,scans))" in BACKEND
    assert BACKEND.index("if(gameover_trial_enabled) {\n    int candidate=") < BACKEND.index(
        'if(!update_keys(desired,scans))')
    assert "spider_sdl3_gameover_trial_reset(&gameover_trial);" in BACKEND
    assert "SDL3-GAMEOVER-TRIAL: ENTER PULSE accepted by OS" in BACKEND
    assert "game reception unverified" in BACKEND


def test_candidate_is_a_strict_read_only_runtime_probe():
    block=BACKEND.split("static int gameover_trial_candidate(",1)[1].split(
        "static void gameover_probe_snapshot(",1)[0]
    for addr in ("0x0060CFA4u","0x0054A510u","0x006B78F0u",
                 "0x00682950u","0x00682948u","0x0068293Cu",
                 "0x0054D38Cu"):
        assert addr in block
    for required in ("request==2u","task==0x14u","input_active==1u",
                     "!pause_ctx","!menu_ctx","!dispatcher","!transition",
                     "menu || !gameplay || bink || native_pause"):
        assert required in block
    for forbidden in ("SendInput(", "send_scan(", "update_keys(",
                      "VirtualProtect(", "WriteProcessMemory("):
        assert forbidden not in block
    assert "SPIDER_GAMEOVER_TRIAL_STABLE_FRAMES 6u" in POLICY
    assert "return 2;" in POLICY


def test_trial_requires_stability_neutral_and_one_press(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    src=tmp_path/"gameover_policy.c"
    exe=tmp_path/"gameover_policy"
    src.write_text(textwrap.dedent(r"""
       #include <assert.h>
       #include "sdl3_universal_policy.h"
       int main(void) {
         SpiderSDL3GameOverTrial trial={0};
         unsigned i;
         /* Normal 3D is untouched even with a held South. */
         for(i=0;i<30u;i++)
           assert(spider_sdl3_gameover_trial_step(&trial,0,0,1)==0u);
         assert(!trial.stable_frames && !trial.armed);
         /* A transient request-2 is not enough. */
         for(i=0;i<5u;i++)
           assert(spider_sdl3_gameover_trial_step(&trial,1,0,1)==0u);
         assert(trial.stable_frames==5u && !trial.armed);
         assert(spider_sdl3_gameover_trial_step(&trial,0,0,1)==0u);
         assert(trial.stable_frames==0u);
         /* Six consecutive frames do NOT inherit a held button. */
         for(i=0;i<6u;i++)
           assert(spider_sdl3_gameover_trial_step(&trial,1,0,1)==0u);
         assert(trial.stable_frames==6u && !trial.armed);
         /* A fresh FULL neutral frame is mandatory. */
         assert(spider_sdl3_gameover_trial_step(&trial,1,1,0)==0u);
         assert(trial.armed);
         /* Ambiguous or unrecognized input never confirms. */
         assert(spider_sdl3_gameover_trial_step(&trial,1,0,0)==0u);
         assert(spider_sdl3_gameover_trial_step(&trial,1,0,1)==1u);
         assert(trial.consumed && !trial.armed);
         /* Held South is consumed without a second Enter or Jump. */
         for(i=0;i<30u;i++)
           assert(spider_sdl3_gameover_trial_step(&trial,1,0,1)==2u);
         assert(spider_sdl3_gameover_trial_step(&trial,1,1,0)==0u);
         assert(spider_sdl3_gameover_trial_step(&trial,1,0,1)==1u);
         /* Owner/context loss unconditionally resets the whole trial. */
         assert(spider_sdl3_gameover_trial_step(&trial,0,0,1)==0u);
         assert(!trial.armed && !trial.consumed && !trial.stable_frames);
         assert(spider_sdl3_gameover_trial_step(&trial,1,0,1)==0u);
         return 0;
       }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
