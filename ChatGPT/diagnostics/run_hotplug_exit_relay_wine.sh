#!/usr/bin/env bash
# Focused, external Wine API exit trace for the original Spider-Man PC game.
# No code injection, DirectInput patches, DLL replacements or EXE edits.
# Temporarily sets the per-prefix HKCU\Software\Wine\Debug\RelayInclude,
# preserves an existing REG_SZ value and restores it when the script exits.
#
# Usage:
#   bash ChatGPT/diagnostics/run_hotplug_exit_relay_wine.sh GAME_DIR [OUTPUT_DIR]
# Optional WINE_BIN=wine and existing WINEPREFIX / WINEDLLOVERRIDES respected.
#
# IMPORTANT: Other Wine processes in the SAME prefix may inherit the trace
# filter while this diagnostic runs. Close them before starting.
set -u
if (( $# < 1 || $# > 2 )); then
  printf 'Usage: %s GAME_DIR [OUTPUT_DIR]\n' "$0" >&2
  exit 2
fi
if [[ ! -d "$1" ]]; then
  printf 'HOTPLUG-EXIT: invalid game directory: %s\n' "$1" >&2
  exit 2
fi
game_dir="$(cd -- "$1" && pwd -P)" || exit 2
if [[ ! -f "$game_dir/SpideyPC.exe" ]]; then
  printf 'HOTPLUG-EXIT: SpideyPC.exe missing: %s\n' "$game_dir" >&2
  exit 2
fi
wine_bin="${WINE_BIN:-wine}"
if ! command -v "$wine_bin" >/dev/null 2>&1; then
  printf 'HOTPLUG-EXIT: Wine executable not found: %s\n' "$wine_bin" >&2
  exit 2
fi
out_dir="${2:-$PWD/hotplug-exit-diagnostics}"
mkdir -p -- "$out_dir" || exit 2
out_dir="$(cd -- "$out_dir" && pwd -P)" || exit 2
stamp="$(date '+%Y%m%d-%H%M%S')"
trace="$out_dir/hotplug-exit-relay-$stamp.log"
status_file="$out_dir/hotplug-exit-status-$stamp.txt"
summary="$out_dir/hotplug-exit-calls-$stamp.txt"
key='HKCU\Software\Wine\Debug'
# Exact exit entrypoints. If one DLL forwards internally rather than via
# import/relay, absence from log does not prove no ExitProcess occurred.
include='kernel32.ExitProcess;kernelbase.ExitProcess;kernel32.TerminateProcess;kernelbase.TerminateProcess;ntdll.RtlExitUserProcess;ntdll.NtTerminateProcess;msvcrt.exit;msvcrt._exit;msvcrt.abort;ucrtbase.exit;ucrtbase._exit;ucrtbase.abort'
had_value=0
old_value=''
modified=0
# A preexisting non-REG_SZ RelayInclude cannot be safely restored by our
# simple diagnostic: refuse to overwrite it.
query="$(WINEDEBUG=-all "$wine_bin" reg query "$key" /v RelayInclude 2>/dev/null)" && query_ok=1 || query_ok=0
if (( query_ok )); then
  if [[ "$query" =~ RelayInclude[[:space:]]+REG_SZ[[:space:]]*(.*) ]]; then
    had_value=1
    old_value="${BASH_REMATCH[1]}"
  else
    printf 'HOTPLUG-EXIT: existing RelayInclude not REG_SZ or unreadable; refusing to replace it.\n' >&2
    exit 2
  fi
fi
restore_filter() {
  local result
  (( modified )) || return 0
  if (( had_value )); then
    WINEDEBUG=-all "$wine_bin" reg add "$key" /v RelayInclude /t REG_SZ /d "$old_value" /f >/dev/null 2>&1
    result=$?
  else
    WINEDEBUG=-all "$wine_bin" reg delete "$key" /v RelayInclude /f >/dev/null 2>&1
    result=$?
  fi
  if (( result )); then
    printf 'HOTPLUG-EXIT: WARNING failed to restore original Wine RelayInclude; manually inspect your prefix.\n' >&2
  else
    printf 'HOTPLUG-EXIT: original Wine RelayInclude restored.\n'
  fi
}
trap restore_filter EXIT
if ! WINEDEBUG=-all "$wine_bin" reg add "$key" /v RelayInclude /t REG_SZ /d "$include" /f >/dev/null 2>&1; then
  printf 'HOTPLUG-EXIT: failed to set temporary Wine relay filter; no game launched.\n' >&2
  exit 2
fi
modified=1
printf 'HOTPLUG-EXIT: temporary exit-only relay filter installed for this Wine prefix.\n'
printf 'HOTPLUG-EXIT: launch game and reproduce the input issue in the intended menu or gameplay mode.\n'
printf 'HOTPLUG-EXIT: Wine log: %s\n' "$trace"
(
  cd -- "$game_dir" || exit 2
  export WINEDEBUG='+relay,+process,+seh'
  export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-winmm=n,b}"
  "$wine_bin" SpideyPC.exe
) > "$trace" 2>&1
exit_code=$?
{
  printf 'WINE_EXIT_CODE=%s\n' "$exit_code"
  printf 'Game controller profile: read the matching SpiderMan2k1Fix log; this script does not enforce ControllerProfile=Off or Auto.\n'
  printf 'A code 9 without explicit API-call evidence does not establish who chose that code.\n'
  printf 'Wine relay may not intercept internal or forwarded calls.\n'
} | tee "$status_file"
# Keep a concise grep alongside the full filtered relay text; do not
# confuse the command-line tool's process exit with a captured Windows call.
grep -Ei '(^|[ .!:])(ExitProcess|RtlExitUserProcess|TerminateProcess|NtTerminateProcess|[_]?exit|abort)\(' "$trace" > "$summary" || true
printf 'HOTPLUG-EXIT: filtered call summary: %s\n' "$summary"
printf 'HOTPLUG-EXIT: preserve also the matching SpiderMan2k1Fix(...).log.\n'
exit "$exit_code"
