"""System report must be passive, privacy-conscious and compiled as Win32 x86.

Host pytest tests structural contracts; Windows x86 MSVCRT compilation in the
CI validates the actual Windows API signatures. Live Wine/Windows hardware
acceptance is explicitly distinct from these source tests.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def test_system_inventory_compiled_and_only_after_supported_game():
    core = read("spiderman2k1fix.c")
    build = read("build.sh")
    header = read("system_diagnostics.h")
    src = read("system_diagnostics.c")
    assert '#include "system_diagnostics.h"' in core
    assert "system_diagnostics.c" in build
    assert "SpiderSystemDiagnostics_Log(void (*logger)(const char *))" in header
    assert "void SpiderSystemDiagnostics_Log(void (*logger)(const char *))" in src
    init = core.split("static DWORD initialize_body(LPVOID unused)", 1)[1]
    assert init.index("debug_log_load_setting();") < init.index("if (!supported_exe())")
    assert init.index("if (!supported_exe())") < init.index(
        "SpiderCfg_ValidateCurrentIni(log_line);")
    assert init.index("SpiderCfg_ValidateCurrentIni(log_line);") < init.index(
        "if(debug_log_enabled)SpiderSystemDiagnostics_Log(log_line);")
    assert "SpiderSystemDiagnostics_Log(" not in core.split(
        "BOOL WINAPI DllMain(", 1)[1] if "BOOL WINAPI DllMain(" in core else True
    assert 'emit("SYS-INFO: BEGIN' in src
    assert 'emit("SYS-INFO: END' in src


def test_user_requested_os_wine_proton_host_and_desktop_fields_have_sources():
    src = read("system_diagnostics.c")
    for s in ("RtlGetVersion", "wine_get_version", "wine_get_host_version",
              "STEAM_COMPAT_DATA_PATH", "STEAM_COMPAT_TOOL_PATHS", "PROTON_VERSION",
              "XDG_CURRENT_DESKTOP", "XDG_SESSION_TYPE",
              "SYS-OS:", "SYS-WINE:", "SYS-HOST:", "SYS-PROTON:",
              "SYS-DESKTOP:"):
        assert s in src
    assert "emulated by Wine, NOT host OS" in src
    assert "environment hint, unverified" in src
    assert "directory name hint, NOT verified runtime version" in src
    assert "static int proton_tool_label(" in src
    assert "strpbrk(raw" in src
    assert "version=Unavailable (not exposed by Wine/Steam)" in src
    assert "cannot exclude Proton" in src


def test_cpu_ram_gpu_graphics_drivers_screen_count_and_resolution():
    src = read("system_diagnostics.c")
    for s in ("ProcessorNameString", "GetSystemInfo",
              "GetLogicalProcessorInformation", "RelationProcessorCore",
              "GlobalMemoryStatusEx", "EnumDisplayDevicesA",
              "DISPLAY_DEVICE_ATTACHED_TO_DESKTOP", "EnumDisplaySettingsA",
              "SM_CMONITORS", "SM_CXSCREEN", "SM_CYSCREEN",
              "SM_CXVIRTUALSCREEN", "SM_CYVIRTUALSCREEN",
              "Direct3DCreate9", "IDirect3D9_GetAdapterIdentifier",
              "DriverVersion", "SYS-CPU:", "SYS-RAM:", "SYS-DISPLAY:",
              "SYS-DISPLAYS:", "SYS-GPU:", "SYS-GRAPHICS-DRIVER:"):
        assert s in src
    assert "NOT Linux kernel/Mesa driver" in src
    assert "LoadLibraryA(\"d3d9.dll\")" in src
    assert "IDirect3D9_CreateDevice" not in src


def test_controllers_log_names_and_apis_without_acquiring_any_pad():
    src = read("system_diagnostics.c")
    for s in ("xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll",
              "XInputGetCapabilities", "DirectInput8Create",
              "IDirectInput8_EnumDevices", "DI8DEVCLASS_GAMECTRL",
              "DIEDFL_ATTACHEDONLY", "guidProduct.Data1", "tszProductName",
              "API=XInput", "API=DirectInput8"):
        assert s in src
    for s in ("IDirectInputDevice8_Acquire", "IDirectInputDevice8_Poll",
              "IDirectInputDevice8_GetDeviceState", "IDirectInput8_CreateDevice",
              "XInputSetState", "SetCooperativeLevel"):
        assert s not in src
    assert "an XInput pad may also appear in DirectInput" in src
    assert "model=Unavailable (XInput does not supply product name)" in src


def test_privacy_no_identifying_data_no_command_execution_or_env_paths():
    src = read("system_diagnostics.c")
    for s in ("USERNAME", "USERPROFILE", "COMPUTERNAME", "WINEPREFIX",
              "HOME", "LOCALAPPDATA", "GetComputerName",
              "GetUserName", "GetAdaptersAddresses", "GetVolumeInformation",
              "CreateProcess", "ShellExecute", "system(", "popen(",
              "Win32_Product", "WMI", "EDID", "RegEnumValue"):
        # Allowed exclusively in explanatory comments, not active code.
        code = re.sub(r"/\*.*?\*/|//[^\n]*", "", src, flags=re.S)
        assert s not in code
    assert "clean(out,cap,raw);" in src
    assert "c>=32 && c!=127" in src
    assert "Embedded newline or control" in src
    assert "Never read truncated values" in src
    assert "SYS-INFO: END" in src


def test_individual_unavailable_values_do_not_cancel_other_sections():
    src = read("system_diagnostics.c")
    entry = src.split("void SpiderSystemDiagnostics_Log(", 1)[1]
    names = ["log_os();", "log_cpu_memory();", "log_screens();",
             "log_graphics();", "log_desktop();", "log_controllers();"]
    assert all(name in entry for name in names)
    assert [entry.index(name) for name in names] == sorted(entry.index(name) for name in names)
    for s in ("SYS-RAM: Unavailable", "SYS-GPU: D3D9 adapter=Unavailable",
              "SYS-CONTROLLER: DirectInput8 enumeration unavailable",
              "resolution=Unavailable"):
        assert s in src
