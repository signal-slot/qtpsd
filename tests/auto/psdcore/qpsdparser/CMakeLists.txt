# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

qt_internal_add_test(tst_qpsdparser
    SOURCES
        tst_qpsdparser.cpp
    LIBRARIES
        Qt::PsdCore
        Qt::Test
        Qt::TestPrivate
)