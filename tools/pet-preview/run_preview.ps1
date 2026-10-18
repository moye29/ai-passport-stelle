# Build and run the same-source native LVGL preview on Windows (zig cc + ESP-IDF's cmake/ninja).
# Usage: pwsh tools/pet-preview/run_preview.ps1 [-BuildDir <dir>] [-OutDir <dir>] [-MemKB 24]
param(
    [string]$BuildDir = "$env:TEMP\stelle-preview-build",
    [string]$OutDir = "",
    [int]$MemKB = 24,
    [int]$Cycles = 24,  # create/destroy cycles for the leak check
    [string]$Target = "x86-windows-gnu",  # 32-bit like the ESP32-C3, so LVGL pool usage is comparable
    [string]$Zig = "$env:USERPROFILE\passport-host-zig\zig.exe"
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..\..").Path.Replace('\', '/')
$tool = "$root/tools/pet-preview"
if (-not $OutDir) { $OutDir = "$root/build/preview-frames" }
$cmake = Get-ChildItem "$env:USERPROFILE\.espressif\tools\cmake" -Recurse -Filter cmake.exe | Select-Object -First 1 -ExpandProperty FullName
$ninja = Get-ChildItem "$env:USERPROFILE\.espressif\tools\ninja" -Recurse -Filter ninja.exe | Select-Object -First 1 -ExpandProperty FullName
if (-not (Test-Path $Zig)) { throw "zig not found at $Zig (pass -Zig)" }
$env:PREVIEW_ZIG = $Zig
$sources = @("$tool/adapter.c", "$root/main/pet_view.c", "$root/main/ui_widgets.c", "$root/main/ui_copy.c",
    "$root/main/app_model.c", "$root/main/companion_model.c", "$root/main/generated/ui/ui_assets.c",
    "$root/main/asset_pack.c", "$root/main/focus_log.c", "$root/main/generated/pack/stelle_pack.c", "$root/main/generated/battle/battle_pack.c",
    "$root/main/anniv_art.c", "$root/main/anniv_image.c", "$root/main/generated/anniv/anniv_pack.c",
    "$root/main/enemy_b_art.c", "$root/main/enemy_b_image.c", "$root/main/generated/battle/enemy_b_pack.c",
    "$root/main/chat_model.c", "$root/main/chat_history.c", "$root/assets/fonts/stelle-chat/stelle_chat_font.c") -join ';'
$includes = @("$root/main", "$root/main/generated/ui", "$root/main/generated/pack", "$root/main/generated/battle", "$root/main/generated/anniv") -join ';'
$env:STELLE_PACK = "$root/build/assets/stelle_assets.bin"  # same pack as the resource partition
$env:TRAILBLAZE_PACK = "$root/build/assets/trailblaze.bin"
& $cmake -S $tool -B $BuildDir -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_C_COMPILER=$tool/cc.cmd" `
    "-DCMAKE_CXX_COMPILER=$tool/cxx.cmd" "-DCMAKE_AR=$tool/ar.cmd" "-DCMAKE_RANLIB=$tool/ranlib.cmd" `
    -DCMAKE_BUILD_TYPE=Release "-DCMAKE_C_FLAGS=-target $Target -DPREVIEW_LV_MEM_KB=${MemKB}U -DPREVIEW_CYCLES=${Cycles}" "-DCMAKE_CXX_FLAGS=-target $Target" "-DCMAKE_EXE_LINKER_FLAGS=-target $Target" `
    "-DLVGL_SOURCE_DIR=$root/managed_components/lvgl__lvgl" "-DPREVIEW_SOURCES=$sources" "-DPREVIEW_INCLUDE_DIRS=$includes" | Out-Null
if ($LASTEXITCODE) { throw "cmake configure failed" }
& $cmake --build $BuildDir -j 16 | Select-String -Pattern 'error|warning' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { throw "build failed" }
& "$BuildDir/trailblaze_binding_tests.exe"
if ($LASTEXITCODE) { throw "Trailblaze Flash binding checks failed" }
& "$BuildDir/anniv_image_tests.exe"
if ($LASTEXITCODE) { throw "Anniversary decoder checks failed" }
& "$BuildDir/enemy_b_image_tests.exe"
if ($LASTEXITCODE) { throw "Enemy B decoder checks failed" }
if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Force $OutDir | Out-Null
& "$BuildDir/pet_preview.exe" $OutDir
exit $LASTEXITCODE
