# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

qt_internal_add_plugin(QPsdAdditionalLayerInformationPattPlugin
    OUTPUT_NAME qpatt
    PLUGIN_TYPE psdadditionallayerinformation
    SOURCES
        patt.cpp
    LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::PsdCore
)
