# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

cmake_minimum_required(VERSION 3.16)

include(.cmake.conf)
project(QtPsd
    VERSION "${QT_REPO_MODULE_VERSION}"
    DESCRIPTION "Qt Psd Libraries"
    HOMEPAGE_URL "https://signal-slot.io/"
    LANGUAGES CXX C
)

set(QT_NO_INTERNAL_COMPATIBILITY_FUNCTIONS TRUE)

find_package(Qt6 ${PROJECT_VERSION} CONFIG REQUIRED COMPONENTS BuildInternals Core Gui)
find_package(Qt6 ${PROJECT_VERSION} CONFIG OPTIONAL_COMPONENTS Widgets)
qt_internal_project_setup()

if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    if(MSVC)
        set(CMAKE_CONFIGURATION_TYPES "Debug;Release;MinSizeRel;RelWithDebInfo" CACHE STRING "" FORCE)
    endif()
endif()

qt_build_repo()
