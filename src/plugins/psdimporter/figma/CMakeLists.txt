# Copyright (C) 2025 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

# Local .fig file support depends on zlib (raw DEFLATE) and libzstd. On
# platforms where either can't be located the plugin still builds and the
# REST-API import path keeps working; .fig paths are rejected at runtime.
find_package(ZLIB QUIET)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBZSTD IMPORTED_TARGET QUIET libzstd)
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

set(_fig_local_enabled OFF)
if(TARGET ZLIB::ZLIB AND TARGET PkgConfig::LIBZSTD)
    set(_fig_local_enabled ON)
else()
    message(STATUS "QPsdImporterFigmaPlugin: local .fig support disabled "
        "(zlib found: ${ZLIB_FOUND}, libzstd found: $<IF:$<BOOL:${LIBZSTD_FOUND}>,yes,no>)")
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
)

if(_fig_local_enabled)
    target_link_libraries(QPsdImporterFigmaPlugin PRIVATE ZLIB::ZLIB PkgConfig::LIBZSTD)
    target_compile_definitions(QPsdImporterFigmaPlugin PRIVATE QPSD_FIGMA_LOCAL_FIG=1)
endif()

qt_internal_add_resource(QPsdImporterFigmaPlugin "figma_icon"
    PREFIX "/figma"
    FILES
        figma.png
)
