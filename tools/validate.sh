#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    echo "Usage: $0 [--all|--static|--firmware]" >&2
}

run_static_checks() {
    local actionlint_bin
    local test_dir

    python3 tools/check_repo.py

    actionlint_bin="${ACTIONLINT_BIN:-}"
    if [[ -z "${actionlint_bin}" ]]; then
        actionlint_bin="$(command -v actionlint || true)"
    fi
    if [[ -z "${actionlint_bin}" || ! -x "${actionlint_bin}" ]]; then
        actionlint_bin="$(./tools/install-actionlint.sh)"
    fi
    "${actionlint_bin}" -color .github/workflows/*.yml

    PYTHONDONTWRITEBYTECODE=1 python3 tools/make_asset_pack.py
    PYTHONDONTWRITEBYTECODE=1 python3 tools/make_trailblaze_pack.py

    test_dir="$(mktemp -d /tmp/ai-passport-host-tests.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_pixel_math.c main/ui_pixel_math.c \
        -o "${test_dir}/test_ui_pixel_math"
    "${test_dir}/test_ui_pixel_math"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_demo_navigation.c main/demo_navigation.c \
        -o "${test_dir}/test_demo_navigation"
    "${test_dir}/test_demo_navigation"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_companion_model.c main/companion_model.c \
        -o "${test_dir}/test_companion_model"
    "${test_dir}/test_companion_model"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_display_greeting.c main/app_model.c main/companion_model.c main/focus_log.c main/chat_model.c main/chat_history.c -o "${test_dir}/test_display_greeting"
    "${test_dir}/test_display_greeting"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_key_sound.c main/key_sound.c -lm -o "${test_dir}/test_key_sound"
    "${test_dir}/test_key_sound"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_anniv_bgm.c main/anniv_bgm.c -o "${test_dir}/test_anniv_bgm"
    "${test_dir}/test_anniv_bgm"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_play_sounds.c main/play_sounds.c main/anniv_bgm.c -o "${test_dir}/test_play_sounds"
    "${test_dir}/test_play_sounds"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_app_battle.c main/app_model.c main/companion_model.c main/focus_log.c \
        main/chat_model.c main/chat_history.c -o "${test_dir}/test_app_battle"
    "${test_dir}/test_app_battle"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain -Imain/generated/battle \
        tests/test_enemy_b_art.c main/enemy_b_art.c main/generated/battle/enemy_b_pack.c -o "${test_dir}/test_enemy_b_art"
    "${test_dir}/test_enemy_b_art" "${test_dir}/enemy-b-decoded.rgb565a8"
    PYTHONDONTWRITEBYTECODE=1 python3 tools/prepare_enemy_b.py --check --decoded "${test_dir}/enemy-b-decoded.rgb565a8"
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_enemy_b_native.py
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain -Imain/generated/anniv \
        tests/test_anniv_art.c main/anniv_art.c main/generated/anniv/anniv_pack.c -o "${test_dir}/test_anniv_art"
    "${test_dir}/test_anniv_art" "${test_dir}/anniv-decoded.rgb565"
    PYTHONDONTWRITEBYTECODE=1 python3 tools/prepare_anniv_frames.py --check --decoded "${test_dir}/anniv-decoded.rgb565"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/battle_nvs_stubs -Imain \
        tests/test_battle_store.c main/app_store.c main/app_model.c main/companion_model.c \
        main/focus_log.c main/chat_model.c main/chat_history.c -o "${test_dir}/test_battle_store"
    "${test_dir}/test_battle_store"
    PYTHONDONTWRITEBYTECODE=1 python3 tools/prepare_battle.py --check
    PYTHONDONTWRITEBYTECODE=1 python3 tools/prepare_play_sounds.py --check
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_play_sound_pack.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_anniv_bgm_pack.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_anniv_full_music.py
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_app_model.c main/app_model.c main/companion_model.c main/focus_log.c main/chat_model.c main/chat_history.c \
        -o "${test_dir}/test_app_model"
    "${test_dir}/test_app_model"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_focus_log.c main/focus_log.c -o "${test_dir}/test_focus_log"
    "${test_dir}/test_focus_log"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_copy.c main/ui_copy.c main/app_model.c main/companion_model.c main/focus_log.c main/chat_model.c main/chat_history.c \
        -o "${test_dir}/test_ui_copy"
    "${test_dir}/test_ui_copy"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_chat_model.c main/chat_model.c -o "${test_dir}/test_chat_model"
    "${test_dir}/test_chat_model"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_chat_history.c main/chat_history.c -o "${test_dir}/test_chat_history"
    "${test_dir}/test_chat_history"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_chat_saved.c main/chat_history.c -o "${test_dir}/test_chat_saved"
    "${test_dir}/test_chat_saved"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_chat_journal.c main/chat_journal.c main/chat_history.c -o "${test_dir}/test_chat_journal"
    "${test_dir}/test_chat_journal"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_chat_wire.c main/chat_wire.c -o "${test_dir}/test_chat_wire"
    "${test_dir}/test_chat_wire"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_chat_cache.c main/chat_cache.c -o "${test_dir}/test_chat_cache"
    "${test_dir}/test_chat_cache"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_chat_codec_pool.c main/chat_codec_pool.c -o "${test_dir}/test_chat_codec_pool"
    "${test_dir}/test_chat_codec_pool"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/chat_codec_stubs -Imain tests/test_chat_codec_memory.c main/chat_codec_memory.c main/chat_codec_pool.c -o "${test_dir}/test_chat_codec_memory"
    "${test_dir}/test_chat_codec_memory"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/chat_tls_stubs -Imain -Imain/generated/chat_ca tests/test_chat_tls.c main/chat_tls.c main/generated/chat_ca/chat_ca.c -o "${test_dir}/test_chat_tls"
    "${test_dir}/test_chat_tls"
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_chat_ca.py
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_app_chat.c main/app_model.c main/companion_model.c main/focus_log.c main/chat_model.c main/chat_history.c -o "${test_dir}/test_app_chat"
    "${test_dir}/test_app_chat"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_charge_detect.c main/charge_detect.c \
        -o "${test_dir}/test_charge_detect"
    "${test_dir}/test_charge_detect"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_net_form.c main/net_form.c \
        -o "${test_dir}/test_net_form"
    "${test_dir}/test_net_form"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_key_guard.c -o "${test_dir}/test_key_guard"
    "${test_dir}/test_key_guard"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/src \
        tests/test_bsp_display_rounding.c components/bsp/src/bsp_display_rounding.c \
        -o "${test_dir}/test_bsp_display_rounding"
    "${test_dir}/test_bsp_display_rounding"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/src \
        tests/test_bsp_es8311_sleep_check.c components/bsp/src/bsp_es8311_sleep_check.c \
        -o "${test_dir}/test_bsp_es8311_sleep_check"
    "${test_dir}/test_bsp_es8311_sleep_check"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/bsp_stubs -Icomponents/bsp/include \
        tests/test_bsp_button.c -o "${test_dir}/test_bsp_button"
    "${test_dir}/test_bsp_button"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/bsp_stubs -Icomponents/bsp/include \
        tests/test_bsp_lvgl_init.c components/bsp/src/bsp_display_rounding.c \
        -o "${test_dir}/test_bsp_lvgl_init"
    "${test_dir}/test_bsp_lvgl_init"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/audio_stubs -Icomponents/bsp/include -Icomponents/bsp/src \
        tests/test_bsp_audio_recovery.c components/bsp/src/bsp_es8311_sleep_check.c \
        -o "${test_dir}/test_bsp_audio_recovery"
    "${test_dir}/test_bsp_audio_recovery"
    for demo in audio low_power ble wifi; do
        "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
            -ffunction-sections -fdata-sections -Itests/demo_stubs -Imain \
            "tests/test_demo_${demo}_runtime.c" -Wl,--gc-sections \
            -o "${test_dir}/test_demo_${demo}_runtime"
        "${test_dir}/test_demo_${demo}_runtime"
    done
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_deep_sleep_contract.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_trailblaze_pack.py
    PYTHONDONTWRITEBYTECODE=1 python3 tools/prepare_trailblaze.py --check
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_companion_assets.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_asset_pack.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_chat_font.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_check_repo.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_verify_firmware.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_archive_firmware.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_install_passport_skills.py
    rm -rf "${test_dir}"
    echo "Host tests: PASS"
}

run_firmware_checks() (
    local validation_build_dir

    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: idf.py is not available; activate ESP-IDF 5.5.3 first." >&2
        return 1
    fi

    PYTHONDONTWRITEBYTECODE=1 python3 tools/make_asset_pack.py
    PYTHONDONTWRITEBYTECODE=1 python3 tools/make_trailblaze_pack.py

    validation_build_dir="$(mktemp -d /tmp/ai-passport-firmware.XXXXXX)"
    trap 'case "${validation_build_dir}" in /tmp/ai-passport-firmware.*) rm -rf -- "${validation_build_dir}" ;; esac' EXIT

    SDKCONFIG_DEFAULTS="${repo_root}/sdkconfig.defaults" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" build
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    python3 tools/verify_firmware.py "${validation_build_dir}"
    PYTHONDONTWRITEBYTECODE=1 python3 tools/archive_firmware.py create \
        "${validation_build_dir}" --archive-root "${repo_root}/build/firmware"
    mkdir -p "${repo_root}/build"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${repo_root}/build/FoloToy-AI-Passport-full.bin"
    echo "Firmware build: PASS"
)

cd "${repo_root}"
case "${mode}" in
    --all)
        run_static_checks
        run_firmware_checks
        ;;
    --static)
        run_static_checks
        ;;
    --firmware)
        run_firmware_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
