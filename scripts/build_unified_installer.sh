#!/usr/bin/env bash
# Single-DLL development installer; only the production mode is user-facing.
set -euo pipefail
cd "$(dirname "$0")/.."
mode=production
if test "$#" -gt 0; then mode="$1"; fi
case "$mode" in
 production) source_file=installer/spiderman2k1fix_installer.c
             output=dist/unified/SpiderMan2k1Fix.exe ;;
 fixture) source_file=installer/spiderman2k1fix_installer.fixture.c
          output=dist/unified/SpiderMan2k1Fix-UNIFIED-FIXTURE-TEST-ONLY.exe ;;
 *) echo "usage: $0 [production|fixture]" >&2; exit 2 ;;
esac
cache_root="$(printenv XDG_CACHE_HOME || true)"
if test -z "$cache_root"; then cache_root="$HOME/.cache"; fi
toolchain="$cache_root/spiderman2k1fix-toolchains/llvm-mingw-20261006-msvcrt-ubuntu-22.04-x86_64/bin"
CC="$toolchain/i686-w64-mingw32-clang"
RC="$toolchain/i686-w64-mingw32-windres"
if test -n "$(printenv SPIDER_INSTALLER_CC || true)"; then CC="$SPIDER_INSTALLER_CC"; fi
if test -n "$(printenv SPIDER_INSTALLER_WINDRES || true)"; then RC="$SPIDER_INSTALLER_WINDRES"; fi
OBJDUMP="$(command -v i686-w64-mingw32-objdump || command -v objdump || true)"
for p in \
 dist/unified/package/game/fix.dll \
 dist/unified/package/game/SpideyPC.exe.manifest \
 dist/unified/package/third_party/SDL3/SDL3.dll \
 dist/unified/package/third_party/SDL3/LICENSE.txt \
 dist/unified/package/linux/spiderman-focus-helper \
 dist/unified/package/docs/README.md \
 dist/unified/package/docs/CHANGELOG.md \
 dist/unified/package/docs/LICENSE; do
 test -s "$p" || { echo "UNIFIED: missing embedded resource: $p" >&2; exit 1; }
done
for t in "$CC" "$RC"; do test -x "$t" || exit 1; done
test -n "$OBJDUMP"
test -s "$source_file"
mkdir -p dist/unified
cat > dist/unified/payload.rc <<'RCDATA'
1102 RCDATA "dist/unified/package/game/fix.dll"
1103 RCDATA "dist/unified/package/third_party/SDL3/SDL3.dll"
1104 RCDATA "dist/unified/package/third_party/SDL3/LICENSE.txt"
1105 RCDATA "dist/unified/package/linux/spiderman-focus-helper"
1106 RCDATA "dist/unified/package/docs/README.md"
1107 RCDATA "dist/unified/package/docs/CHANGELOG.md"
1108 RCDATA "dist/unified/package/docs/LICENSE"
1109 RCDATA "dist/unified/package/game/SpideyPC.exe.manifest"
RCDATA
"$RC" -I . -O coff -i dist/unified/payload.rc -o dist/unified/payload.o
"$CC" -std=gnu11 -O2 -Wall -Wextra -Werror -static-libgcc -municode \
  -DSPIDER_UNIFIED_INSTALL -Wl,--subsystem,console -I . \
  -o "$output" \
  "$source_file" installer/unified_import_alias.c \
  dist/unified/payload.o -ladvapi32 -luser32
"$OBJDUMP" -f "$output" | grep -q 'pei-i386'
"$OBJDUMP" -p "$output" > dist/unified/installer-pe-info.txt
! grep -Eiq 'DLL Name:[[:space:]]*(ucrtbase\.dll|libgcc_s|winmm\.dll|ddraw\.dll)' \
 dist/unified/installer-pe-info.txt
grep -Eiq 'DLL Name:[[:space:]]*msvcrt\.dll' dist/unified/installer-pe-info.txt
echo "UNIFIED: compiled $output (DEVELOPMENT BUILD, NOT RELEASE)"
sha256sum "$output"
