# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

qt_internal_add_plugin(QPsdExporterSlintPlugin
    OUTPUT_NAME qslint
    PLUGIN_TYPE psdexporter
    SOURCES
        slint.cpp
    LIBRARIES
        Qt::Core
        Qt::PsdExporter
)

qt_internal_add_resource(QPsdExporterSlintPlugin "slint"
    PREFIX "/slint"
    FILES
        slint.png
)
