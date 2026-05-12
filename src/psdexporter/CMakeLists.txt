# Copyright (C) 2025 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#####################################################################
## PsdExporter Module:
#####################################################################
qt_internal_add_module(PsdExporter
    PLUGIN_TYPES psdexporter
    SOURCES
        qpsdexporterglobal.h
        qpsdexporterplugin.h qpsdexporterplugin.cpp
        qpsdqmlexporterplugin.h qpsdqmlexporterplugin.cpp
        qpsdimagestore.h qpsdimagestore.cpp
        qpsdexportertreeitemmodel.h qpsdexportertreeitemmodel.cpp
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC_LIBRARIES
        Qt::Gui
        Qt::PsdGui
)

## Scopes:
#####################################################################
