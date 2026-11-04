#!/usr/bin/env bash
# SpiderMan2k1Fix Wine/X11 focus bridge, explicitly opted in.
# Pure X11 observer. NEVER sends keys, activates windows or touches game memory.
set -euo pipefail

if ! command -v xdotool >/dev/null 2>&1; then
  echo "ERROR: xdotool is required (X11 only)." >&2
  exit 2
fi
if [[ -z "${DISPLAY:-}" ]]; then
  echo "ERROR: DISPLAY is unset (X11 session required)." >&2
  exit 2
fi

outdir="${1:-$PWD}"
if [[ ! -d "$outdir" || ! -w "$outdir" ]]; then
  echo "ERROR: output directory does not exist or is not writable: $outdir" >&2
  exit 2
fi
# Must be the directory that contains SpideyPC.exe, not the DLL subdirectory.
if [[ ! -f "$outdir/SpideyPC.exe" ]]; then
  echo "ERROR: $outdir/SpideyPC.exe not found. Pass the game's directory." >&2
  exit 2
fi
mapfile -t game_pids < <(pgrep -x 'SpideyPC.exe' || true)
if (( ${#game_pids[@]} != 1 )); then
  echo "ERROR: expected exactly one running SpideyPC.exe; found ${#game_pids[@]}." >&2
  exit 2
fi
game_pid="${game_pids[0]}"
[[ "$(cat "/proc/$game_pid/comm" 2>/dev/null || true)" == "SpideyPC.exe" ]] || {
  echo "ERROR: selected PID is not the game." >&2
  exit 2
}
out="$outdir/SpiderMan2k1Fix.x11-focus"
tmp=""
cleanup() {
  [[ -z "$tmp" ]] || rm -f -- "$tmp"
  rm -f -- "$out"
}
trap cleanup EXIT INT TERM
echo "Wine/X11 focus bridge: game Linux PID=$game_pid"
echo "Writing $out (every 200ms); run the game with SPIDERMAN2K1FIX_X11_BRIDGE=1"
echo "Press Ctrl+C to stop. All actions read-only except bridge state file."
last=""
while kill -0 "$game_pid" 2>/dev/null &&
      [[ "$(cat "/proc/$game_pid/comm" 2>/dev/null || true)" == "SpideyPC.exe" ]]; do
  focus_pid="$(xdotool getwindowfocus getwindowpid 2>/dev/null || true)"
  state="UNKNOWN"
  if [[ "$focus_pid" =~ ^[0-9]+$ ]] && (( focus_pid > 0 )); then
    if [[ "$focus_pid" == "$game_pid" ]]; then state="GAME"
    else state="OTHER"
    fi
  fi
  # The DLL checks last-write time and fails closed on a stale or missing
  # bridge. An atomic rename ensures it never observes half a state line.
  tmp="$(mktemp "$outdir/.SpiderMan2k1Fix-x11focus.XXXXXXXX")"
  printf 'SPIDEY_X11_FOCUS_V1 %s\n' "$state" > "$tmp"
  mv -f -- "$tmp" "$out"
  tmp=""
  if [[ "$state" != "$last" ]]; then
    printf '%s X11 focus=%s focus_pid=%s game_pid=%s\n' \
      "$(date '+%Y-%m-%d %H:%M:%S.%3N')" "$state" "${focus_pid:-unknown}" "$game_pid"
    last="$state"
  fi
  sleep 0.2
done
echo "Game process exited; bridge stopped."
