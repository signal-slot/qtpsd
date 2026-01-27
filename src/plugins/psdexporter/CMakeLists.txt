# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

add_subdirectory(json)
add_subdirectory(images)

# These plugins have resource targets that cause CMake export issues
# Skip them for WASM builds or when explicitly disabled
if(NOT EMSCRIPTEN AND NOT QT_PSD_SKIP_EXPORTER_PLUGINS)
    add_subdirectory(qtquick)
    add_subdirectory(slint)
    add_subdirectory(flutter)
    add_subdirectory(reactnative)
    add_subdirectory(swiftui)
    add_subdirectory(lvgl)
endif()
