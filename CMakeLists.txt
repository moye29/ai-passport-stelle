cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# The Stelle UI uses a square screen frame (like the Miki firmware): disable
# the BSP's rounded-corner mask for this app without changing the BSP default.
idf_build_set_property(COMPILE_DEFINITIONS "BSP_LVGL_SCREEN_RADIUS=0" APPEND)

project(FoloToy-AI-Passport)
