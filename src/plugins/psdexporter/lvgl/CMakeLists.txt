# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

qt_internal_add_plugin(QPsdExporterLvglPlugin
    OUTPUT_NAME qlvgl
    PLUGIN_TYPE psdexporter
    SOURCES
        lvgl.cpp
    LIBRARIES
        Qt::Core
        Qt::PsdExporter
)

qt_add_resources(QPsdExporterLvglPlugin "lvgl"
    PREFIX "/lvgl"
    FILES
        lvgl.png
)
