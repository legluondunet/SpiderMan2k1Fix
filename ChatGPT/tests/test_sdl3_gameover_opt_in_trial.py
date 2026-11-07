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
    assert BACKEND.index("if(gameover_trial_enabled || gameover_owner_probe_enabled) {") < BACKEND.index(
        'if(!update_keys(desired,scans))')
    assert "if(gameover_trial_enabled)\n      trial=spider_sdl3_gameover_trial_step(" in BACKEND
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


def test_owner_observer_is_read_only_and_not_a_game_over_detector():
    # The newly traced script manager is known from native disassembly,
    # but has NOT been proven to be the owner of the GAME OVER screen.
    # It must never gate a new Return action or mutate engine memory.
    owner=BACKEND.split("static void gameover_owner_probe_dump(",1)[1].split(
        "static void gameover_probe_snapshot(",1)[0]
    for address in (
        "0x0060CFA4u", "0x0054A510u", "0x006B78F0u",
        "0x006A9038u", "0x00682968u", "0x00682960u",
        "0x02E0988Cu", "0x006B4CA0u",
    ):
        assert address in owner
    for offset in ("0x1ACu", "0x1B4u", "0x1B8u", "0x1A4u"):
        assert offset in owner
    assert "gameover_probe_read_u32(" in owner
    assert "script>=0x10000u && script<=0x7fff0000u" in owner
    assert "script-ok=%u" in owner
    assert "[PASSIVE; NO CONFIRMED GAME-OVER OWNER]" in owner
    assert "gameover_owner_probe_events>=32u" in owner
    assert "if(!gameover_owner_probe_enabled || !report ||" in owner
    assert "report(msg);" in owner
    for forbidden in (
        "SendInput(", "send_scan(", "update_keys(", "VirtualProtect(",
        "WriteProcessMemory(", "InterlockedExchangePointer(",
        "pulse(", "gameover_trial_enabled=1",
    ):
        assert forbidden not in owner, forbidden


def test_owner_probe_does_not_modify_gameover_trial_or_3d_action_routing():
    assert 'gameover_owner_probe_dump("3D-BASELINE",id,context,0);' in BACKEND
    assert 'gameover_owner_probe_dump("CANDIDATE-ENTER"' in BACKEND or (
        '"CANDIDATE-ENTER"' in BACKEND)
    assert '"CANDIDATE-EXIT"' in BACKEND
    assert '"CANDIDATE-CONTEXT-EXIT"' in BACKEND
    assert '"CANDIDATE-STABLE"' in BACKEND
    assert '"SOUTH-BEFORE-PULSE"' in BACKEND
    assert '"SELECTED-PAD-CHANGE"' in BACKEND
    assert "gameover_owner_probe_selected_id!=id" in BACKEND
    assert "gameover_owner_probe_selected_id=0;" in BACKEND
    assert "gameover_owner_probe_stable_frames=0;" in BACKEND
    assert "stable_probe_before<SPIDER_GAMEOVER_TRIAL_STABLE_FRAMES" in BACKEND
    assert '"ExperimentalSDL3GameOverOwnerProbe"' in BACKEND
    assert "gameover_owner_probe_enabled=ini &&" in BACKEND
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3GameOverOwnerProbe",0,ini)==1' in BACKEND
    assert "gameover_owner_probe_was_candidate=0;" in BACKEND
    assert "gameover_owner_probe_baselines<8u" in BACKEND
    assert "gameover_owner_probe_frames%180u==0u" in BACKEND
    assert "gameover_owner_probe_events=0;" in BACKEND
    # The original opt-in gate and one-shot Space suppression stay
    # unchanged. No automatic/native menu ownership is claimed.
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3GameOverConfirm",0,ini)==1' in BACKEND
    assert "gameover_trial_candidate(context,menu,gameplay,bink,native_pause)" in BACKEND
    assert 'pulse(0x1cu,"opt-in GAME OVER Return trial");' in BACKEND
    assert "if(!update_keys(desired,scans))" in BACKEND
    assert "if(pause_edge)pulse(0x01u,\"3D Pause\");" in BACKEND


