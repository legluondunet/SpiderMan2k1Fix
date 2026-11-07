"""Fail-closed default-on GAME OVER controller confirmation.

The exact original game-owned menu call is mandatory: request=2 alone
cannot grant Return. Neutral/edge/focus behavior is independently tested;
positive field logs do not replace native Windows hardware regression.
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


def test_native_gameover_confirmation_is_mandatory_and_exact_owner_gated():
    assert "ExperimentalSDL3GameOverConfirm=1\\r\\n" not in GENERATED
    assert "ExperimentalSDL3GameOverConfirm=0" not in GENERATED
    assert '"ExperimentalSDL3GameOverConfirm"' in VALIDATION
    assert "gameover_trial_enabled=1;" in BACKEND
    assert "if(gameover_trial_enabled)" in BACKEND
    assert "gameover_trial_candidate(context,menu,gameplay,bink,native_pause)" in BACKEND
    assert "if(context!=SPIDER_UNIVERSAL_CTX_3D)" in BACKEND
    assert 'pulse(0x1cu,"native GAME OVER Return confirmation");' in BACKEND
    assert "if(pulse_scan!=0x1cu)" in BACKEND
    assert 'if(pause_edge)pulse(0x01u,"3D Pause");' in BACKEND
    assert "if(!update_keys(desired,scans))" in BACKEND
    assert BACKEND.index("if(gameover_trial_enabled) {") < BACKEND.index(
        'if(!update_keys(desired,scans))')
    assert "if(gameover_trial_enabled)\n      trial=spider_sdl3_gameover_trial_step(" in BACKEND
    assert "spider_sdl3_gameover_trial_reset(&gameover_trial);" in BACKEND
    assert "SDL3-GAMEOVER: ENTER PULSE accepted by OS" in BACKEND
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


def test_obsolete_independent_owner_probe_is_completely_retired():
    # Only the original, authenticated CALL-site owner may authorize Return.
    validation=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    migrator=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
    assert '"ExperimentalSDL3GameOverOwnerProbe"' in validation
    assert '{"Input","ExperimentalSDL3GameOverOwnerProbe"}' in migrator
    assert "gameover_owner_probe_" not in BACKEND
    assert "gameover_probe_native_title_now" not in BACKEND
    assert "SDL3-GAMEOVER-OWNER-PROBE:" not in BACKEND
    assert "gameover_trial_enabled=1;" in BACKEND
    assert "gameover_native_owner_hook_installed &&" in BACKEND
    assert "spider_sdl3_gameover_trial_step(&gameover_trial,owner_confirm," in BACKEND


def test_native_gameover_owner_wrapper_is_exact_retained_call_not_a_new_menu():
    wrapper=BACKEND.split(
        "static int __cdecl spider_gameover_owner_call_wrapper(void)",1
    )[1].split("static int spider_gameover_owner_install_hook(void)",1)[0]
    assert "InterlockedIncrement(&gameover_native_owner_depth)" in wrapper
    assert "InterlockedDecrement(&gameover_native_owner_depth)" in wrapper
    assert "((int (__cdecl *)(void))(uintptr_t)0x0047D830u)()" in wrapper
    assert "return result;" in wrapper
    for forbidden in ("WriteProcessMemory(", "VirtualProtect(",
                      "SendInput(", "send_scan(", "update_keys("):
        assert forbidden not in wrapper

    installer=BACKEND.split(
        "static int spider_gameover_owner_install_hook(void)",1
    )[1].split("/* Exact retail PE32: keyboard GetDeviceData",1)[0]
    assert "original[5]={0xE8,0x9A,0x78,0x02,0x00}" in installer
    assert "0x00455F91u" in installer
    assert "sizeof(void*)!=4" in installer
    assert "GetModuleHandleA(NULL)!=0x00400000u" in installer
    assert "memcmp(site,original,sizeof(original))" in installer
    assert "VirtualProtect(" in installer
    assert "FlushInstructionCache(" in installer
    assert "int64_t displacement;" in installer
    assert "displacement>INT32_MAX || displacement<INT32_MIN" in installer
    assert "gameover_native_owner_hook_installed=memcmp(" in installer


def test_strict_gameover_owner_is_always_required_and_fails_closed():
    assert '"ExperimentalSDL3GameOverNativeOwner"' in (
        ROOT/"ini_validation.h"
    ).read_text()
    assert "ExperimentalSDL3GameOverNativeOwner=1" not in (
        ROOT/"virtual_spidey_cfg.c"
    ).read_text()
    assert "ExperimentalSDL3GameOverConfirm=1\\r\\n" not in (
        ROOT/"virtual_spidey_cfg.c"
    ).read_text()
    assert "gameover_native_owner_trial_enabled=1;" in BACKEND
    assert "gameover_native_owner_trial_enabled=1;" in BACKEND
    assert "authenticated original native owner is mandatory" in BACKEND
    assert "if(requested && gameover_native_owner_trial_enabled)" in BACKEND
    assert "int installed=spider_gameover_owner_install_hook();" in BACKEND
    assert "SDL3-GAMEOVER-NATIVE: hook REFUSED" in BACKEND
    assert "int owner_confirm=candidate && gameover_native_owner_trial_enabled &&" in BACKEND
    assert "gameover_native_owner_hook_installed &&" in BACKEND
    assert "InterlockedCompareExchange(&gameover_native_owner_depth,0,0)>0;" in BACKEND
    assert "trial=spider_sdl3_gameover_trial_step(&gameover_trial,owner_confirm," in BACKEND
    assert "if(owner_confirm &&" in BACKEND
    assert "if(gameover_trial_enabled)" in BACKEND
    assert 'pulse(0x1cu,"native GAME OVER Return confirmation");' in BACKEND


def test_no_legacy_fallback_in_current_main_gameover_runtime():
    """Neither the default mode nor an explicit confirm=1 may permit a
    request-2-only Return: the original game-owner hook is mandatory."""
    assert "gameover_native_owner_trial_enabled=1;" in BACKEND
    assert "if(requested && gameover_native_owner_trial_enabled)" in BACKEND
    assert "spider_gameover_owner_install_hook()" in BACKEND
    assert "SDL3-GAMEOVER-NATIVE: hook REFUSED" in BACKEND
    assert "CONFIRM BLOCKED: enable ExperimentalSDL3GameOverNativeOwner" not in BACKEND
    gate=BACKEND.split("if(gameover_trial_enabled) {",1)[1].split(
        "if(!update_keys(desired,scans))",1)[0]
    assert "int candidate=gameover_trial_candidate(" in gate
    assert "int owner_confirm=candidate && gameover_native_owner_trial_enabled &&" in gate
    assert "gameover_native_owner_hook_installed &&" in gate
    assert "InterlockedCompareExchange(&gameover_native_owner_depth,0,0)>0" in gate
    assert "spider_sdl3_gameover_trial_step(&gameover_trial,owner_confirm," in gate
    assert "spider_sdl3_gameover_trial_step(&gameover_trial,candidate," not in gate
    assert "if(owner_confirm &&" in gate
    assert "if(trial==1u)" in gate
    assert "pulse(0x1cu" in gate
    # No passive owner probe can lower the source-verified confirmation gate.
    assert "gameover_owner_probe_" not in gate


def test_confirm_auto_install_never_bypasses_native_owner():
    """The enabled-by-default feature attempts only the source-backed
    owner hook. No unverified candidate can authorize Return."""
    init=BACKEND.split("void SpiderSDL3Universal_Init(",1)[1].split(
        "int SpiderSDL3Universal_Enabled(",1)[0]
    assert init.index("gameover_trial_enabled=1;") < (
        init.index("gameover_native_owner_trial_enabled=1;"))
    assert "if(requested && gameover_native_owner_trial_enabled)" in init
    assert "spider_gameover_owner_install_hook()" in init
    assert "experimental" in BACKEND.lower()
    gate=BACKEND.split("if(gameover_trial_enabled) {",1)[1].split(
        "if(!update_keys(desired,scans))",1)[0]
    assert "gameover_native_owner_hook_installed &&" in gate
    assert "InterlockedCompareExchange(&gameover_native_owner_depth,0,0)>0" in gate
    assert "spider_sdl3_gameover_trial_step(&gameover_trial,owner_confirm," in gate
    assert "spider_sdl3_gameover_trial_step(&gameover_trial,candidate," not in gate
    assert "if(gameover_trial_enabled)" in gate


def test_built_in_gameover_mode_preserves_exact_owner_and_neutral_state():
    assert "gameover_trial_enabled=1;" in BACKEND
    assert "gameover_native_owner_trial_enabled=1;" in BACKEND
    assert "if(requested && gameover_native_owner_trial_enabled)" in BACKEND
    assert "if(gameover_trial_enabled)" in BACKEND
    gate=BACKEND.split("if(gameover_trial_enabled) {",1)[1].split(
        "if(!update_keys(desired,scans))",1)[0]
    assert "owner_confirm=candidate && gameover_native_owner_trial_enabled &&" in gate
    assert "gameover_native_owner_hook_installed &&" in gate
    assert "InterlockedCompareExchange(&gameover_native_owner_depth,0,0)>0" in gate
    assert "spider_sdl3_gameover_trial_step(&gameover_trial,owner_confirm," in gate
    assert "spider_sdl3_gameover_trial_step(&gameover_trial,candidate," not in gate
    assert "pulse(0x1cu,\"native GAME OVER Return confirmation\")" in gate
    assert "ExperimentalSDL3GameOverConfirm=0" not in GENERATED
    assert "ExperimentalSDL3GameOverConfirm=1\\r\\n" not in GENERATED
