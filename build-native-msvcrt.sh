#!/usr/bin/env bash
# Native Linux cross build (no Docker/Podman, no sudo, no GitHub project upload).
set -euo pipefail
cd "$(dirname "$0")"

if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
    printf 'ERROR: this portable LLVM-MinGW bundle targets Linux x86_64 hosts.\n' >&2
    exit 1
fi
for tool in tar python3; do
    command -v "$tool" >/dev/null 2>&1 || { printf 'ERROR: missing command: %s\n' "$tool" >&2; exit 127; }
done
if ! command -v objdump >/dev/null 2>&1 && ! command -v i686-w64-mingw32-objdump >/dev/null 2>&1; then
    echo 'ERROR: GNU objdump is needed to verify PE32 imports and exports.' >&2
    exit 127
fi

version=20261006
pkg="llvm-mingw-${version}-msvcrt-ubuntu-22.04-x86_64"
url="https://github.com/mstorsjo/llvm-mingw/releases/download/${version}/${pkg}.tar.xz"
cache="${XDG_CACHE_HOME:-$HOME/.cache}/spiderman2k1fix-toolchains"
root="$cache/$pkg"
archive="$cache/$pkg.tar.xz"
compiler="$root/bin/i686-w64-mingw32-clang"

if [[ ! -x "$compiler" ]]; then
    mkdir -p "$cache"
    if [[ ! -s "$archive" ]]; then
        tmp="$archive.partial"
        trap 'rm -f -- "$tmp"' EXIT
        printf 'DOWNLOADING portable LLVM-MinGW (MSVCRT, ~83 MB):\n%s\n' "$url"
        if command -v curl >/dev/null 2>&1; then
            curl --fail --location --retry 3 --progress-bar --output "$tmp" "$url"
        elif command -v wget >/dev/null 2>&1; then
            wget --output-document="$tmp" "$url"
        else
            echo 'ERROR: curl or wget required for first download.' >&2
            exit 127
        fi
        mv -- "$tmp" "$archive"
        trap - EXIT
    fi
    echo 'CHECKING tarball integrity...'
    if ! tar -tJf "$archive" >/dev/null; then
        echo 'ERROR: toolchain archive is invalid; remove cache archive and retry.' >&2
        exit 1
    fi
    echo 'EXTRACTING portable toolchain...'
    tar -xJf "$archive" -C "$cache"
fi
if [[ ! -x "$compiler" ]]; then
    echo "ERROR: compiler not found after extraction: $compiler" >&2
    exit 1
fi

echo 'COMPILER:'
"$compiler" --version | head -n 1
printf 'CRT: msvcrt (portable llvm-mingw %s)\n' "$version"
echo 'BUILD: Windows x86 / PE32'
CC="$compiler" ./build.sh
