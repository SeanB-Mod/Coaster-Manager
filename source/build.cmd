@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
where clang-cl >nul 2>nul || (echo Requires clang-cl and lld-link from LLVM. & exit /b 1)
where lld-link >nul 2>nul || (echo Requires lld-link. & exit /b 1)
lld-link /lib /def:imports\kernel32.def /machine:x64 /out:build\kernel32.lib || exit /b 1
lld-link /lib /def:imports\user32.def /machine:x64 /out:build\user32.lib || exit /b 1
clang-cl /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:build\coaster_selection_probe.obj coaster_selection_probe.c || exit /b 1
if not exist ..\Mods mkdir ..\Mods
lld-link /dll /timestamp:0 /machine:x64 /nodefaultlib /entry:DllMain /out:..\Mods\TPM-Coaster-Tools.dll build\coaster_selection_probe.obj build\kernel32.lib build\user32.lib || exit /b 1
echo Build complete.
