"""External Wine crash tracing does not alter the retail game's DirectInput."""
from pathlib import Path
import os
import subprocess
import sys
import textwrap

ROOT=Path(__file__).resolve().parents[2]
SCRIPT=ROOT/"ChatGPT"/"diagnostics"/"run_hotplug_crash_wine.sh"

def test_wine_external_capture_keeps_exit_status_and_both_logs(tmp_path):
    game=tmp_path/"spider man with spaces"
    game.mkdir()
    (game/"SpideyPC.exe").write_bytes(b"fake exe for isolated launcher test")
    wine=tmp_path/"test-wine"
    wine.write_text(textwrap.dedent("""#!/bin/sh
    printf 'TEST-WINE: fake foreground game running\\n'
    printf 'TEST-WINE: deliberate abnormal return\\n' >&2
    exit 42
    """))
    wine.chmod(0o755)
    out=tmp_path/"artifacts"
    env=dict(os.environ,WINE_BIN=str(wine))
    result=subprocess.run(["bash",str(SCRIPT),str(game),str(out)],
                          text=True,capture_output=True,env=env)
    assert result.returncode==42, (result.stdout,result.stderr)
    traces=list(out.glob("hotplug-wine-*.log"))
    statuses=list(out.glob("hotplug-exit-*.txt"))
    assert len(traces)==len(statuses)==1
    assert "TEST-WINE: fake foreground game running" in traces[0].read_text()
    assert "TEST-WINE: deliberate abnormal return" in traces[0].read_text()
    status=statuses[0].read_text()
    assert "WINE_EXIT_CODE=42" in status
    assert "WINEDEBUG=+seh,+dinput" in status
    assert "POSSIBLE_UNIX_SIGNAL=" not in status
    assert "only a candidate" not in status
    assert "NOTE=Exit status alone does not identify" in status
    assert "SpideyPC.exe" in SCRIPT.read_text()
    assert "WINEDLLOVERRIDES" in SCRIPT.read_text()

def test_wine_external_capture_signal_like_status_is_not_misdiagnosed(tmp_path):
    game=tmp_path/"game"
    game.mkdir()
    (game/"SpideyPC.exe").touch()
    wine=tmp_path/"test-wine"
    wine.write_text("#!/bin/sh\nexit 139\n")
    wine.chmod(0o755)
    env=dict(os.environ,WINE_BIN=str(wine),HOTPLUG_WINEDEBUG="+seh")
    result=subprocess.run(["bash",str(SCRIPT),str(game),str(tmp_path/"out")],
                          text=True,capture_output=True,env=env)
    assert result.returncode==139
    status=next((tmp_path/"out").glob("hotplug-exit-*.txt")).read_text()
    assert "POSSIBLE_UNIX_SIGNAL=11 (only a candidate;" in status
    assert "WINEDEBUG=+seh" in status

def test_wine_external_capture_refuses_missing_retail_exe(tmp_path):
    game=tmp_path/"game"
    game.mkdir()
    result=subprocess.run(["bash",str(SCRIPT),str(game)],
                          text=True,capture_output=True)
    assert result.returncode==2
    assert "SpideyPC.exe missing" in result.stderr

def test_wine_external_capture_no_in_process_hooks():
    s=SCRIPT.read_text()
    for forbidden in ("SetWindowsHookEx(", "IDirectInputDevice8_Acquire(",
                      "WriteProcessMemory(", "VirtualProtect(", "KillProcess",
                      "kill -9", "wineserver -k", "wmic process"):
        assert forbidden not in s
