# Copyright (C) 2025 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#####################################################################
## PsdGui Module:
#####################################################################
qt_internal_add_module(PsdWidget
    SOURCES
        qpsdwidgetglobal.h
        qpsdview.h qpsdview.cpp
        qpsdscene.h qpsdscene.cpp
        qpsdabstractitem.h qpsdabstractitem.cpp
        qpsdtextitem.h qpsdtextitem.cpp
        qpsdshapeitem.h qpsdshapeitem.cpp
        qpsdimageitem.h qpsdimageitem.cpp
        qpsdfolderitem.h qpsdfolderitem.cpp
        qpsdwidgettreeitemmodel.h qpsdwidgettreeitemmodel.cpp
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC_LIBRARIES
        Qt::Widgets
        Qt::PsdGui
)

## Scopes:
#####################################################################
