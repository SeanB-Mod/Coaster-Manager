@echo off
setlocal
cd /d "%~dp0"
if not defined LLVM_BIN set "LLVM_BIN=C:\Tools\llvm-mingw\bin"
if not exist build mkdir build
if not exist ..\Mods mkdir ..\Mods
"%LLVM_BIN%\ld.lld.exe" -flavor link /lib /def:imports\kernel32.def /machine:x64 /out:build\kernel32.lib || exit /b 1
"%LLVM_BIN%\ld.lld.exe" -flavor link /lib /def:imports\user32.def /machine:x64 /out:build\user32.lib || exit /b 1
"%LLVM_BIN%\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:build\coaster_selection_probe.obj coaster_selection_probe.c || exit /b 1
"%LLVM_BIN%\ld.lld.exe" -flavor link /dll /timestamp:0 /machine:x64 /nodefaultlib /entry:DllMain /out:..\Mods\TPM-Coaster-Tools.dll build\coaster_selection_probe.obj build\kernel32.lib build\user32.lib || exit /b 1
echo Build complete.
