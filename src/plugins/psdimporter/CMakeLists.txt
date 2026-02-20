# Copyright (C) 2025 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

if(NOT EMSCRIPTEN AND NOT QT_PSD_SKIP_IMPORTER_PLUGINS)
    add_subdirectory(figma)
endif()
