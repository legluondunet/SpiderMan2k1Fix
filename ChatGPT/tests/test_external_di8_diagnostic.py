"""Guard the Windows-only debugger probe against accidental COM/input mutation."""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
JS = ROOT / "ChatGPT/diagnostics" / "trace_directinput8_windbg.js"
PS = ROOT / "ChatGPT/diagnostics" / "run_directinput8_trace.ps1"
DOC = ROOT / "ChatGPT/diagnostics" / "WINDBG_DI8_TRACE.md"


def test_external_probe_files_exist_and_are_observation_only():
    assert JS.is_file() and PS.is_file() and DOC.is_file()
    src = JS.read_text(encoding="utf-8")
    for method in ("DirectInput8Create", "CreateDevice", "Acquire",
                   "Unacquire", "GetDeviceState", "GetDeviceData"):
        assert method in src
    assert "di8WatchMethod(interfacePtr, 3" in src
    assert "di8WatchMethod(ptr, 7" in src
    assert "di8WatchMethod(ptr, 8" in src
    assert "di8WatchMethod(ptr, 9" in src
    assert "di8WatchMethod(ptr, 10" in src
    assert 'di8Break(method, "di8OnCreateDevice()")' in src
    assert '"di8OnMethod(" + slot + ")"' in src
    assert "di8Return({ type:" in src
    for forbidden in ("VirtualProtect", "SetCooperativeLevel(",
                      "SetForegroundWindow(", "SendInput(",
                      "->Acquire", ".Acquire(", "WriteProcessMemory"):
        assert forbidden not in src


def test_launcher_has_no_game_binary_or_ini_writes():
    launcher = PS.read_text(encoding="utf-8")
    assert "Debuggers\x5cx86\x5ccdb.exe" in launcher
    assert "SpideyPC.exe" in launcher
    assert "trace_directinput8_windbg.js" in launcher
    assert 'dx @$scriptContents.di8Start(); g' in launcher
    assert "-logo $logPath" in launcher
    assert "Copy-Item -LiteralPath $scriptSource" in launcher
    for forbidden in ("Set-Content", "Add-Content", "Set-ItemProperty",
                      "SpiderMan2k1Fix.ini", "SpiderMan2k1Fix.dll"):
        assert forbidden not in launcher


def test_js_syntax_if_node_is_available():
    node = shutil.which("node")
    if node:
        subprocess.run([node, "--check", str(JS)], check=True, timeout=10)
