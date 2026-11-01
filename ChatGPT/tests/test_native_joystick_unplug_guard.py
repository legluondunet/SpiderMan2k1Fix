"""First-stage x86 game-native DIERR_UNPLUGGED mitigation guard tests.

These assert the safe logical policy and the original PE opcode gates.
Real DirectInput unplug/replug under Wine and Windows require hardware tests.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
SOURCE=(ROOT/"native_joystick_unplug.c").read_text()
HEADER=(ROOT/"native_joystick_unplug_policy.h").read_text()
MAIN=(ROOT/"spiderman2k1fix.c").read_text()
INI=(ROOT/"ini_validation.h").read_text()
TEMPLATE=(ROOT/"virtual_spidey_cfg.c").read_text()
BUILD=(ROOT/"build.sh").read_text()


def test_portable_error_gate_suppresses_unplug_only(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    source=tmp_path/"gate.c"
    program=tmp_path/"gate"
    source.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <stdint.h>
        #include "native_joystick_unplug_policy.h"
        int main(void) {
          assert(spider_native_unplug_recoverable(0x80040209u));
          assert(!spider_native_unplug_recoverable(0x8007001Eu));
          assert(!spider_native_unplug_recoverable(0x8007000Cu));
          assert(!spider_native_unplug_recoverable(0x80070005u));
          assert(!spider_native_unplug_recoverable(0xC0000005u));
          assert(!spider_native_unplug_recoverable(0x00000000u));
          assert(SPIDER_NATIVE_JOYSTICK_BUTTON_BYTES==32u);
          return 0;
        }
    """))
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(program)],check=True)
    subprocess.run([str(program)],check=True)


def test_original_native_x86_bytes_signature_pinned():
    # Extracted from the maintainer-provided, SHA-256 checked retail EXE;
    # no copyrighted game executable is committed to the repository.
    instructions={
        "SPIDER_SITE_CMP": "0x00501EA6u",
        "SPIDER_ORIGINAL_BRANCH": "0x00501EACu",
        "SPIDER_GRACEFUL_RETURN": "0x00501EBBu",
        "SPIDER_GAME_BUTTON_STATES": "0x006B7A34u",
        "poll": "{0xFF,0x51,0x64},3",
        "lost": "{0x3D,0x1E,0x00,0x07,0x80},5",
        "not_acquired": "{0x3D,0x0C,0x00,0x07,0x80},5",
        "native_acquire": "{0x8B,0x10,0xFF,0x52,0x1C},5",
        "original_cmp": "{0x81,0xFE,0x05,0x00,0x07,0x80},6",
        "original_branch": "{0x75,0x18},2",
        "return": "{0x33,0xC0,0x5F},3",
        "exit": "{0xE8,0x15,0x82,0x02,0x00},5",
    }
    for name,value in instructions.items():
        assert value in SOURCE, (name,value)
    assert "SPIDER_EXE_BASE 0x00400000u" in SOURCE
    assert "sizeof(void*)!=4" in SOURCE
    assert "memcmp((const void *)signatures[i].addr" in SOURCE
    assert "if(!exact_supported_site())" in SOURCE
    assert "memcmp((void *)(uintptr_t)SPIDER_SITE_CMP,original_cmp,6)" in SOURCE


def test_stub_preserves_all_other_hresult_error_paths():
    assert "append32(&p,SPIDER_DIERR_UNPLUGGED);" in SOURCE
    assert "memcpy(p,original_cmp,sizeof(original_cmp))" in SOURCE
    assert "jmp32(&p,SPIDER_ORIGINAL_BRANCH);" in SOURCE
    assert "jmp32(&p,SPIDER_GRACEFUL_RETURN);" in SOURCE
    assert "SPIDER_NATIVE_JOYSTICK_BUTTON_BYTES/4u" in SOURCE
    assert "retained_stub=page;" in SOURCE
    assert "VirtualProtect(page,4096,PAGE_EXECUTE_READ,&old)" in SOURCE
    assert "FlushInstructionCache(GetCurrentProcess()" in SOURCE
    assert "InterlockedExchange(&installed,1);" in SOURCE
    assert "lock inc [counter]" in SOURCE
    # No game-owned COM vtable hooks, no reset of unrelated game state,
    # no global process-exit interception or forced success HRESULTs.
    for banned in ("ExitProcess(", "TerminateProcess(", "SetWindowsHookEx(",
                   "IDirectInputDevice8_Acquire(", "IDirectInput8_CreateDevice("):
        assert banned not in SOURCE
    assert "GetPrivateProfileIntA(\"Input\"," in SOURCE


def test_ini_and_build_integration_opt_in_only():
    assert 'ExperimentalNativeJoystickUnplugGuard=0' in TEMPLATE
    assert '"ExperimentalNativeJoystickUnplugGuard"' in INI
    assert '"ExperimentalNativeJoystickUnplugGuard",0,ini)==1' in SOURCE
    assert "#include \"native_joystick_unplug.h\"" in MAIN
    assert MAIN.index("if (!supported_exe())") < MAIN.index("SpiderNativeUnplug_Init(ini,log_line);")
    assert "SpiderNativeUnplug_OnFlip(count);" in MAIN
    assert "if(flip_index==120)install_guard();" in SOURCE
    assert "native_joystick_unplug.c" in BUILD
    assert "native rebind NOT implemented" in SOURCE


def test_displacement_math_uses_live_in_process_patch_address():
    # This is critical: compute JMP displacement from the actual native
    # 0x501EA6 patch address, never from the temporary patch[] array.
    assert "displacement=(uint32_t)(uintptr_t)page-(SPIDER_SITE_CMP+5u);" in SOURCE
    assert "memcpy(patch+1,&displacement,4);" in SOURCE
    assert "uint32_t disp=(uint32_t)target - ((uint32_t)(uintptr_t)field+4u);" in SOURCE
