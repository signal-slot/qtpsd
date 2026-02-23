# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

qt_internal_add_plugin(QPsdExporterFigmaPlugin
    OUTPUT_NAME qfigma
    PLUGIN_TYPE psdexporter
    SOURCES
        figma.cpp
    LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::PsdExporter
)
