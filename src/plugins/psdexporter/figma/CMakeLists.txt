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

qt_internal_add_resource(QPsdExporterFigmaPlugin "figma_icon"
    PREFIX "/figma"
    FILES
        figma.png
)

set(FIGMA_PLUGIN_DIR "${PROJECT_SOURCE_DIR}/tools/figma-plugin")

qt_internal_add_resource(QPsdExporterFigmaPlugin "figma_plugin"
    PREFIX "/figma/plugin"
    BASE "${FIGMA_PLUGIN_DIR}"
    FILES
        "${FIGMA_PLUGIN_DIR}/manifest.json"
        "${FIGMA_PLUGIN_DIR}/dist/main.js"
        "${FIGMA_PLUGIN_DIR}/dist/ui.html"
)
