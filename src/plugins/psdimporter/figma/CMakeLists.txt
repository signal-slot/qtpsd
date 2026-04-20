# Copyright (C) 2025 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_package(ZLIB REQUIRED)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBZSTD IMPORTED_TARGET libzstd)
endif()

if(NOT TARGET PkgConfig::LIBZSTD)
    find_library(LIBZSTD_LIBRARY NAMES zstd)
    find_path(LIBZSTD_INCLUDE_DIR NAMES zstd.h)
    if(LIBZSTD_LIBRARY AND LIBZSTD_INCLUDE_DIR)
        add_library(_fig_zstd UNKNOWN IMPORTED)
        set_target_properties(_fig_zstd PROPERTIES
            IMPORTED_LOCATION "${LIBZSTD_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBZSTD_INCLUDE_DIR}"
        )
        add_library(PkgConfig::LIBZSTD ALIAS _fig_zstd)
    endif()
endif()

if(NOT TARGET PkgConfig::LIBZSTD)
    message(WARNING "libzstd not found; QPsdImporterFigmaPlugin will skip .fig local-file support")
endif()

qt_internal_add_plugin(QPsdImporterFigmaPlugin
    OUTPUT_NAME qfigmaimporter
    PLUGIN_TYPE psdimporter
    SOURCES
        figma.cpp
        fig_archive.cpp fig_archive.h
        fig_kiwi.cpp fig_kiwi.h
        fig_to_rest.cpp fig_to_rest.h
    LIBRARIES
        Qt::Core
        Qt::CorePrivate
        Qt::Gui
        Qt::Widgets
        Qt::Network
        Qt::PsdCore
        Qt::PsdGui
        Qt::PsdWidget
        Qt::PsdImporter
        ZLIB::ZLIB
)

if(TARGET PkgConfig::LIBZSTD)
    target_link_libraries(QPsdImporterFigmaPlugin PRIVATE PkgConfig::LIBZSTD)
    target_compile_definitions(QPsdImporterFigmaPlugin PRIVATE QPSD_FIGMA_HAS_ZSTD=1)
endif()

qt_internal_add_resource(QPsdImporterFigmaPlugin "figma_icon"
    PREFIX "/figma"
    FILES
        figma.png
)
