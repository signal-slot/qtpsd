# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

qt_internal_add_test(tst_image_file_name
    SOURCES
        tst_image_file_name.cpp
    LIBRARIES
        Qt::PsdExporter
        Qt::Test
)
