"""Wine exit-only relay diagnosis is external and must restore existing prefix debug settings."""
import os
from pathlib import Path
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
SCRIPT=ROOT/"ChatGPT/diagnostics/run_hotplug_exit_relay_wine.sh"


def run_mock(tmp_path, old_value=None):
    game=tmp_path/"game with spaces"
    game.mkdir()
    (game/"SpideyPC.exe").touch()
    fake=tmp_path/"fake-wine"
    state=tmp_path/"wine-registry-value"
    if old_value is not None:
        state.write_text(old_value,encoding="utf-8")
    fake.write_text(textwrap.dedent("""\\
        #!/usr/bin/env bash
        set -u
        if [[ "$1" == reg ]]; then
          case "$2" in
            query)
              if [[ -f "$FAKE_STATE" ]]; then
                printf '    RelayInclude    REG_SZ    %s\\n' "$(cat "$FAKE_STATE")"
                exit 0
              fi
              exit 1
              ;;
            add)
              shift 2
              while (( $# )); do
                if [[ "$1" == /d ]]; then printf '%s' "$2" > "$FAKE_STATE"; exit 0; fi
                shift
              done
              exit 4
              ;;
            delete)
              rm -f -- "$FAKE_STATE"
              exit 0
              ;;
          esac
          exit 3
        fi
        if [[ "$1" == SpideyPC.exe ]]; then
          test -f "$FAKE_STATE" || exit 7
          grep -q 'kernel32.ExitProcess' "$FAKE_STATE" || exit 8
          printf '012c:trace:relay: kernel32.ExitProcess(00000009) ret=004abcde\\n'
          printf '0130:trace:seh:RtlUnwindEx code=80000026\\n'
          exit 9
        fi
        exit 2
        """), encoding="utf-8")
    fake.chmod(0o755)
    env={**os.environ,"WINE_BIN":str(fake),"FAKE_STATE":str(state)}
    out=tmp_path/"results"
    cmd=["bash",str(SCRIPT),str(game),str(out)]
    result=subprocess.run(cmd,capture_output=True,text=True,env=env)
    return result,state,out


def test_restore_existing_relay_filter_and_preserve_call_log(tmp_path):
    result,state,out=run_mock(tmp_path,"kernel32.ReadFile;user32.MessageBoxA")
    assert result.returncode==9, result.stderr
    assert state.read_text()=="kernel32.ReadFile;user32.MessageBoxA"
    assert "original Wine RelayInclude restored" in result.stdout
    status=next(out.glob("hotplug-exit-status-*.txt")).read_text()
    assert "WINE_EXIT_CODE=9" in status
    calls=next(out.glob("hotplug-exit-calls-*.txt")).read_text()
    assert "kernel32.ExitProcess(00000009)" in calls
    raw=next(out.glob("hotplug-exit-relay-*.log")).read_text()
    assert "RtlUnwindEx" in raw


def test_restore_initially_absent_relay_filter(tmp_path):
    result,state,out=run_mock(tmp_path)
    assert result.returncode==9, result.stderr
    assert not state.exists()
    assert next(out.glob("hotplug-exit-status-*.txt")).exists()


def test_no_game_if_exe_missing_and_refuse_unsafe_registry_type(tmp_path):
    game=tmp_path/"missing"
    game.mkdir()
    result=subprocess.run(["bash",str(SCRIPT),str(game)],capture_output=True,text=True)
    assert result.returncode==2
    assert "SpideyPC.exe missing" in result.stderr
    assert not list(tmp_path.glob("hotplug-exit-diagnostics/*"))


def test_read_only_external_wine_diagnostic():
    body=SCRIPT.read_text()
    assert "RelayInclude" in body
    assert "trap restore_filter EXIT" in body
    assert "WINEDEBUG='+relay,+process,+seh'" in body
    assert "RtlExitUserProcess" in body
    assert "WINE_EXIT_CODE" in body
    for unsafe in ("VirtualProtect(", "SetWindowsHookEx(", "WriteProcessMemory(",
                   "IDirectInputDevice8_Acquire(", "pkill -9", "wineserver -k"):
        assert unsafe not in body
