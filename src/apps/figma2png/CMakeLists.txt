# Copyright (C) 2025 Signal Slot Inc.
# SPDX-License-Identifier: BSD-3-Clause

qt_internal_add_app(figma2png
    SOURCES
        main.cpp
    LIBRARIES
        Qt::Widgets
        Qt::PsdWidget
        Qt::PsdExporter
        Qt::PsdImporter
)

target_link_libraries(figma2png PRIVATE
    Qt::Widgets
    Qt::PsdWidget
    Qt::PsdExporter
    Qt::PsdImporter
)
