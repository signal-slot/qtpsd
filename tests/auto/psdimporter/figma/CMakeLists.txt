# Copyright (C) 2026 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

if(NOT QT_BUILD_STANDALONE_TESTS AND NOT QT_BUILDING_QT)
    cmake_minimum_required(VERSION 3.16)
    project(tst_psdimporter_figma LANGUAGES CXX)
    find_package(Qt6BuildInternals REQUIRED COMPONENTS STANDALONE_TEST)
endif()

qt_internal_add_test(tst_psdimporter_figma
    SOURCES
        tst_psdimporter_figma.cpp
    LIBRARIES
        Qt::Test
        Qt::Gui
        Qt::Widgets
        Qt::Network
        Qt::PsdCore
        Qt::PsdGui
        Qt::PsdWidget
        Qt::PsdExporter
        Qt::PsdImporter
)
