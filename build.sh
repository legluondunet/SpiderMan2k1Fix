#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
CC=${CC:-i686-w64-mingw32-gcc}

# Both DLLs use the same Git commit; source archives use "local".
if [[ -n "${SPIDERMAN2K1FIX_COMMIT:-}" ]]; then
  build_commit="$SPIDERMAN2K1FIX_COMMIT"
elif git rev-parse --is-inside-work-tree >/dev/null 2>&1 &&
     git ls-files --error-unmatch -- spiderman2k1fix.c >/dev/null 2>&1; then
  build_commit=$(git rev-parse --short=7 HEAD)
  if ! git diff --quiet -- . || ! git diff --cached --quiet -- .; then
    build_commit="${build_commit}-dirty"
  fi
else
  build_commit=local
fi
if [[ ! "$build_commit" =~ ^[A-Za-z0-9][A-Za-z0-9._-]{0,31}$ ]]; then
  echo "ERROR: invalid SPIDERMAN2K1FIX_COMMIT: $build_commit" >&2
  exit 1
fi
version_define="-DSPIDERMAN2K1FIX_COMMIT=\"${build_commit}\""
build_version=$(sed -n 's/^#define SPIDERMAN2K1FIX_VERSION "\([^"]*\)"/\1/p' version_info.h)
if [[ ! "$build_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "ERROR: invalid version in version_info.h: $build_version" >&2
  exit 1
fi
printf 'BUILD-VERSION: SpiderMan2k1Fix v%s-%s\n' "$build_version" "$build_commit"

if ! command -v "$CC" >/dev/null 2>&1; then
  printf 'ERROR: missing 32-bit MinGW-w64 compiler: %s\n' "$CC" >&2
  printf 'Try ./build-native-msvcrt.sh (portable LLVM-MinGW; no Docker).\n' >&2
  exit 127
fi

OBJDUMP=""
for tool in i686-w64-mingw32-objdump objdump; do
  if command -v "$tool" >/dev/null 2>&1; then OBJDUMP="$tool"; break; fi
done
if [[ -z "$OBJDUMP" ]]; then
  echo 'ERROR: objdump is required to verify DLL imports.' >&2
  exit 127
fi

# A GCC option such as -mcrtdll=msvcrt cannot reliably switch a UCRT-built
# mingw-w64 CRT/headers/runtime library set. Probe the *linked* CRT, not the
# GCC driver's accepted options, before spending time building the game DLLs.
probe_dir=$(mktemp -d './spiderman2k1fix-crt-probe.XXXXXXXX')
trap 'rm -rf -- "$probe_dir"' EXIT
printf '%s\n' \
  '#include <stdio.h>' \
  '__declspec(dllexport) int crt_probe(const char *p) {' \
  '    FILE *f = fopen(p, "rb");' \
  '    if (!f) return 0;' \
  '    fclose(f);' \
  '    return 1;' \
  '}' | "$CC" -x c - -O0 -static-libgcc -shared -o "$probe_dir/crt_probe.dll"

imports=$("$OBJDUMP" -p "$probe_dir/crt_probe.dll" | grep 'DLL Name:' || true)
printf 'CRT-PROBE: %s\n' "$CC"
printf '%s\n' "$imports"
if printf '%s\n' "$imports" | grep -Eiq 'DLL Name:[[:space:]]*(api-ms-win-crt-[^[:space:]]*|ucrtbase\.dll)'; then
  echo 'ERROR: this MinGW-w64 toolchain links UCRT, not MSVCRT.' >&2
  echo 'Do not combine its UCRT libraries with MSVCRT by adding linker flags.' >&2
  echo 'Use ./build-native-msvcrt.sh for a consistent 32-bit MSVCRT toolchain.' >&2
  exit 1
fi
if ! printf '%s\n' "$imports" | grep -Eiq 'DLL Name:[[:space:]]*msvcrt\.dll'; then
  echo 'ERROR: runtime probe did not import MSVCRT.dll; compiler is unsuitable.' >&2
  echo 'Try ./build-native-msvcrt.sh.' >&2
  exit 1
fi
printf 'CRT-PROBE: MSVCRT import verified; building DLLs.\n'

# Build to temporary paths. Old DLLs stay intact if compilation or runtime
# validation fails; do not let a partially compiled pair reach the game.
"$CC" -std=gnu11 -O2 -fno-pie -mno-sse -mno-sse2 -mfpmath=387 -Wall -Wextra \
  -static-libgcc "$version_define" -shared -o "$probe_dir/SpiderMan2k1Fix.dll" \
  spiderman2k1fix.c quick_exit_f12.c f9_png.c f9_toast.c menu_version.c hud_widescreen.c hud_adaptive.c frame_limiter.c display_plan.c menu_scale.c comic_native.c virtual_spidey_cfg.c keyboard_names.c display_policy.c display_menu_lock.c fov_aspect.c fov_aspect_win.c input_focus_trace.c input_notacquired_retry.c native_joystick_unplug.c pause_any_key.c pause_surface.c focus_media_pause.c focus_dsound_pause.c pkr_audio_play_hook.c pkr_audio_index.c pkr_audio_loader.c gamepad_ui_profile.c ps1_joystick_readonly_menu.c ps1_mouse_mode.c third_party/miniz/miniz_tinfl.c \
  -ladvapi32 -ldinput8 -ldxguid -lgdi32 -lmsimg32 -lshell32 -Wl,--subsystem,windows
"$CC" -std=gnu11 -O2 -fno-pie -Wall -Wextra \
  -static-libgcc "$version_define" -shared -o "$probe_dir/winmm.dll" \
  winmm_proxy.c winmm.def -Wl,--subsystem,windows

for dll in SpiderMan2k1Fix.dll winmm.dll; do
  file="$probe_dir/$dll"
  if ! "$OBJDUMP" -f "$file" | grep -q 'pei-i386'; then
    echo "ERROR: $dll is not a 32-bit Windows PE/COFF DLL." >&2
    exit 1
  fi
  imports=$("$OBJDUMP" -p "$file" | grep 'DLL Name:' || true)
  printf '=== %s direct imports ===\n' "$dll"
  printf '%s\n' "$imports"
  if printf '%s\n' "$imports" | grep -Eiq 'DLL Name:[[:space:]]*(api-ms-win-crt-[^[:space:]]*|ucrtbase\.dll|libgcc_s[^[:space:]]*\.dll)'; then
    echo "ERROR: $dll still imports UCRT or dynamic libgcc; output not installed." >&2
    exit 1
  fi
  if ! printf '%s\n' "$imports" | grep -Eiq 'DLL Name:[[:space:]]*msvcrt\.dll'; then
    echo "ERROR: $dll does not import MSVCRT.dll; output not installed." >&2
    exit 1
  fi
done

# The winmm proxy's first-DirectDraw startup gate must locate this exact
# stable C export. Otherwise the proxy fails closed instead of launching the
# original unsupported 640x480 exclusive mode.
"$OBJDUMP" -p "$probe_dir/SpiderMan2k1Fix.dll" > "$probe_dir/fix-exports.txt"
if ! grep -Eq '(^|[[:space:]])SpiderMan2k1Fix_DirectDrawGate([[:space:]]|$)' "$probe_dir/fix-exports.txt"; then
  echo 'ERROR: missing SpiderMan2k1Fix_DirectDrawGate export required by startup proxy.' >&2
  exit 1
fi
echo 'EXPORTS: early DirectDraw bootstrap gate available'

# Validate all original game exports plus audio middleware forwards.
"$OBJDUMP" -p "$probe_dir/winmm.dll" > "$probe_dir/winmm-exports.txt"
python3 - "$probe_dir/winmm-exports.txt" <<'PYEXPORT'
import pathlib, re, sys
content = pathlib.Path(sys.argv[1]).read_text(errors='replace')
required = ('mmioAscend','mmioRead','mmioClose','mmioStringToFOURCCA',
            'mmioOpenA','mmioDescend','timeGetTime','timeEndPeriod',
            'timeBeginPeriod','timeSetEvent','timeKillEvent',
            'mciSendCommandA','timeGetDevCaps','timeGetSystemTime',
            'waveOutGetNumDevs', 'waveOutGetDevCapsA', 'waveOutGetErrorTextA', 'waveOutOpen',
            'waveOutClose', 'waveOutPrepareHeader', 'waveOutUnprepareHeader', 'waveOutWrite',
            'waveOutPause', 'waveOutRestart', 'waveOutReset', 'waveOutBreakLoop',
            'waveOutGetPosition', 'waveOutGetPitch', 'waveOutSetPitch', 'waveOutGetPlaybackRate',
            'waveOutSetPlaybackRate', 'waveOutGetVolume', 'waveOutSetVolume', 'waveOutGetID',
            'waveOutMessage', 'waveInGetNumDevs', 'waveInGetDevCapsA', 'waveInGetErrorTextA',
            'waveInOpen', 'waveInClose', 'waveInPrepareHeader', 'waveInUnprepareHeader',
            'waveInAddBuffer', 'waveInStart', 'waveInStop', 'waveInReset',
            'waveInGetPosition', 'waveInGetID', 'waveInMessage' )
missing = [name for name in required if not re.search(r'(?<![A-Za-z0-9_])' + re.escape(name) + r'(?![A-Za-z0-9_])', content)]
if missing:
    sys.exit('ERROR: missing winmm exports: ' + ', '.join(missing))
print('EXPORTS: 49 required winmm entry points detected')
PYEXPORT

cp "$probe_dir/SpiderMan2k1Fix.dll" "$probe_dir/winmm.dll" .
echo 'OK: validated local Windows x86 DLLs (MSVCRT; static libgcc).'
