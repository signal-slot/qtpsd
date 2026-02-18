# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#####################################################################
## PsdWriter Module:
#####################################################################
qt_internal_add_module(PsdWriter
    SOURCES
        qpsdwriter.cpp qpsdwriter.h
        qpsdwriterglobal.h
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
    LIBRARIES
        Qt::CorePrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::PsdCore
    PRIVATE_MODULE_INTERFACE
        Qt::CorePrivate
)
