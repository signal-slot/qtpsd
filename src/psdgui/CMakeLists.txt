# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#####################################################################
## PsdGui Module:
#####################################################################
qt_internal_add_module(PsdGui
    SOURCES
        qpsdguiglobal.h qpsdguiglobal.cpp
        qpsdabstractlayeritem.h qpsdabstractlayeritem.cpp
        qpsdtextlayeritem.h qpsdtextlayeritem.cpp
        qpsdshapelayeritem.h qpsdshapelayeritem.cpp
        qpsdfolderlayeritem.h qpsdfolderlayeritem.cpp
        qpsdimagelayeritem.h qpsdimagelayeritem.cpp
        qpsdborder.h qpsdborder.cpp
        qpsdpatternfill.h qpsdpatternfill.cpp
        qpsdguilayertreeitemmodel.h qpsdguilayertreeitemmodel.cpp
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC_LIBRARIES
        Qt::Gui
        Qt::PsdCore
)

## Scopes:
#####################################################################
