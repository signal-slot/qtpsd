# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

qt_internal_add_plugin(QPsdExporterQtQuickPlugin
    OUTPUT_NAME qqtquick
    PLUGIN_TYPE psdexporter
    SOURCES
        qtquick.cpp
    LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::PsdExporter
        Qt::PsdGui
)

qt_internal_add_resource(QPsdExporterQtQuickPlugin "qtquick"
    PREFIX "/qtquick"
    FILES
        qtquick.png
        blend.frag.qsb
)
