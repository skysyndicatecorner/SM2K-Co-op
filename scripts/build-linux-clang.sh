#!/usr/bin/env bash
# Build dinput8.dll on Linux with clang + lld, without MinGW installed:
# fetches the mingw-w64 headers and .def files (sparse git checkout), makes import libraries with llvm-dlltool,
# compiles for i686-w64-mingw32 and links against msvcrt.dll. This is how the v0.12 release DLL was built.
# Needs: git, clang (>= 14), lld, llvm-dlltool.   Usage: scripts/build-linux-clang.sh   -> build/dinput8.dll
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${WORK:-$ROOT/build/toolchain}"
SYS="$WORK/sysroot"
# pinned mingw-w64 commit (headers + .def files) so the same clang version gives a byte-identical DLL
MINGW_REF="${MINGW_REF:-3ad495b55f0524c61fa3d02dbd0cdeceabb0c038}"
DLLTOOL="$(command -v llvm-dlltool || command -v llvm-dlltool-18 || echo /usr/lib/llvm-18/bin/llvm-dlltool)"

if [ ! -d "$WORK/mingw-w64" ]; then
  mkdir -p "$WORK"
  git clone -q --depth 1 --filter=blob:none --sparse https://github.com/mingw-w64/mingw-w64.git "$WORK/mingw-w64"
  git -C "$WORK/mingw-w64" sparse-checkout set mingw-w64-headers/include mingw-w64-headers/crt \
      mingw-w64-crt/lib32 mingw-w64-crt/lib-common mingw-w64-crt/def-include
  git -C "$WORK/mingw-w64" fetch -q --depth 1 origin "$MINGW_REF"
  git -C "$WORK/mingw-w64" checkout -q FETCH_HEAD
fi
MW="$WORK/mingw-w64"
if [ ! -f "$SYS/lib/libmsvcrt.a" ]; then
  mkdir -p "$SYS/include" "$SYS/lib"
  cp -r "$MW/mingw-w64-headers/include/." "$MW/mingw-w64-headers/crt/." "$SYS/include/"
  sed -e 's/@DEFAULT_MSVCRT_VERSION@/0x600/' -e 's/@DEFAULT_WIN32_WINNT@/0x601/' \
      "$MW/mingw-w64-headers/crt/_mingw.h.in" > "$SYS/include/_mingw.h"
  cp "$MW/mingw-w64-crt/lib32/kernel32.def" "$MW/mingw-w64-crt/lib32/user32.def" "$SYS/lib/"
  for f in ws2_32 msvcrt; do
    clang -E -P -x c -DDEF_I386 -D__i386__ -I "$MW/mingw-w64-crt/def-include" \
          "$MW/mingw-w64-crt/lib-common/$f.def.in" -o "$SYS/lib/$f.def" 2>/dev/null
  done
  for f in kernel32 user32 ws2_32 msvcrt; do "$DLLTOOL" -m i386 -k -d "$SYS/lib/$f.def" -l "$SYS/lib/lib$f.a"; done
fi

mkdir -p "$ROOT/build"
RES="$(clang -print-resource-dir)"
clang -target i686-w64-mingw32 -nostdinc -isystem "$RES/include" -isystem "$SYS/include" \
      -O2 -Wall -Wextra -Wno-unused-parameter -ffile-prefix-map="$ROOT"=. -c "$ROOT/src/dinput8.c" -o "$ROOT/build/dinput8.o"
clang -target i686-w64-mingw32 -fuse-ld=lld -shared -nostdlib -o "$ROOT/build/dinput8.dll" \
      "$ROOT/build/dinput8.o" "$ROOT/src/dinput8.def" -L"$SYS/lib" -lkernel32 -luser32 -lws2_32 -lmsvcrt \
      -Wl,--entry,_DllMain@12 -Wl,--kill-at -Wl,--no-insert-timestamp -s
echo "OK: build/dinput8.dll"
sha256sum "$ROOT/build/dinput8.dll"
