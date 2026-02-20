# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

qt_internal_add_test(tst_qpsdwriter
    SOURCES
        tst_qpsdwriter.cpp
    LIBRARIES
        Qt::Gui
        Qt::PsdCore
        Qt::PsdGui
        Qt::PsdWidget
        Qt::PsdWriter
        Qt::Test
        Qt::TestPrivate
        Qt::Widgets
)
