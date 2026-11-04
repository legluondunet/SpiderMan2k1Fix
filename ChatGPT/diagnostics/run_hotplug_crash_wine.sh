#!/usr/bin/env bash
# External, read-only crash capture for Spider-Man 2001 DirectInput hot-unplug.
# Does not inject code, hook DirectInput, modify the EXE, or change the patch.
# Usage: bash ChatGPT/diagnostics/run_hotplug_crash_wine.sh /path/to/game [output-dir]
# Overrides: WINE_BIN=wine; HOTPLUG_WINEDEBUG=+seh,+dinput;
#            WINEDLLOVERRIDES defaults to winmm=n,b only if unset.
set -u
if (( $# < 1 || $# > 2 )); then
  printf 'Usage: %s GAME_DIR [OUTPUT_DIR]\n' "$0" >&2
  exit 2
fi
if [[ ! -d "$1" ]]; then
  printf 'HOTPLUG-CAPTURE: no game directory: %s\n' "$1" >&2
  exit 2
fi
game_dir="$(cd -- "$1" && pwd -P)" || exit 2
if [[ ! -f "$game_dir/SpideyPC.exe" ]]; then
  printf 'HOTPLUG-CAPTURE: SpideyPC.exe missing from: %s\n' "$game_dir" >&2
  exit 2
fi
wine_bin="${WINE_BIN:-wine}"
if ! command -v "$wine_bin" >/dev/null 2>&1; then
  printf 'HOTPLUG-CAPTURE: Wine executable unavailable: %s\n' "$wine_bin" >&2
  exit 2
fi
output_dir="${2:-$PWD/hotplug-crash-diagnostics}"
mkdir -p -- "$output_dir" || exit 2
output_dir="$(cd -- "$output_dir" && pwd -P)" || exit 2
stamp="$(date '+%Y%m%d-%H%M%S')"
trace="$output_dir/hotplug-wine-$stamp.log"
status_log="$output_dir/hotplug-exit-$stamp.txt"
start="$(date -Is)"
printf 'HOTPLUG-CAPTURE: running %s from %s\n' "$wine_bin" "$game_dir"
printf 'HOTPLUG-CAPTURE: output %s\n' "$trace"
printf 'HOTPLUG-CAPTURE: start=%s\n' "$start"
printf 'HOTPLUG-CAPTURE: unplug the DS4 from the main menu; do not alt-tab during this test.\n'
# Do not use set -e: capturing a nonzero exit is the purpose of this tool.
(
  cd -- "$game_dir" || exit 2
  ulimit -c unlimited 2>/dev/null || true
  export WINEDEBUG="${HOTPLUG_WINEDEBUG:-+seh,+dinput}"
  export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-winmm=n,b}"
  "$wine_bin" SpideyPC.exe
) > "$trace" 2>&1
exit_code=$?
end="$(date -Is)"
{
  printf 'START=%s\n' "$start"
  printf 'END=%s\n' "$end"
  printf 'WINE_EXIT_CODE=%s\n' "$exit_code"
  if (( exit_code > 128 )); then
    printf 'POSSIBLE_UNIX_SIGNAL=%s (only a candidate; inspect Wine and kernel logs)\n' "$((exit_code - 128))"
  fi
  printf 'WINEDEBUG=%s\n' "${HOTPLUG_WINEDEBUG:-+seh,+dinput}"
  printf 'GAME_DIR=%s\n' "$game_dir"
  printf 'WINE_LOG=%s\n' "$trace"
  printf 'NOTE=Exit status alone does not identify a faulting Windows module.\n'
  printf 'NOTE=RtlUnwindEx code 80000026 alone is NOT a fatal SEH exception.\n'
} | tee "$status_log"
printf 'HOTPLUG-CAPTURE: preserve both logs and the new SpiderMan2k1Fix(...).log.\n'
exit "$exit_code"
