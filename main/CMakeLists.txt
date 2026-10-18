idf_component_register(
    SRCS "main.c"
         "key_sound.c"
         "anniv_bgm.c"
         "anniv_art.c"
         "anniv_image.c"
         "enemy_b_art.c"
         "enemy_b_image.c"
         "play_sounds.c"
         "app_model.c"
         "app_store.c"
         "charge_detect.c"
         "net.c"
         "net_form.c"
         "companion_model.c"
         "pet_view.c"
         "ui_copy.c"
         "ui_widgets.c"
         "generated/ui/ui_assets.c"
         "asset_pack.c"
         "focus_log.c"
         "chat_model.c"
         "chat_history.c"
         "chat_journal.c"
         "chat_store.c"
         "chat_service.c"
         "chat_wire.c"
         "chat_cache.c"
         "chat_codec_pool.c"
         "chat_codec_memory.c"
         "chat_tls.c"
         "generated/chat_ca/chat_ca.c"
         "../assets/fonts/stelle-chat/stelle_chat_font.c"
         "generated/pack/stelle_pack.c"
         "generated/battle/battle_pack.c"
         "generated/anniv/anniv_pack.c"
         "generated/battle/enemy_b_pack.c"
    INCLUDE_DIRS "." "generated/ui" "generated/pack" "generated/chat_ca" "generated/battle" "generated/anniv"
    REQUIRES bsp esp_timer nvs_flash esp_partition esp_driver_usb_serial_jtag
             esp_wifi esp_netif esp_event esp_http_server lwip dns_server
             esp_http_client mqtt mbedtls json esp_app_format
)

# Include the unchanged Stelle resource partition in merge-bin output. The
# static gate generates and validates this pack before the isolated build.
set(stelle_asset_pack "${CMAKE_SOURCE_DIR}/build/assets/stelle_assets.bin")
if(NOT EXISTS "${stelle_asset_pack}")
    message(FATAL_ERROR "Generate Stelle assets first: python tools/make_asset_pack.py (requires Pillow)")
endif()
idf_build_get_property(stelle_build_dir BUILD_DIR)
file(MAKE_DIRECTORY "${stelle_build_dir}/assets")
set(anniv_music "${CMAKE_SOURCE_DIR}/assets/music/stelle-anniversary/selected/star-rail-chip-full.sbgm")
set(play_sounds "${CMAKE_SOURCE_DIR}/assets/music/stelle-play-sounds/device/play_sounds.bin")
execute_process(COMMAND "${PYTHON}" "${CMAKE_SOURCE_DIR}/tools/prepare_anniv_bgm.py"
    --combine "${stelle_asset_pack}" --source "${anniv_music}" --effects "${play_sounds}"
    --output "${stelle_build_dir}/assets/stelle_assets.bin"
    RESULT_VARIABLE anniv_pack_result)
if(NOT anniv_pack_result EQUAL 0)
    message(FATAL_ERROR "Cannot pack anniversary BGM into the existing assets partition")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${anniv_music}" "${play_sounds}" "${CMAKE_SOURCE_DIR}/tools/prepare_anniv_bgm.py")
esptool_py_flash_to_partition(flash "assets" "${stelle_build_dir}/assets/stelle_assets.bin")

# Destinations use only the unallocated gap before Recovery. Existing partition
# offsets stay unchanged. The complete destination pack is required for delivery.
set(trailblaze_pack "${CMAKE_SOURCE_DIR}/build/assets/trailblaze.bin")
if(EXISTS "${trailblaze_pack}")
    configure_file("${trailblaze_pack}" "${stelle_build_dir}/assets/trailblaze.bin" COPYONLY)
    esptool_py_flash_to_partition(flash "trailblaze" "${stelle_build_dir}/assets/trailblaze.bin")
else()
    message(FATAL_ERROR "Generate Trailblaze artwork first: python tools/prepare_trailblaze.py then python tools/make_trailblaze_pack.py")
endif()
