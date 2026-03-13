# Copyright (C) 2025 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

qt_internal_add_plugin(QPsdImporterFigmaPlugin
    OUTPUT_NAME qfigmaimporter
    PLUGIN_TYPE psdimporter
    SOURCES
        figma.cpp
    LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
        Qt::PsdCore
        Qt::PsdGui
        Qt::PsdWidget
        Qt::PsdImporter
)

qt_internal_add_resource(QPsdImporterFigmaPlugin "figma_icon"
    PREFIX "/figma"
    FILES
        figma.png
)
