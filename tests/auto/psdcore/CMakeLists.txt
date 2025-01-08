# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

add_subdirectory(qpsdenginedataparser)
add_subdirectory(qpsdlayertreeitemmodel)

# ag-psd is a JavaScript/TypeScript library, handle it as test data only
add_library(ag-psd-files INTERFACE)
target_sources(ag-psd-files INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/ag-psd/test/files/blend.psd
    ${CMAKE_CURRENT_SOURCE_DIR}/ag-psd/test/files/blend-2.psd
    ${CMAKE_CURRENT_SOURCE_DIR}/ag-psd/test/files/groups.psd
    ${CMAKE_CURRENT_SOURCE_DIR}/ag-psd/test/files/masks.psd
    ${CMAKE_CURRENT_SOURCE_DIR}/ag-psd/test/files/paths.psd
    ${CMAKE_CURRENT_SOURCE_DIR}/ag-psd/test/files/simple.psd
)
