$ErrorActionPreference = 'Stop'
if (-not $env:LLVM_BIN) { $env:LLVM_BIN = 'C:\Tools\llvm-mingw\bin' }
if (-not $env:TPM_TEST_PYTHON) { $env:TPM_TEST_PYTHON = 'python' }
function Check { if ($LASTEXITCODE -ne 0) { throw "Test/build failed: $LASTEXITCODE" } }
$originalUserProfile=$env:USERPROFILE
$originalTemp=$env:TEMP
$originalTmp=$env:TMP
$env:TEMP=Join-Path $PSScriptRoot '.test-tmp'
New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null
$env:TMP=$env:TEMP
$env:USERPROFILE=Join-Path $env:TEMP 'coaster-native-test-profile'
New-Item -ItemType Directory -Force -Path $env:USERPROFILE | Out-Null
Push-Location $PSScriptRoot
try {
  cmd /c ..\build-llvm-mingw.cmd; Check
  & $env:TPM_TEST_PYTHON test_ui_compat.py; Check
  & $env:TPM_TEST_PYTHON test_native_compat.py; Check
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_log_budget.obj test_log_budget.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /dll /noentry /machine:x64 /nodefaultlib /out:test_log_budget.dll test_log_budget.obj ..\build\kernel32.lib ..\build\user32.lib; Check
  & $env:TPM_TEST_PYTHON test_log_budget.py; Check
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_footprint.obj test_footprint.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /dll /noentry /machine:x64 /nodefaultlib /out:test_footprint.dll test_footprint.obj; Check
  & $env:TPM_TEST_PYTHON test_footprint.py; Check

  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_library.obj test_library.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /dll /noentry /machine:x64 /nodefaultlib /out:test_library.dll test_library.obj ..\build\kernel32.lib ..\build\user32.lib; Check
  & $env:TPM_TEST_PYTHON test_library.py; Check
  & $env:TPM_TEST_PYTHON test_manual.py; Check
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_preview.obj test_preview.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /dll /noentry /machine:x64 /nodefaultlib /out:test_preview.dll test_preview.obj ..\build\kernel32.lib ..\build\user32.lib; Check
  & $env:TPM_TEST_PYTHON test_preview.py; Check

  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_save.obj test_save.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /dll /noentry /machine:x64 /nodefaultlib /out:test_save.dll test_save.obj ..\build\kernel32.lib ..\build\user32.lib; Check
  & $env:TPM_TEST_PYTHON test_save.py; Check
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_management.obj test_management.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /dll /noentry /machine:x64 /nodefaultlib /out:test_management.dll test_management.obj ..\build\kernel32.lib ..\build\user32.lib; Check
  & $env:TPM_TEST_PYTHON test_management.py; Check
  & $env:TPM_TEST_PYTHON test_coaster_package.py; Check
  & $env:TPM_TEST_PYTHON test_workshop.py; Check
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_upload.obj test_upload.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /dll /noentry /machine:x64 /nodefaultlib /out:test_upload.dll test_upload.obj ..\build\kernel32.lib ..\build\user32.lib; Check
  & $env:TPM_TEST_PYTHON test_upload.py; Check
  & $env:TPM_TEST_PYTHON test_prepare.py; Check
  & $env:TPM_TEST_PYTHON test_verify_import.py; Check
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_import_rotation.obj test_import_rotation.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /dll /noentry /machine:x64 /nodefaultlib /out:test_import_rotation.dll test_import_rotation.obj ..\build\kernel32.lib ..\build\user32.lib; Check
  & $env:TPM_TEST_PYTHON test_import_rotation.py; Check
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_json.obj test_json.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /dll /noentry /machine:x64 /nodefaultlib /out:test_json.dll test_json.obj; Check
  & $env:TPM_TEST_PYTHON test_json.py; Check
  $env:USERPROFILE=Join-Path $env:TEMP 'coaster-native-test-profile'
  New-Item -ItemType Directory -Force -Path (Join-Path $env:USERPROFILE 'AppData/LocalLow/Two Point Studios') | Out-Null
  Get-Content ..\imports\kernel32.def | Set-Content test-kernel32.def
  Add-Content test-kernel32.def ' ExitProcess'
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /lib /def:test-kernel32.def /machine:x64 /out:test-kernel32.lib; Check
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_export.obj test_export.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /machine:x64 /nodefaultlib /entry:TestMain /subsystem:console /out:test_export.exe test_export.obj test-kernel32.lib ..\build\user32.lib; Check
  & .\test_export.exe; Check
  & $env:TPM_TEST_PYTHON check_records.py; Check
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_preflight.obj test_preflight.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /machine:x64 /nodefaultlib /entry:TestMain /subsystem:console /out:test_preflight.exe test_preflight.obj test-kernel32.lib ..\build\user32.lib; Check
  & .\test_preflight.exe; Check
  Write-Output 'PASS: station binding, stale/duplicate/mismatched listeners, managed exceptions, definition resolution, floor query flags/signatures/mismatch and GC cleanup; unexpected invocations rejected.'
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_write.obj test_write.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /machine:x64 /nodefaultlib /entry:TestMain /subsystem:console /out:test_write.exe test_write.obj test-kernel32.lib ..\build\user32.lib; Check
  & .\test_write.exe; Check
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_placement_entry.obj test_placement_entry.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /machine:x64 /nodefaultlib /entry:TestMain /subsystem:console /out:test_placement_entry.exe test_placement_entry.obj test-kernel32.lib ..\build\user32.lib; Check
  & .\test_placement_entry.exe; Check
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_asset_array.obj test_asset_array.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /machine:x64 /nodefaultlib /entry:TestMain /subsystem:console /out:test_asset_array.exe test_asset_array.obj test-kernel32.lib ..\build\user32.lib; Check
  & .\test_asset_array.exe; Check
  Write-Output 'PASS: UI artwork inventories over 256 objects remain discoverable; import/config bound stays 256; oversized, negative, wrong-type and throwing arrays rejected.'
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_colour.obj test_colour.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /machine:x64 /nodefaultlib /entry:TestMain /subsystem:console /out:test_colour.exe test_colour.obj test-kernel32.lib ..\build\user32.lib; Check
  & .\test_colour.exe; Check
  Write-Output 'PASS: ride component colour capture preserves custom/default state and four channels; invalid flags/ranges/null reject without stale output.'
  & "$env:LLVM_BIN\clang.exe" --driver-mode=cl --target=x86_64-pc-windows-msvc /nologo /c /TC /W4 /WX /O1 /GS- /Zl /Fo:test_isolated_preview.obj test_isolated_preview.c; Check
  & "$env:LLVM_BIN\ld.lld.exe" -flavor link /machine:x64 /nodefaultlib /entry:TestMain /subsystem:console /out:test_isolated_preview.exe test_isolated_preview.obj test-kernel32.lib ..\build\user32.lib; Check
  & .\test_isolated_preview.exe; Check
  Write-Output 'PASS: isolated capture renderer/root bounds; rendering journal records before setters, reverses all mutations after each injected failure, continues on restore errors, rejects capacity overflow, and bounds/deduplicates track IDs; accepts verified spline or procedural rail ownership, rejects neither, resets route state; GPU cleanup ordering.'
  Write-Output 'PASS: F10 callback isolation, original forwarding, null/context rejection; Funfair row identity, menu ownership, unlock/affordability/button guards, exact handler target, one-attempt guard and exception lock.'
  Write-Output 'PASS: three sequential completion unlocks, reference cleanup, failure queue retention, explicit fresh-retry release and pending-request guard.'
  Write-Output 'PASS: fresh retry blocks active/queued/processing/unreadable work; probes preserve state; explicit reset releases station, frozen plan and retained failed event bus.'
  Write-Output 'PASS: expanded native fields, destination piece/chain/booster/toggle allowlists, decoration resolution, support limits and managed exception stops.'
  Write-Output 'PASS: import plan decode, file loading/reloading/missing file, 1/512 support construction, malformed input and exact queue-wrapper tracking.'
  Write-Output 'PASS: seven-support fixture construction, target frame/separation, floor IDs, empty-target gate, snapshot restoration before commit, exception stops after commit, count mismatch, one-shot enforcement and GC cleanup.'
} finally { $env:USERPROFILE=$originalUserProfile; $env:TEMP=$originalTemp; $env:TMP=$originalTmp; Pop-Location }

Write-Output "PASS: import accepts only exact StatChanged notifications; captures the appended TrackRevert with a notification prefix; rejects unknown/duplicate/misplaced wrappers, unreadable queues and active processing."
