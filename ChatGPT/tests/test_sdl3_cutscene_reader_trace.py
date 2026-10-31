"""Fail-closed source contracts for optional native 3D reader attribution.

The actual original game binary is not available to these host tests.
Successful CI is not evidence that Cross's native skip reader is identified.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def test_debug_ini_flag_is_supported_and_opt_in_only():
    ini=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    native=(ROOT/"quick_exit_f12.c").read_text(encoding="utf-8")
    assert '"SDL3CutsceneReaderTrace"' in ini
    assert 'GetPrivateProfileIntA("Debug","SDL3CutsceneReaderTrace",0,ini)==1' in native
    for key in ("ExperimentalSDL3Probe", "ExperimentalSDL3NativeOwner",
                "ExperimentalSDL3ContextBridge"):
        assert f'GetPrivateProfileIntA("Input","{key}",0,ini)==1' in native
    assert 'GetPrivateProfileIntA("Debug","Log",0,ini)==1' in native
    assert "five original 0x50C180 callsites installed;" in native
    assert "REFUSED, original 3D input callsite signature mismatch" in native
    assert "static volatile LONG sdl3_cutscene_reader_trace=0;" in native


def test_original_native_reader_return_is_never_changed_and_has_bounded_logs():
    code=(ROOT/"quick_exit_f12.c").read_text(encoding="utf-8")
    section=code.split("static void spider_trace_sdl3_cutscene_reader(",1)[1].split(
        "/* The original 3D update at 0x455400",1)[0]
    wrapper=section.split("static BYTE __cdecl spider_probe_3d_native_input(",1)[1]
    trace=section.split("static BYTE __cdecl spider_probe_3d_native_input(",1)[0]
    assert "original=SPIDER_NATIVE_3D_INPUT(mask,arg1,arg2);" in wrapper
    assert "spider_trace_sdl3_cutscene_reader((DWORD)frame[1],mask,arg1,arg2," in wrapper
    assert "return original;" in wrapper
    assert "reports>=64u" in trace
    assert "context!=1u || active!=1u || ip==0u" in trace
    assert "VirtualQuery((LPCVOID)address,&mbi,sizeof(mbi))" in trace
    assert "mbi.Protect&(PAGE_NOACCESS|PAGE_GUARD)" in trace
    assert "SPIDER_NATIVE_CUTSCENE_SKIP_REQUEST" in trace
    assert "last_result[i]==(int)original" in trace
    assert "site=0x%08lX mask=0x%08lX" in trace
    assert "[ORIGINAL RESULT UNCHANGED; CORRELATION ONLY]" in trace
    assert "patch_native_call(" not in trace
    for prohibited in ("VirtualProtect(", "WriteProcessMemory(", "SendInput(",
                       "IDirectInputDevice8_Acquire(", "*SPIDER_NATIVE_CUTSCENE_SKIP_REQUEST="):
        assert prohibited not in trace+wrapper
    hook=code.split("static int install_debug_3d_native_input_probes(void) {",1)[1].split(
        "static void restore_debug_3d_native_input_probes(void)",1)[0]
    assert "spider_native_3d_input_original[i]" in hook
    assert "restore_intro_tick_call" in hook
    assert "InterlockedExchange(&debug_newgame_native_input_probes_installed,1);" in hook
