#!/usr/bin/env bash
# Standalone Windows x86 release candidate: project code and redistributable runtimes
# embedded as RCDATA, with no original Spider-Man game files included.
set -euo pipefail
cd "$(dirname "$0")/.."

toolchain="${XDG_CACHE_HOME:-$HOME/.cache}/spiderman2k1fix-toolchains/llvm-mingw-20261006-msvcrt-ubuntu-22.04-x86_64/bin"
CC="${SPIDER_INSTALLER_CC:-$toolchain/i686-w64-mingw32-clang}"
RC="${SPIDER_INSTALLER_WINDRES:-$toolchain/i686-w64-mingw32-windres}"
OBJDUMP="$(command -v i686-w64-mingw32-objdump || command -v objdump || true)"
for asset in \
  dist/package/winmm.dll \
  dist/package/SpiderMan2k1Fix/SpiderMan2k1Fix.dll \
  dist/package/SpiderMan2k1Fix/SDL3.dll \
  dist/package/SpiderMan2k1Fix/SDL3-LICENSE.txt \
  dist/package/SpiderMan2k1Fix/linux/spiderman-focus-helper \
  dist/package/SpiderMan2k1Fix/README.md \
  dist/package/SpiderMan2k1Fix/CHANGELOG.md; do
  test -s "$asset" || { echo "INSTALLER: required payload missing: $asset" >&2; exit 1; }
done
for tool in "$CC" "$RC"; do
  test -x "$tool" || { echo "INSTALLER: missing tool: $tool" >&2; exit 1; }
done
test -n "$OBJDUMP"
mkdir -p dist/installer
cat > dist/installer/payload.rc <<'RCDATA'
1101 RCDATA "dist/package/winmm.dll"
1102 RCDATA "dist/package/SpiderMan2k1Fix/SpiderMan2k1Fix.dll"
1103 RCDATA "dist/package/SpiderMan2k1Fix/SDL3.dll"
1104 RCDATA "dist/package/SpiderMan2k1Fix/SDL3-LICENSE.txt"
1105 RCDATA "dist/package/SpiderMan2k1Fix/linux/spiderman-focus-helper"
1106 RCDATA "dist/package/SpiderMan2k1Fix/README.md"
1107 RCDATA "dist/package/SpiderMan2k1Fix/CHANGELOG.md"
RCDATA
"$RC" -I . -O coff -i dist/installer/payload.rc -o dist/installer/payload.o
"$CC" -std=gnu11 -O2 -Wall -Wextra -Werror \
  -static-libgcc -municode -Wl,--subsystem,console \
  -I . -o dist/installer/SpiderMan2k1Fix.exe \
  installer/spiderman2k1fix_installer.c dist/installer/payload.o \
  -ladvapi32 -luser32
"$OBJDUMP" -f dist/installer/SpiderMan2k1Fix.exe | grep -q 'pei-i386'
"$OBJDUMP" -h dist/installer/SpiderMan2k1Fix.exe | grep -q '\.rsrc'
"$OBJDUMP" -p dist/installer/SpiderMan2k1Fix.exe > dist/installer/pe-info.txt
if grep 'DLL Name:' dist/installer/pe-info.txt | grep -Eiq 'ucrtbase\.dll|api-ms-win-crt-|libgcc_s'; then
  echo 'INSTALLER: UCRT or dynamic libgcc dependency forbidden' >&2
  exit 1
fi
grep -Eiq 'DLL Name:[[:space:]]*msvcrt\.dll' dist/installer/pe-info.txt
# Avoid publishing an executable without all seven embedded resource names.
# Windres expands bytes as RCDATA in the PE32 .rsrc; source test inventories
# the exact names and ID mapping. Only this EXE is uploaded as a candidate artifact.
echo 'INSTALLER: standalone Windows x86 resource-packed EXE successfully built'
sha256sum dist/installer/SpiderMan2k1Fix.exe

