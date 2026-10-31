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


def test_second_passive_owner_boundary_probe_is_noninvasive():
    native=(ROOT/"quick_exit_f12.c").read_text(encoding="utf-8")
    section=native.split(
        "static void spider_trace_sdl3_native_cutscene_boundary(",1)[1].split(
        "/* Native game-input owner wrapper at 0x45553B:",1)[0]
    assert "reports>=80u" in section
    assert "VirtualQuery((LPCVOID)address,&mbi,sizeof(mbi))" in section
    assert "mbi.Protect&(PAGE_NOACCESS|PAGE_GUARD)" in section
    assert "obj[0x1acu]" in section and "obj[0x1a4u]" in section
    assert "obj[0x1b4u]" in section and "address+0x1b8u" in section
    assert "request=*SPIDER_NATIVE_CUTSCENE_SKIP_REQUEST;" in section
    assert "last_snapshot[after]==packed" in section
    assert "SDL3-CUTSCENE-OWNER: boundary=%s" in section
    assert "ORIGINAL NATIVE UPDATE UNCHANGED; CORRELATION ONLY" in section
    for mutation in ("VirtualProtect(", "WriteProcessMemory(", "SendInput(",
                     "IDirectInputDevice8_Acquire(",
                     "*SPIDER_NATIVE_CUTSCENE_SKIP_REQUEST="):
        assert mutation not in section
    owner=native.split(
        "static void __cdecl spider_observe_native_game_input(void) {",1)[1].split(
        "/* PASSIVE, exact-site 3D input diagnostics",1)[0]
    a=owner.find("spider_trace_sdl3_native_cutscene_boundary(0u);")
    b=owner.find("SPIDER_NATIVE_GAME_INPUT_UPDATE();",a)
    c=owner.find("spider_trace_sdl3_native_cutscene_boundary(1u);",b)
    assert 0<=a<b<c
    assert owner.count("SPIDER_NATIVE_GAME_INPUT_UPDATE();")==1
    plan=(ROOT/"sdl3_gameplay_plan_probe.c").read_text(encoding="utf-8")
    assert "LT=%d RT=%d ms=%lu [NO NATIVE INJECTION]" in plan
    assert "(unsigned long)GetTickCount());" in plan


def test_selected_physical_cross_square_edge_requires_neutral_owner():
    native=(ROOT/"quick_exit_f12.c").read_text(encoding="utf-8")
    header=(ROOT/"quick_exit_f12.h").read_text(encoding="utf-8")
    plan=(ROOT/"sdl3_gameplay_plan_probe.c").read_text(encoding="utf-8")
    section=native.split(
        "void SpiderQuickExitF12_CutscenePhysicalSample(",1)[1].split(
        "void SpiderQuickExitF12_AttachWindow(",1)[0]
    assert "SpiderQuickExitF12_CutscenePhysicalSample(" in header
    assert "physical_buttons&0x05u" in section
    assert "selected_owner_valid" in section
    assert "sdl3_cutscene_sample_armed" in section
    assert "if(!wanted)InterlockedExchange(&sdl3_cutscene_sample_armed,1);" in section
    assert "fresh=wanted&~previous;" in section
    assert "sdl3_cutscene_edge_bits,fresh" in section
    assert "sdl3_cutscene_edge_ms,(LONG)now" in section
    assert "SDL3-CUTSCENE-EDGE:" in section
    assert "SOUTH=CROSS 0x01, WEST=SQUARE 0x04" in section
    assert "sdl3_cutscene_sample_ms,0" in section
    assert "sdl3_cutscene_sample_instance,0" in section
    assert "GetAsyncKeyState(" not in section
    assert "SPIDER_NATIVE_CUTSCENE_SKIP_REQUEST" not in section
    assert "IDirectInputDevice8_" not in section
    assert "SendInput(" not in section
    assert 'SpiderQuickExitF12_CutscenePhysicalSample(id,vid,pid,sample.buttons,ready);' in plan
    assert "SpiderNativeReconnect_SDL3SelectionReady(vid,pid)" in plan
    assert plan.count("SpiderQuickExitF12_CutscenePhysicalSample(0,0,0,0,0);") >= 4
    assert plan.index("GetForegroundWindow()!=hwnd") < plan.index(
        "SpiderQuickExitF12_CutscenePhysicalSample(id,vid,pid,sample.buttons,ready)")
    for forbidden in ("SendInput(", "VirtualProtect(", "WriteProcessMemory("):
        assert forbidden not in plan


def test_original_owner_edge_snapshots_do_not_deduplicate_unchanged_native_state():
    native=(ROOT/"quick_exit_f12.c").read_text(encoding="utf-8")
    part=native.split(
        "static void spider_trace_sdl3_cutscene_edge_window(",1)[1].split(
        "static void spider_trace_sdl3_native_cutscene_boundary(",1)[0]
    boundary=native.split(
        "static void spider_trace_sdl3_native_cutscene_boundary(",1)[1].split(
        "/* Native game-input owner wrapper at",1)[0]
    assert "reports>=64u" in part
    assert "(DWORD)(now-edge)>350u" in part
    assert "(DWORD)(now-sample)>125u" in part
    assert "(DWORD)(now-last_emit[after])<30u" in part
    assert "context!=1u || active!=1u || !ip" in part
    assert "SDL3-CUTSCENE-EDGE-OWNER:" in part
    assert "allow=%u active=%u request=%u script_ip=" in part
    assert "boundary=%s" in part
    assert "ORIGINAL 0x440EF0 UNCHANGED; CORRELATION ONLY" in part
    assert boundary.index("spider_trace_sdl3_cutscene_edge_window(") < boundary.index(
        "if(last_owner[after]==owner && last_snapshot[after]==packed)return;")
    owner=native.split(
        "static void __cdecl spider_observe_native_game_input(void) {",1)[1].split(
        "/* PASSIVE, exact-site 3D input diagnostics",1)[0]
    a=owner.index("spider_trace_sdl3_native_cutscene_boundary(0u);")
    b=owner.index("SPIDER_NATIVE_GAME_INPUT_UPDATE();")
    c=owner.index("spider_trace_sdl3_native_cutscene_boundary(1u);")
    assert a<b<c
    assert owner.count("SPIDER_NATIVE_GAME_INPUT_UPDATE();")==1
    for forbidden in ("VirtualProtect(", "WriteProcessMemory(",
                      "SendInput(", "*SPIDER_NATIVE_CUTSCENE_SKIP_REQUEST="):
        assert forbidden not in part+boundary