def test_passive_owner_probe_is_independent_of_enter_injection():
    """Enabling passive state sampling must not implicitly enable the
    separately tested experimental GAME OVER Enter action."""
    validation=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    generated=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
    assert '"ExperimentalSDL3GameOverOwnerProbe"' in validation
    assert "ExperimentalSDL3GameOverOwnerProbe=1" not in generated
    assert "ExperimentalSDL3GameOverConfirm=1" not in generated
    assert "if(gameover_trial_enabled || gameover_owner_probe_enabled)" in BACKEND
    assert "if(gameover_trial_enabled)\n      trial=spider_sdl3_gameover_trial_step(" in BACKEND
    assert "if(trial) {" in BACKEND
    assert "if(trial==1u)" in BACKEND
    assert "gameover_owner_probe_dump(" in BACKEND
    assert "SDL3-GAMEOVER-OWNER-PROBE: passive native state observation enabled" in BACKEND
    assert "gameover_owner_probe_stable_frames" in BACKEND


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


def test_strict_owner_is_separate_opt_in_and_fails_closed():
    assert '"ExperimentalSDL3GameOverNativeOwner"' in (
        ROOT/"ini_validation.h"
    ).read_text()
    assert "ExperimentalSDL3GameOverNativeOwner=1" not in (
        ROOT/"virtual_spidey_cfg.c"
    ).read_text()
    assert '"ExperimentalSDL3GameOverNativeOwner",0,ini)==1' in BACKEND
    assert "gameover_native_owner_trial_enabled=gameover_trial_enabled ||" in BACKEND
    assert "secure native GAME OVER owner hook mandatory" in BACKEND
    assert "if(requested && gameover_native_owner_trial_enabled)" in BACKEND
    assert "int installed=spider_gameover_owner_install_hook();" in BACKEND
    assert "SDL3-GAMEOVER-NATIVE: hook REFUSED" in BACKEND
    assert "int owner_confirm=candidate && gameover_native_owner_trial_enabled &&" in BACKEND
    assert "gameover_native_owner_hook_installed &&" in BACKEND
    assert "InterlockedCompareExchange(&gameover_native_owner_depth,0,0)>0;" in BACKEND
    assert "trial=spider_sdl3_gameover_trial_step(&gameover_trial,owner_confirm," in BACKEND
    assert "if(owner_confirm &&" in BACKEND
    assert 'gameover_owner_probe_dump("SOUTH-BEFORE-PULSE",id,context,owner_confirm);' in BACKEND
    assert "if(gameover_trial_enabled)" in BACKEND
    assert 'pulse(0x1cu,"opt-in GAME OVER Return trial");' in BACKEND


def test_no_legacy_fallback_in_current_main_gameover_runtime():
    """Even an omitted owner flag cannot activate the unsafe request-2
    fallback: Confirm=1 ALWAYS enables the authenticated hook attempt."""
    assert "gameover_native_owner_trial_enabled=gameover_trial_enabled ||" in BACKEND
    assert "if(requested && gameover_native_owner_trial_enabled)" in BACKEND
    assert "spider_gameover_owner_install_hook()" in BACKEND
    assert "SDL3-GAMEOVER-NATIVE: hook REFUSED" in BACKEND
    assert "CONFIRM BLOCKED: enable ExperimentalSDL3GameOverNativeOwner" not in BACKEND
    gate=BACKEND.split("if(gameover_trial_enabled || gameover_owner_probe_enabled) {",1)[1].split(
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
    # Passive evidence still sees the raw seven-word candidate but it
    # never changes owner_confirm or enables a synthetic key.
    assert 'gameover_owner_probe_dump("CANDIDATE-STABLE"' in gate
    assert "gameover_owner_probe_was_candidate=candidate;" in gate


def test_confirm_auto_install_never_bypasses_native_owner():
    """A single user opt-in is enough to try the source-verified hook,
    while the menu owner remains mandatory for any synthetic Enter."""
    init=BACKEND.split("void SpiderSDL3Universal_Init(",1)[1].split(
        "int SpiderSDL3Universal_Enabled(",1)[0]
    assert init.index('GetPrivateProfileIntA("Input","ExperimentalSDL3GameOverConfirm",0,ini)==1') < (
        init.index("gameover_native_owner_trial_enabled=gameover_trial_enabled ||"))
    assert "if(requested && gameover_native_owner_trial_enabled)" in init
    assert "spider_gameover_owner_install_hook()" in init
    assert "experimental" in BACKEND.lower()
    gate=BACKEND.split("if(gameover_trial_enabled || gameover_owner_probe_enabled) {",1)[1].split(
        "if(!update_keys(desired,scans))",1)[0]
    assert "gameover_native_owner_hook_installed &&" in gate
    assert "InterlockedCompareExchange(&gameover_native_owner_depth,0,0)>0" in gate
    assert "spider_sdl3_gameover_trial_step(&gameover_trial,owner_confirm," in gate
    assert "spider_sdl3_gameover_trial_step(&gameover_trial,candidate," not in gate
    assert "if(gameover_trial_enabled)" in gate
