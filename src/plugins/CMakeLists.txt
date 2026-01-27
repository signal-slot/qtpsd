# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

add_subdirectory(psdadditionallayerinformation)
add_subdirectory(psddescriptor)
add_subdirectory(psdeffectslayer)

# psdexporter plugins require the PsdExporter module, skip for WASM
if(NOT EMSCRIPTEN)
    add_subdirectory(psdexporter)
endif()
