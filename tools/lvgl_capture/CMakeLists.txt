cmake_minimum_required(VERSION 3.16)
project(lvgl_capture C)

set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG        v9.3.0
    GIT_SHALLOW    TRUE
)

# Point LVGL to our lv_conf.h
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)

FetchContent_MakeAvailable(lvgl)

add_executable(lvgl_capture main.c)
target_link_libraries(lvgl_capture PRIVATE lvgl lvgl::thorvg)
target_include_directories(lvgl_capture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
