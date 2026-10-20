#!/usr/bin/env bash
# Native Wine game regression collector. Requires user's installed game.
# Does not edit the EXE, DLLs, Wine prefix, or INI.
set -uo pipefail

usage() {
  printf 'Usage: %s /path/to/Spider-Man/game-directory\n' "$0" >&2
  printf 'Optional: WINEPREFIX=/your/prefix WINE_BIN=/path/to/wine WINEDEBUG=+loaddll\n' >&2
  exit 2
}
[[ $# -eq 1 ]] || usage
game_dir=$(realpath -- "$1") || exit 2
[[ -d "$game_dir" ]] || usage
for name in SpideyPC.exe winmm.dll SpiderMan2k1Fix.dll SpiderMan2k1Fix.ini; do
  if [[ ! -f "$game_dir/$name" ]]; then
    printf 'ERROR: required file missing: %s\n' "$game_dir/$name" >&2
    exit 2
  fi
done

expected='0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea'
checksum=$(sha256sum -- "$game_dir/SpideyPC.exe" | cut -d' ' -f1)
if [[ "$checksum" != "$expected" ]]; then
  echo 'ERROR: unsupported SpideyPC.exe SHA256; stopping without launching.' >&2
  printf 'Expected: %s\nGot:      %s\n' "$expected" "$checksum" >&2
  exit 2
fi

if ! grep -Eq '^[[:space:]]*Log[[:space:]]*=[[:space:]]*1([[:space:]]*([;#].*)?)?$' "$game_dir/SpiderMan2k1Fix.ini"; then
  printf 'ERROR: set [Debug] Log=1 in %s before testing.\n' "$game_dir/SpiderMan2k1Fix.ini" >&2
  exit 2
fi

runner=${WINE_BIN:-wine}
if ! command -v "$runner" >/dev/null 2>&1; then
  printf 'ERROR: cannot find Wine executable: %s\n' "$runner" >&2
  exit 127
fi
report_dir=${WINE_TEST_REPORT_DIR:-"$PWD/wine-regression-$(date +%Y%m%d-%H%M%S)"}
mkdir -p -- "$report_dir"
report_dir=$(realpath -- "$report_dir")

{
  echo 'SpiderMan2k1Fix Wine native regression'
  echo "Date: $(date --iso-8601=seconds)"
  echo "Host: $(uname -srmo)"
  echo "Wine command: $runner"
  "$runner" --version
  echo "Prefix: ${WINEPREFIX:-default Wine prefix}"
  echo "Exe SHA256: $checksum"
  echo "Windows EXE: $(file -b "$game_dir/SpideyPC.exe" 2>/dev/null || echo unavailable)"
  echo "Fix DLL: $(file -b "$game_dir/SpiderMan2k1Fix.dll" 2>/dev/null || echo unavailable)"
  echo "WinMM DLL: $(file -b "$game_dir/winmm.dll" 2>/dev/null || echo unavailable)"
  grep -E '^(Log|Resolution|AspectRatio|FPSLimit|PauseOnFocusLoss|RestoreCursorOnFocusLoss|ExperimentalAltTabReacquire|ExperimentalFocusTrace)=' "$game_dir/SpiderMan2k1Fix.ini" || true
} > "$report_dir/environment.txt" 2>&1
cp -- "$game_dir/SpiderMan2k1Fix.ini" "$report_dir/SpiderMan2k1Fix.ini"

game_log="$game_dir/SpiderMan2k1Fix.log"
start_bytes=0
if [[ -f "$game_log" ]]; then start_bytes=$(stat -c '%s' -- "$game_log"); fi
echo "Wine version: $("$runner" --version 2>/dev/null)"
echo 'Launch: actual supported game, current DLL pair, desktop resolution.'
echo 'Test menu, intro movies, gameplay, audio, keyboard/mouse, then several Alt+Tab.'
echo "Report directory: $report_dir"
echo 'Close the game normally to finish collecting logs.'

# Respect an existing prefix and non-winmm Wine overrides.
overrides='winmm=n,b'
if [[ -n "${WINEDLLOVERRIDES:-}" ]]; then
  overrides+=";${WINEDLLOVERRIDES}"
fi
(
  cd "$game_dir" || exit 2
  WINEDLLOVERRIDES="$overrides" WINEDEBUG="${WINEDEBUG:--all}" "$runner" ./SpideyPC.exe
) > "$report_dir/wine-console.log" 2>&1
status=$?
printf 'Wine/game exit code: %d\n' "$status" | tee "$report_dir/exit-status.txt"

if [[ -f "$game_log" ]]; then
  size=$(stat -c '%s' -- "$game_log")
  if (( size >= start_bytes )); then
    tail -c "+$((start_bytes+1))" -- "$game_log" > "$report_dir/SpiderMan2k1Fix-new.log"
  else
    cp -- "$game_log" "$report_dir/SpiderMan2k1Fix-new.log"
  fi
fi
if [[ -s "$report_dir/SpiderMan2k1Fix-new.log" ]]; then
  grep -E '^(START:|DLL-LOAD:|EXE-CHECK:|OK:|ERROR:|DPI-BOOT:|WINDOW-ICON:|FOCUS-PAUSE:|INPUT-CURSOR:|INPUT-WINDOW: CURSOR-TEST|BORDERLESS:|DISPLAY-DPI:|D3D7: CreateDevice|SPIDEY-CFG: BOOTSTRAP)' \
    "$report_dir/SpiderMan2k1Fix-new.log" > "$report_dir/checkpoints.txt" || true
  echo 'Key startup checkpoints:'
  grep -E '^(OK: supported SpideyPC.exe|OK: SpiderMan2k1Fix.dll LoadLibrary succeeded|WINDOW-ICON:|INPUT-CURSOR:|D3D7: CreateDevice|ERROR:)' \
    "$report_dir/checkpoints.txt" | head -n 24 || true
else
  echo 'WARNING: No newly appended SpiderMan2k1Fix.log entries; check winmm=n,b and Log=1.' >&2
fi
archive="$report_dir.tar.gz"
tar -czf "$archive" -C "$(dirname "$report_dir")" "$(basename "$report_dir")"
printf 'Evidence archive: %s\n' "$archive"
if [[ $status -ne 0 ]]; then exit "$status"; fi
if [[ ! -s "$report_dir/SpiderMan2k1Fix-new.log" ]]; then exit 3; fi
if ! grep -Fq 'OK: supported SpideyPC.exe SHA-256 verified' "$report_dir/SpiderMan2k1Fix-new.log"; then
  echo 'WARNING: verified EXE initialization not present; check Wine override.' >&2
  exit 4
fi
echo 'Collector OK: supported game DLL startup confirmed. Visual/gameplay results require user assessment.'
