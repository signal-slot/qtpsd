# Copyright (C) 2026 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

qt_internal_add_plugin(QPsdExporterQtForMcuPlugin
    OUTPUT_NAME qqtformcu
    PLUGIN_TYPE psdexporter
    SOURCES
        qtformcu.cpp
    LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::PsdExporter
        Qt::PsdGui
)

qt_internal_add_resource(QPsdExporterQtForMcuPlugin "qtformcu"
    PREFIX "/qtformcu"
    FILES
        qtformcu.png
)
