#!/usr/bin/env bash
# Passive X11 active-window logger for SpiderMan2k1Fix Wine focus diagnostics.
# Run from a Linux X11 session. Does not modify window focus or game state.
set -u

if (( $# > 1 )); then
  printf 'Usage: bash %s [output.log]\n' "$0" >&2
  exit 2
fi
if ! command -v xprop >/dev/null 2>&1; then
  printf 'ERROR: xprop is required (often provided by xorg-xprop).\n' >&2
  exit 1
fi
if [[ -z "${DISPLAY:-}" ]]; then
  printf 'ERROR: DISPLAY is unset; an X11 server is required.\n' >&2
  exit 1
fi

output=${1:-"$PWD/spiderman-x11-focus-$(date +%Y%m%d-%H%M%S).log"}
if ! touch -- "$output"; then
  printf 'ERROR: cannot write log: %s\n' "$output" >&2
  exit 1
fi
printf 'X11 focus log: %s\n' "$output"
printf 'Start this script before launching Spider-Man; press Ctrl+C after the test.\n'

previous='__unset__'
last_heartbeat=0
printf '# X11 active-window changes: started %s; DISPLAY=%s\n' \
  "$(date '+%Y-%m-%dT%H:%M:%S.%3N%z')" "$DISPLAY" >> "$output"

while :; do
  timestamp=$(date '+%Y-%m-%dT%H:%M:%S.%3N%z')
  raw=$(LC_ALL=C xprop -root _NET_ACTIVE_WINDOW 2>&1)
  if [[ "$raw" =~ \#[[:space:]]*(0x[[:xdigit:]]+) ]]; then
    active=${BASH_REMATCH[1]}
  else
    active=unknown
  fi

  if [[ "$active" != "$previous" ]]; then
    {
      printf '%s CHANGE active=%s\n' "$timestamp" "$active"
      if [[ "$active" == unknown ]]; then
        printf '  xprop-root: %s\n' "$raw"
      elif [[ "$active" == 0x0 ]]; then
        printf '  (no X11 active window)\n'
      else
        LC_ALL=C xprop -id "$active" _NET_WM_NAME WM_NAME WM_CLASS _NET_WM_PID 2>&1 \
          | sed 's/^/  /'
      fi
    } >> "$output"
    previous=$active
  fi

  # An occasional heartbeat proves the observer is alive even without changes.
  if (( SECONDS - last_heartbeat >= 10 )); then
    printf '%s HEARTBEAT active=%s\n' "$timestamp" "$active" >> "$output"
    last_heartbeat=$SECONDS
  fi
  sleep 0.2
done
