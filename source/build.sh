#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build ../Mods
lld-link /lib /def:imports/kernel32.def /machine:x64 /out:build/kernel32.lib
lld-link /lib /def:imports/user32.def /machine:x64 /out:build/user32.lib
clang-cl /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:build/coaster_selection_probe.obj coaster_selection_probe.c
lld-link /dll /timestamp:0 /machine:x64 /nodefaultlib /entry:DllMain /out:../Mods/TPM-Coaster-Tools.dll build/coaster_selection_probe.obj build/kernel32.lib build/user32.lib
file ../Mods/TPM-Coaster-Tools.dll
