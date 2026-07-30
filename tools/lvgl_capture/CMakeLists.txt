cmake_minimum_required(VERSION 3.16)
project(lvgl_capture C)

set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG        v9.4.0
    GIT_SHALLOW    TRUE
)

# Point LVGL to our lv_conf.h
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)

FetchContent_MakeAvailable(lvgl)

# The harness has main() but no UI: the exporter-generated main_screen.c is
# compiled per export and linked against this by scripts/lvgl2png.sh.
add_library(capture_harness STATIC main.c)
target_link_libraries(capture_harness PRIVATE lvgl)
target_include_directories(capture_harness PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Build metadata consumed by scripts/lvgl2png.sh
configure_file(capture_env.sh.in capture_env.sh @ONLY)
