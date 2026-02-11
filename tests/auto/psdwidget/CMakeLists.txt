# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: BSD-3-Clause

if(NOT QT_BUILD_STANDALONE_TESTS AND NOT QT_BUILDING_QT)
    cmake_minimum_required(VERSION 3.16)
    project(tst_psdwidget LANGUAGES CXX)
    find_package(Qt6BuildInternals REQUIRED COMPONENTS STANDALONE_TEST)
endif()

qt_internal_add_test(tst_qpsdview
    SOURCES
        qpsdview/tst_qpsdview.cpp
    LIBRARIES
        Qt::Test
        Qt::Gui
        Qt::Widgets
        Qt::PsdCore
        Qt::PsdWidget
)
