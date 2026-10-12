@echo off
REM SM2K Co-op: build the 32-bit proxy dinput8.dll with Visual Studio (MSVC).
REM Run from "x86 Native Tools Command Prompt for VS" (it must be x86, the game is 32-bit).
REM Result: build\dinput8.dll  ->  copy it next to SpideyPC.exe

setlocal
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /O2 /MT /LD /W3 src\dinput8.c /Fobuild\ /Febuild\dinput8.dll ^
   /link /DEF:src\dinput8.def user32.lib kernel32.lib ws2_32.lib
if errorlevel 1 (
  echo.
  echo BUILD FAILED. Check that you opened the x86 ^(not x64^) Native Tools prompt.
  exit /b 1
)
echo.
echo OK: build\dinput8.dll
endlocal
