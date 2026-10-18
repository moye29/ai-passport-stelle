cmake_minimum_required(VERSION 3.16)
project(pet_preview C CXX)
set(LVGL_SOURCE_DIR "" CACHE PATH "Existing LVGL 9 source checkout")
set(PREVIEW_SOURCES "" CACHE STRING "Absolute adapter, view, controller, model and sprite C sources")
set(PREVIEW_INCLUDE_DIRS "" CACHE STRING "Application include directories")
if(NOT EXISTS "${LVGL_SOURCE_DIR}/CMakeLists.txt" OR NOT PREVIEW_SOURCES)
    message(FATAL_ERROR "Set LVGL_SOURCE_DIR and PREVIEW_SOURCES. See native-preview.md.")
endif()
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${LVGL_SOURCE_DIR}" lvgl)
add_executable(pet_preview preview_main.c ${PREVIEW_SOURCES})
target_include_directories(pet_preview PRIVATE "${CMAKE_CURRENT_LIST_DIR}" ${PREVIEW_INCLUDE_DIRS})
target_link_libraries(pet_preview PRIVATE lvgl m)
target_compile_options(pet_preview PRIVATE -Wall -Wextra -Werror)

# Exercise the actual Flash image reader against real LVGL descriptors.
add_executable(trailblaze_binding_tests ../../tests/test_trailblaze_binding.c
    ../../main/asset_pack.c ../../main/generated/pack/stelle_pack.c)
target_include_directories(trailblaze_binding_tests PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}" ${PREVIEW_INCLUDE_DIRS})
target_link_libraries(trailblaze_binding_tests PRIVATE lvgl m)
target_compile_options(trailblaze_binding_tests PRIVATE -Wall -Wextra -Werror)

add_executable(anniv_image_tests ../../tests/test_anniv_image.c ../../main/anniv_art.c
    ../../main/anniv_image.c ../../main/generated/anniv/anniv_pack.c)
target_include_directories(anniv_image_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}" ${PREVIEW_INCLUDE_DIRS})
target_link_libraries(anniv_image_tests PRIVATE lvgl m)
target_compile_options(anniv_image_tests PRIVATE -Wall -Wextra -Werror)

add_executable(enemy_b_image_tests ../../tests/test_enemy_b_image.c ../../main/enemy_b_art.c
    ../../main/enemy_b_image.c ../../main/generated/battle/enemy_b_pack.c)
target_include_directories(enemy_b_image_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}" ${PREVIEW_INCLUDE_DIRS})
target_link_libraries(enemy_b_image_tests PRIVATE lvgl m)
target_compile_options(enemy_b_image_tests PRIVATE -Wall -Wextra -Werror)
