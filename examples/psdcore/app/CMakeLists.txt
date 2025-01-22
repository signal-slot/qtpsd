# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(app LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
    set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/qtpsd/app")

find_package(Qt6 REQUIRED COMPONENTS Widgets PsdCore PsdGui)

qt_standard_project_setup()

qt_add_executable(app
    main.cpp
    mainwindow.h mainwindow.cpp mainwindow.ui
    psdwidget.h psdwidget.cpp psdwidget.ui
    psdview.h psdview.cpp
    psdabstractitem.h psdabstractitem.cpp
    psdtextitem.h psdtextitem.cpp
    psdshapeitem.h psdshapeitem.cpp
    psdimageitem.h psdimageitem.cpp
    psdfolderitem.h psdfolderitem.cpp
    itemexportsettingdialog.h itemexportsettingdialog.cpp itemexportsettingdialog.ui
    exportdialog.h exportdialog.cpp exportdialog.ui
    psdtreeitemmodel.h psdtreeitemmodel.cpp
)

set_target_properties(app PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(app PRIVATE
    Qt::Widgets
    Qt::PsdCore
    Qt::PsdGui
)

install(TARGETS app
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)
