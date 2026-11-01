"""First universal SDL3 milestone: raw hardware discovery and GUID mapping wizard.

The tests verify portable mapping rules and guard the Win32 integration.
Hardware SDL joystick events and in-game command delivery need runtime testing.
"""
import shutil
import subprocess
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_raw_guid_mapping_policy_compiles_and_rejects_invalid_or_duplicate_inputs(tmp_path):
    cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    src = tmp_path / "raw.c"
    binary = tmp_path / "raw"
    src.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <stdio.h>
        #include <string.h>
        #include "sdl3_raw_mapping_policy.h"

        static SpiderRawBinding b(unsigned kind,unsigned index,unsigned hat) {
          SpiderRawBinding result={(unsigned char)kind,(unsigned char)index,
                                   (unsigned char)hat};
          return result;
        }
        int main(void) {
          SpiderRawBinding slots[SPIDER_RAW_BIND_COUNT]={{0}};
          char text[1024];
          unsigned step;
          /* Reject a missing required binding, duplicated button or axis,
             inappropriate hat and unsafe GUID. */
          assert(!spider_raw_bind_assign(slots,0,b(0,0,0)));
          assert(spider_raw_bind_assign(slots,0,b(SPIDER_RAW_BUTTON,0,0)));
          assert(!spider_raw_bind_assign(slots,1,b(SPIDER_RAW_BUTTON,0,0)));
          assert(!spider_raw_bind_assign(slots,1,b(SPIDER_RAW_HAT,0,3)));
          for(step=1;step<=7;step++)
            assert(spider_raw_bind_assign(slots,step,
                       b(SPIDER_RAW_BUTTON,step,0)));
          assert(spider_raw_bind_assign(slots,8,b(SPIDER_RAW_AXIS,0,0)));
          assert(spider_raw_bind_assign(slots,9,b(SPIDER_RAW_AXIS,1,0)));
          assert(!spider_raw_bind_assign(slots,10,b(SPIDER_RAW_AXIS,1,0)));
          assert(spider_raw_bind_assign(slots,13,b(SPIDER_RAW_AXIS,5,0)));
          assert(spider_raw_bind_assign(slots,14,b(SPIDER_RAW_HAT,0,1)));
          assert(spider_raw_bind_assign(slots,15,b(SPIDER_RAW_HAT,0,4)));
          assert(!spider_raw_mapping_text("bad", "Test",slots,text,sizeof(text)));
          assert(spider_raw_mapping_text(
              "341a3608000000000000504944564944",
              "Generic, Controller",slots,text,sizeof(text)));
          assert(strstr(text,",Generic_ Controller,"));
          assert(strstr(text,"a:b0,"));
          assert(strstr(text,"start:b7,"));
          assert(strstr(text,"leftx:a0,"));
          assert(strstr(text,"lefty:a1,"));
          assert(strstr(text,"righttrigger:a5,"));
          assert(strstr(text,"dpup:h0.1,"));
          assert(strstr(text,"dpdown:h0.4"));
          assert(text[strlen(text)-1]!=',');
          /* Optional controls can remain unbound. */
          assert(spider_raw_bind_assign(slots,16,b(0,0,0)));
          /* Never reuse a captured raw button for an unrelated action. */
          assert(!spider_raw_bind_assign(slots,19,b(SPIDER_RAW_BUTTON,7,0)));
          return 0;
        }
    """), encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)


def test_raw_mapping_is_integrated_without_native_game_input_hooks():
    raw=(ROOT/"sdl3_raw_mapping.c").read_text(encoding="utf-8")
    setup=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    build=(ROOT/"build.sh").read_text(encoding="utf-8")
    assert "sdl3_raw_mapping.c" in build
    assert "SpiderSDL3Raw_Init(module,report);" in probe
    assert "SpiderSDL3Raw_Recognize(hwnd);" in setup
    assert "Recognize unknown controller (keyboard)" in setup
    for symbol in ("SDL_GetJoysticks","SDL_IsGamepad","SDL_OpenJoystick",
                   "SDL_GetJoystickButton","SDL_GetJoystickAxis",
                   "SDL_GetJoystickHat","SDL_GetJoystickGUIDForID",
                   "SDL_GUIDToString","SDL_AddGamepadMapping"):
        assert symbol in raw
    assert "SDL3-gamepad-mappings.txt" in raw
    assert "MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH" in raw
    assert "SDL3-RAW: loaded" in raw
    assert "SDL3-RAW: GUID" in raw
    assert "IsDialogMessageA" in raw
    for forbidden in ("VirtualProtect(", "WriteProcessMemory(",
                      "IDirectInputDevice8_", "spider_ps1_write_word(",
                      "SetWindowsHookEx(", "SetWindowLongPtrA(owner,"):
        assert forbidden not in raw
