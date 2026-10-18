# Windows mirror of the host-test part of tools/validate.sh --static (no bash here).
# C tests are built with zig cc; Python tests run with Pillow via uv when available.
# Usage: pwsh tools/validate_host.ps1 [-Zig <zig.exe>]
param([string]$Zig = "$env:USERPROFILE\passport-host-zig\zig.exe")
$ErrorActionPreference = "Stop"
Set-Location (Resolve-Path "$PSScriptRoot\..")
$out = Join-Path $env:TEMP "stelle-host-tests"
New-Item -ItemType Directory -Force $out | Out-Null
$flags = @("-std=c11", "-Wall", "-Wextra", "-Werror")
$tests = @(
    @("test_ui_pixel_math", "-Imain", "tests/test_ui_pixel_math.c", "main/ui_pixel_math.c"),
    @("test_demo_navigation", "-Imain", "tests/test_demo_navigation.c", "main/demo_navigation.c"),
    @("test_companion_model", "-Imain", "tests/test_companion_model.c", "main/companion_model.c"),
    @("test_display_greeting", "-Imain", "tests/test_display_greeting.c", "main/app_model.c", "main/companion_model.c", "main/focus_log.c", "main/chat_model.c", "main/chat_history.c"),
    @("test_key_sound", "-Imain", "tests/test_key_sound.c", "main/key_sound.c", "-lm"),
    @("test_anniv_bgm", "-Imain", "tests/test_anniv_bgm.c", "main/anniv_bgm.c"),
    @("test_anniv_art", "-Imain", "-Imain/generated/anniv", "tests/test_anniv_art.c", "main/anniv_art.c", "main/generated/anniv/anniv_pack.c"),
    @("test_enemy_b_art", "-Imain", "-Imain/generated/battle", "tests/test_enemy_b_art.c", "main/enemy_b_art.c", "main/generated/battle/enemy_b_pack.c"),
    @("test_play_sounds", "-Imain", "tests/test_play_sounds.c", "main/play_sounds.c", "main/anniv_bgm.c"),
    @("test_app_model", "-Imain", "tests/test_app_model.c", "main/app_model.c", "main/companion_model.c", "main/focus_log.c", "main/chat_model.c", "main/chat_history.c"),
    @("test_app_battle", "-Imain", "tests/test_app_battle.c", "main/app_model.c", "main/companion_model.c", "main/focus_log.c", "main/chat_model.c", "main/chat_history.c"),
    @("test_battle_store", "-Itests/battle_nvs_stubs", "-Imain", "tests/test_battle_store.c", "main/app_store.c", "main/app_model.c", "main/companion_model.c", "main/focus_log.c", "main/chat_model.c", "main/chat_history.c"),
    @("test_chat_model", "-Imain", "tests/test_chat_model.c", "main/chat_model.c"),
    @("test_chat_codec_pool", "-Imain", "tests/test_chat_codec_pool.c", "main/chat_codec_pool.c"),
    @("test_chat_codec_memory", "-Itests/chat_codec_stubs", "-Imain", "tests/test_chat_codec_memory.c", "main/chat_codec_memory.c", "main/chat_codec_pool.c"),
    @("test_chat_history", "-Imain", "tests/test_chat_history.c", "main/chat_history.c"),
    @("test_chat_wire", "-Imain", "tests/test_chat_wire.c", "main/chat_wire.c"),
    @("test_app_chat", "-Imain", "tests/test_app_chat.c", "main/app_model.c", "main/companion_model.c", "main/focus_log.c", "main/chat_model.c", "main/chat_history.c"),
    @("test_focus_log", "-Imain", "tests/test_focus_log.c", "main/focus_log.c"),
    @("test_ui_copy", "-Imain", "tests/test_ui_copy.c", "main/ui_copy.c", "main/app_model.c", "main/companion_model.c", "main/focus_log.c", "main/chat_model.c", "main/chat_history.c"),
    @("test_charge_detect", "-Imain", "tests/test_charge_detect.c", "main/charge_detect.c"),
    @("test_net_form", "-Imain", "tests/test_net_form.c", "main/net_form.c"),
    @("test_key_guard", "-Imain", "tests/test_key_guard.c"),
    @("test_bsp_display_rounding", "-Icomponents/bsp/src", "tests/test_bsp_display_rounding.c", "components/bsp/src/bsp_display_rounding.c"),
    @("test_bsp_es8311_sleep_check", "-Icomponents/bsp/src", "tests/test_bsp_es8311_sleep_check.c", "components/bsp/src/bsp_es8311_sleep_check.c")
)
$failed = 0
foreach ($t in $tests) {
    $name = $t[0]
    $exe = Join-Path $out "$name.exe"
    & $Zig cc @flags @($t[1..($t.Length - 1)]) -o $exe
    if ($LASTEXITCODE) { Write-Output "BUILD FAIL $name"; $failed++; continue }
    & $exe
    if ($LASTEXITCODE) { Write-Output "FAIL $name"; $failed++ } else { Write-Output "ok   $name" }
}
$py = @("tests/test_play_sound_pack.py", "tests/test_anniv_bgm_pack.py", "tests/test_anniv_full_music.py", "tests/test_companion_assets.py", "tests/test_asset_pack.py", "tests/test_trailblaze_pack.py", "tests/test_chat_font.py")
foreach ($p in $py) {
    $env:PYTHONDONTWRITEBYTECODE = "1"
    uv run --offline --no-project --with pillow python $p 2>&1 | Select-Object -Last 3
    if ($LASTEXITCODE) { Write-Output "FAIL $p"; $failed++ } else { Write-Output "ok   $p" }
}
if ($failed) { Write-Output "Host tests: FAIL ($failed)"; exit 1 }
Write-Output "Host tests: PASS (subset mirrored from validate.sh; see notes)"
