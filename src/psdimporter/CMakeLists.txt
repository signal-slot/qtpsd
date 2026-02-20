# Copyright (C) 2025 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#####################################################################
## PsdImporter Module:
#####################################################################
qt_internal_add_module(PsdImporter
    PLUGIN_TYPES psdimporter
    SOURCES
        qpsdimporterglobal.h
        qpsdimporterplugin.h qpsdimporterplugin.cpp
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::PsdExporter
)

## Scopes:
#####################################################################
