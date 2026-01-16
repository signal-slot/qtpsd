# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

qt_internal_add_plugin(QPsdExporterReactNativePlugin
    OUTPUT_NAME qreactnative
    PLUGIN_TYPE psdexporter
    SOURCES
        reactnative.cpp
    LIBRARIES
        Qt::Core
        Qt::PsdExporter
)

qt_add_resources(QPsdExporterReactNativePlugin "reactnative"
    PREFIX "/reactnative"
    FILES
        reactnative.png
)
