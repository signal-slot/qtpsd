# Copyright (C) 2025 Signal Slot Inc.
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(psdcreator LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
    set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/qtpsd/psdcreator")

find_package(Qt6 REQUIRED COMPONENTS Widgets PsdCore PsdWriter PsdGui)

qt_standard_project_setup()

qt_add_executable(psdcreator
    main.cpp
    mainwindow.h mainwindow.cpp
    canvas.h canvas.cpp
    layermodel.h layermodel.cpp
)

set_target_properties(psdcreator PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(psdcreator PRIVATE
    Qt::Widgets
    Qt::PsdCore
    Qt::PsdWriter
    Qt::PsdGui
)

install(TARGETS psdcreator
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)
