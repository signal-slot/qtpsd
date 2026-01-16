# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

qt_internal_add_plugin(QPsdExporterSwiftUIPlugin
    OUTPUT_NAME qswiftui
    PLUGIN_TYPE psdexporter
    SOURCES
        swiftui.cpp
    LIBRARIES
        Qt::Core
        Qt::PsdExporter
)

qt_add_resources(QPsdExporterSwiftUIPlugin "swiftui"
    PREFIX "/swiftui"
    FILES
        swiftui.png
)
