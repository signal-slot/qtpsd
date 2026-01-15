# Copyright (C) 2025 Signal Slot Inc.
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(psd2png LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
    set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/qtpsd/psd2png")

find_package(Qt6 REQUIRED COMPONENTS Widgets PsdWidget)

qt_standard_project_setup()

qt_add_executable(psd2png
    main.cpp
)

set_target_properties(psd2png PROPERTIES
    WIN32_EXECUTABLE FALSE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(psd2png PRIVATE
    Qt::Widgets
    Qt::PsdWidget
)

install(TARGETS psd2png
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)
