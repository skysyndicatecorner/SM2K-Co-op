# SM2K Co-op: build with MinGW-w64 (MSYS2 "MINGW32" shell, or Linux with gcc-mingw-w64-i686).
#   make            -> build/dinput8.dll
#   make clean
CC      ?= i686-w64-mingw32-gcc
CFLAGS  ?= -O2 -Wall -Wno-unused-parameter
LDFLAGS ?= -shared -static-libgcc -Wl,--kill-at
LIBS     = -luser32 -lkernel32 -lws2_32

build/dinput8.dll: src/dinput8.c src/dinput8.def
	mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ src/dinput8.c src/dinput8.def $(LIBS)

clean:
	rm -rf build

.PHONY: clean
