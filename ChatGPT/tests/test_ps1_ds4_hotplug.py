"""Sony DS4 reconnect handling is bounded, edge-safe and doesn't patch game input."""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
PAD=(ROOT/"gamepad_ui_profile.c").read_text()
POLICY=(ROOT/"ps1_ds4_hotplug.h").read_text()
CFG=(ROOT/"virtual_spidey_cfg.c").read_text()

def test_hotplug_integrates_on_foreground_flip_only():
    assert '#include "ps1_ds4_hotplug.h"' in PAD
    assert "spider_ds4_hotplug_scan_due(&hotplug," in PAD
    assert "spider_ds4_hotplug_attached(&hotplug)" in PAD
    assert "spider_ds4_hotplug_focus_lost(&hotplug)" in PAD
    assert "spider_ds4_hotplug_read_failed(&hotplug)" in PAD
    assert "spider_ds4_hotplug_accept_buttons(&hotplug,held)" in PAD
    assert "IDirectInput8_EnumDevices(di,DI8DEVCLASS_GAMECTRL" in PAD
    assert "DIEDFL_ATTACHEDONLY" in PAD
    assert "if(d.found!=1u || d.ds4_count!=1u)" in PAD
    assert "DISCL_FOREGROUND|DISCL_NONEXCLUSIVE" in PAD
    assert "if(GetForegroundWindow()!=hwnd || !IsWindowVisible(hwnd))" in PAD
    assert "release_active_key();" in PAD
    assert 'hotplug rescan every 2s while foreground' in PAD
    assert "disable_profile(NULL);" in PAD
    assert "ps1_preview_active=1;" in PAD
    assert "if(ps1_preview_requested && !ps1_preview_active)" in PAD
    assert "synthetic actions disabled until restart with controller present" in PAD
    # Existing startup-only verified native CFG reload must NOT happen in
    # OnFlip or from the physical reconnect path. No guessed memory writes.
    flip=PAD.split("void SpiderGamepadUI_OnFlip(",1)[1]
    assert "SpiderCfg_BootstrapGameRead" not in flip
    assert "SpiderCfg_EnablePS1Preview" not in flip
    for banned in ("SetWindowsHookEx", "VirtualProtect(", "WriteProcessMemory(",
                   "SendInput(", "Sleep("):
        assert banned not in POLICY

def test_c99_hotplug_timing_and_neutral_gate(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    source=tmp_path/"hotplug.c"
    exe=tmp_path/"hotplug"
    source.write_text(textwrap.dedent(r"""
    #include <assert.h>
    #include <stdint.h>
    #include "ps1_ds4_hotplug.h"
    int main(void) {
        SpiderDS4Hotplug h={0},e={0};
        unsigned i;
        /* initial scan, two-second delay, no scans while attached */
        assert(spider_ds4_hotplug_scan_due(&h,0,0));
        assert(!spider_ds4_hotplug_scan_due(&h,0,0));
        assert(!spider_ds4_hotplug_scan_due(&h,1999u,0));
        assert(spider_ds4_hotplug_scan_due(&h,2000u,0));
        assert(!spider_ds4_hotplug_scan_due(&h,9000u,1));
        assert(spider_ds4_hotplug_scan_due(&h,9000u,0));
        /* UINT32 wrap: unsigned subtraction is intentional. */
        e.scan_started=1; e.last_scan=0xffffff00u;
        assert(!spider_ds4_hotplug_scan_due(&e,0x00000500u,0));
        assert(spider_ds4_hotplug_scan_due(&e,0x00000800u,0));
        spider_ds4_hotplug_attached(&h);
        /* held Cross at reconnect cannot synthesize Return */
        assert(!spider_ds4_hotplug_accept_buttons(&h,2u));
        assert(!spider_ds4_hotplug_accept_buttons(&h,2u));
        assert(!spider_ds4_hotplug_accept_buttons(&h,0u));
        assert(spider_ds4_hotplug_accept_buttons(&h,2u));
        assert(spider_ds4_hotplug_accept_buttons(&h,0u));
        /* read errors immediately rearm neutral and detach only at 3 */
        assert(!spider_ds4_hotplug_read_failed(&h));
        assert(!spider_ds4_hotplug_read_failed(&h));
        assert(h.require_neutral);
        assert(!spider_ds4_hotplug_accept_buttons(&h,0u));
        assert(!h.read_failures);
        for(i=0;i<2;i++)assert(!spider_ds4_hotplug_read_failed(&h));
        assert(spider_ds4_hotplug_read_failed(&h));
        assert(spider_ds4_hotplug_read_failed(&h));
        spider_ds4_hotplug_attached(&h);
        assert(!spider_ds4_hotplug_accept_buttons(&h,0x800u));
        assert(!spider_ds4_hotplug_accept_buttons(&h,0u));
        assert(spider_ds4_hotplug_accept_buttons(&h,0x800u));
        spider_ds4_hotplug_focus_lost(&h);
        assert(!spider_ds4_hotplug_accept_buttons(&h,0x800u));
        assert(!spider_ds4_hotplug_accept_buttons(&h,0u));
        assert(spider_ds4_hotplug_accept_buttons(&h,0x800u));
        assert(!spider_ds4_hotplug_scan_due(0,0,0));
        assert(!spider_ds4_hotplug_read_failed(0));
        assert(!spider_ds4_hotplug_accept_buttons(0,0));
        return 0;
    }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
                    str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_unplug_inputlost_never_acquires_disconnected_device_same_frame():
    # Exact regression for the maintainer's crash log: after physical USB
    # detach the last trace was GetDeviceState hr=0x8007001E, and legacy
    # code immediately called Acquire(device) on the stale handle.
    assert "if(hr==DIERR_INPUTLOST || repeatedly_failed)" in PAD
    assert "spider_hotplug_drop_device(hr," in PAD
    assert "secondary device release BEGIN (no Acquire on failed read)" in PAD
    assert "secondary device release COMPLETE; rescan deferred 2s" in PAD
    assert "NO stale Acquire inside PAUSED" in PAD
    assert "pause_wake_active=0; /* stop polling a lost secondary device until Flip */" in PAD
    assert "disable_profile_with_policy(NULL,hr==DIERR_INPUTLOST);" in PAD
    assert "secondary stale device Release BEGIN (skip Unacquire after INPUTLOST)" in PAD
    assert "secondary stale device Release COMPLETE" in PAD
    failed=PAD[PAD.index("hr=IDirectInputDevice8_GetDeviceState(device,sizeof(state),&state);",
                         PAD.index("void SpiderGamepadUI_OnFlip(")):]
    failed=failed[:failed.index("bad_reads=0;")]
    assert "IDirectInputDevice8_Acquire(device);" not in failed
    paused=PAD[PAD.index("unsigned SpiderGamepadUI_PauseWake_Poll("):
               PAD.index("void SpiderGamepadUI_PauseWake_End(")]
    assert "IDirectInputDevice8_Acquire(device);" not in paused
    # Reacquisition must open a fresh instance during scheduled rediscovery,
    # not silently reload the game's native PS1 profile in mid-game.
    assert "IDirectInput8_CreateDevice(di,&d.ds4_instance,&device,NULL)" in PAD
    assert "hotplug.last_scan=GetTickCount();" in PAD
