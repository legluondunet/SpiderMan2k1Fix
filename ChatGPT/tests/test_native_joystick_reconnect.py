"""Conservative stage-2 native DS4 rebind after stage-1 unplug guard."""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
SOURCE=(ROOT/"native_joystick_reconnect.c").read_text()
POLICY=(ROOT/"native_joystick_reconnect_policy.h").read_text()
GUARD=(ROOT/"native_joystick_unplug.c").read_text()
GUARD_H=(ROOT/"native_joystick_unplug.h").read_text()
MAIN=(ROOT/"spiderman2k1fix.c").read_text()
BUILD=(ROOT/"build.sh").read_text()
VALIDATE=(ROOT/"ini_validation.h").read_text()
TEMPLATE=(ROOT/"virtual_spidey_cfg.c").read_text()


def test_reconnect_bounded_timer_and_strict_ds4_vid_pid(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc: return
    src=tmp_path/"policy.c"
    exe=tmp_path/"policy"
    src.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <stdint.h>
        #include "native_joystick_reconnect_policy.h"
        int main(void) {
          assert(spider_reconnect_sony_ds4(0x054c,0x09cc));
          assert(spider_reconnect_sony_ds4(0x054c,0x05c4));
          assert(spider_reconnect_sony_ds4(0x054c,0x0ba0));
          assert(!spider_reconnect_sony_ds4(0x045e,0x09cc));
          assert(!spider_reconnect_sony_ds4(0x054c,0x0001));
          assert(spider_reconnect_xbox360(0x045e,0x028e)); /* Holtek sample */
          assert(spider_reconnect_kind(0x045e,0x028e)==SPIDER_RECONNECT_XBOX360);
          assert(spider_reconnect_kind(0x054c,0x09cc)==SPIDER_RECONNECT_DS4);
          assert(spider_reconnect_kind(0x045e,0x0719)==SPIDER_RECONNECT_NONE);
          assert(spider_reconnect_kind(0x045e,0xffff)==SPIDER_RECONNECT_NONE);
          assert(spider_reconnect_kind(0x046d,0x028e)==SPIDER_RECONNECT_NONE);
          assert(spider_reconnect_scan_due(100,0,0));
          assert(!spider_reconnect_scan_due(2099,100,1));
          assert(spider_reconnect_scan_due(2100,100,1));
          assert(!spider_reconnect_scan_due(0x400,0xffffff00u,1));
          assert(spider_reconnect_scan_due(0x900,0xffffff00u,1));
          assert(spider_reconnect_eligible(1,1,1,1));
          assert(!spider_reconnect_eligible(0,1,1,1));
          assert(!spider_reconnect_eligible(1,0,1,1));
          assert(!spider_reconnect_eligible(1,1,0,1));
          assert(!spider_reconnect_eligible(1,1,1,0));
          return 0;
        }
    """))
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_original_game_owner_interface_not_parallel_factory():
    # Must re-use ORIGINAL game factory rather than replacing its vtable
    # or open an independent DirectInput factory that the game never reads.
    for x in (
        "SPIDER_NATIVE_FACTORY 0x006B7A30u",
        "SPIDER_NATIVE_DEVICE 0x006B7A2Cu",
        "SPIDER_NATIVE_HWND 0x006B7A60u",
        "SPIDER_NATIVE_FORMAT 0x0053D0F4u",
        "SPIDER_NATIVE_BUTTON_COUNT 0x006B7A58u",
        "SPIDER_NATIVE_POLLED_FLAG 0x006B7928u",
        "IDirectInput8_EnumDevices(di,DI8DEVCLASS_GAMECTRL",
        "IDirectInput8_CreateDevice(di,&list.instance,&candidate,NULL)",
        "IDirectInputDevice8_SetCooperativeLevel(device,hwnd,",
        "DISCL_FOREGROUND|DISCL_NONEXCLUSIVE",
        "(const DIDATAFORMAT *)(uintptr_t)SPIDER_NATIVE_FORMAT",
        "DIPROP_BUFFERSIZE", "DIPROP_AXISMODE", "DIPROP_RANGE",
        "DIPH_BYOFFSET", "set_native_range(device,0u)",
        "set_native_range(device,4u)",
        "IDirectInputDevice8_GetCapabilities(device,caps)",
        "IDirectInputDevice8_Acquire(device)",
        "IDirectInputDevice8_GetDeviceData(device,sizeof(DIDEVICEOBJECTDATA)",
    ):
        assert x in SOURCE, x
    assert "list.count!=1u" in SOURCE
    for forbidden in ("DirectInput8Create(", "WriteProcessMemory(",
                      "SetWindowsHookEx(",
                      "IDirectInput8_Release(", "ExitProcess(", "TerminateProcess("):
        assert forbidden not in SOURCE, forbidden


def test_stage_1_observed_before_rebinding_and_keyboard_untouched():
    assert "SpiderNativeUnplug_Events(void)" in GUARD_H
    assert "SpiderNativeUnplug_Installed(void)" in GUARD_H
    assert "SpiderNativeUnplug_Events(void)" in GUARD
    assert "SpiderNativeUnplug_Installed(void)" in GUARD
    assert "events=SpiderNativeUnplug_Events();" in SOURCE
    assert "if(!SpiderNativeUnplug_Installed())return;" in SOURCE
    assert "detach_stale_native();" in SOURCE
    assert "InterlockedCompareExchangePointer((PVOID volatile *)game_device_slot(),NULL,old)" in SOURCE
    assert "IDirectInputDevice8_Release(old);" in SOURCE
    assert "zero_native_buttons();" in SOURCE
    assert "GetForegroundWindow()!=game_hwnd" in SOURCE
    assert "GetCurrentThreadId()" in SOURCE
    assert "game_device_slot(),\n                                              candidate,NULL" in SOURCE
    assert "neutral_and_flush(candidate)" in SOURCE
    assert "state.rgbButtons[i]&0x80u" in SOURCE
    assert "if(FAILED(hr))return 0;" in SOURCE
    assert "scan_started=1;" in SOURCE
    assert "SPIDER_RECONNECT_INTERVAL_MS 2000u" in POLICY
    assert "NATIVE-RECONNECT: REBOUND original game-owned DirectInput joystick" in SOURCE


def test_startup_ini_gating_and_build_wiring():
    assert '"ExperimentalNativeJoystickReconnect",0,ini)==1' in SOURCE
    assert '"ExperimentalNativeJoystickUnplugGuard",0,ini)!=1' in SOURCE
    assert "ExperimentalNativeJoystickReconnect=0" in TEMPLATE
    assert '"ExperimentalNativeJoystickReconnect"' in VALIDATE
    assert 'native_joystick_reconnect.c' in BUILD
    assert '#include "native_joystick_reconnect.h"' in MAIN
    assert "SpiderNativeReconnect_Init(ini,log_line)" in MAIN
    assert "SpiderNativeReconnect_OnFlip(count,game_hwnd)" in MAIN
    assert MAIN.index("SpiderNativeUnplug_OnFlip(count)") < MAIN.index("SpiderNativeReconnect_OnFlip(count,game_hwnd)")
    assert "if(!requested || !initialized || flip_index<120)return;" in SOURCE
    assert "valid_binary_layout()" in SOURCE
    assert "memcmp((const void *)(uintptr_t)SPIDER_NATIVE_FORMAT,original_format" in SOURCE


def test_original_exe_signatures_and_nonintrusive_candidate_preparation():
    assert "0x00501890u" in SOURCE
    assert "0x00501AFBu" in SOURCE
    assert "0x00501E6Du" in SOURCE
    assert "SPIDER_NATIVE_BASE 0x00400000u" in SOURCE
    assert "if(sizeof(void*)!=4" in SOURCE
    assert "0xB0,0x77,0x51,0x00" in SOURCE
    assert "DIJOYSTATE" in SOURCE
    assert "0x005019E0" in SOURCE
    assert "0x00501A42" in SOURCE
    assert "set_native_range(device,16u)" not in SOURCE
    assert SOURCE.index("set_native_range(device,0u)") < SOURCE.index("set_native_range(device,4u)")
    assert "memcmp((const void *)(uintptr_t)0x00501AFBu,original_create" in SOURCE
    assert "game factory" in SOURCE.lower() or "GAME'S factory" in SOURCE



def test_post_rebind_menu_input_breadcrumbs_are_passive_and_bounded():
    ui=(ROOT/"gamepad_ui_profile.c").read_text()
    header=(ROOT/"native_joystick_reconnect.h").read_text()
    assert "void SpiderNativeReconnect_LogSyntheticMenuKey(unsigned scan, const char *phase);" in header
    assert "void SpiderNativeReconnect_LogSyntheticMenuKey(unsigned scan,const char *phase)" in SOURCE
    assert "if(!requested || !initialized || rebound==0)return;" in SOURCE
    assert "if(reported_generation!=rebound)" in SOURCE
    assert "reported=0;" in SOURCE
    assert "if(reported>=40u)return;" in SOURCE
    assert "native_thread!=GetCurrentThreadId()" in SOURCE
    assert "SPIDER_NATIVE_FORCE_FEEDBACK 0x006B7A68u" in SOURCE
    assert "NATIVE-RECONNECT: POST-REBIND INPUT" in SOURCE
    assert 'SpiderNativeReconnect_LogSyntheticMenuKey(scan,"before KEYDOWN");' in ui
    assert 'SpiderNativeReconnect_LogSyntheticMenuKey(scan,"after KEYDOWN");' in ui
    assert 'SpiderNativeReconnect_LogSyntheticMenuKey(active_scan,"before KEYUP");' in ui
    assert 'SpiderNativeReconnect_LogSyntheticMenuKey(active_scan,"after KEYUP");' in ui
    assert ui.index('SpiderNativeReconnect_LogSyntheticMenuKey(scan,"before KEYDOWN");') < ui.index('if(SendInput(1,&in,sizeof(in))!=1) {', ui.index('static void begin_scancode'))
    assert "IDirectInputDevice8_GetDeviceData" not in SOURCE.split("void SpiderNativeReconnect_LogSyntheticMenuKey(",1)[1].split("void SpiderNativeReconnect_Init(",1)[0]
    assert "IDirectInputDevice8_Release" not in SOURCE.split("void SpiderNativeReconnect_LogSyntheticMenuKey(",1)[1].split("void SpiderNativeReconnect_Init(",1)[0]


def test_post_rebind_health_heartbeat_is_passive_generation_aware_and_bounded():
    header=(ROOT/"native_joystick_reconnect.h").read_text()
    assert "void SpiderNativeReconnect_ObserveMode(long flip_index, int menu_2d," in header
    assert "void SpiderNativeReconnect_ObserveMode(long flip_index,int menu_2d," in SOURCE
    assert "SpiderNativeReconnect_ObserveMode(count," in MAIN
    assert "observed_rebound==rebound" in SOURCE
    assert "(flip_index-last_flip)<300" in SOURCE
    assert "reports>=80u" in SOURCE
    assert "NATIVE-RECONNECT: HEALTH" in SOURCE
    assert 'mode==2?"3D"' in SOURCE
    assert "joystick=InterlockedCompareExchangePointer(" in SOURCE
    assert "ffb=InterlockedCompareExchangePointer(" in SOURCE
    block=SOURCE.split("void SpiderNativeReconnect_ObserveMode(",1)[1].split("void SpiderNativeReconnect_Init(",1)[0]
    for forbidden in ("IDirectInputDevice8_", "IDirectInput8_", "VirtualProtect(",
                      "Release(", "SetProperty(", "WriteProcessMemory("):
        assert forbidden not in block
    assert "if(!requested || !initialized || !SpiderNativeUnplug_Installed()" in block


def test_xbox360_native_recovery_and_separately_opted_in_ds4_ps1_remap():
    ui=(ROOT/"gamepad_ui_profile.c").read_text()
    assert "spider_reconnect_kind(vid,pid)" in SOURCE
    assert "enumerate_supported_pad" in SOURCE
    assert 'list.kind==SPIDER_RECONNECT_DS4?"Sony DS4":"Xbox360-compatible"' in SOURCE
    assert "LOWORD(list.product),HIWORD(list.product)" in SOURCE
    assert "list.count!=1u" in SOURCE  # refuse ambiguous DS4+Xbox
    assert "spider_reconnect_xbox360" in POLICY
    assert "pid==0x028eu" in POLICY
    assert "SPIDER_DS4_VID" in POLICY
    assert "static int ds4_pid(" in ui
    assert "SPIDER_PROFILE_DUALSHOCK4" in ui
    assert "SPIDER_PROFILE_XBOX360" in ui
    assert "detected_kind!=SPIDER_RECONNECT_DS4 || !ps1_preview_requested" in ui
    assert "if(detected && detected_kind==SPIDER_RECONNECT_XBOX360)" in ui
