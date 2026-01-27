# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

add_subdirectory(psdcore)
add_subdirectory(psdgui)
add_subdirectory(psdexporter)
if(TARGET Qt6::Widgets)
    add_subdirectory(psdwidget)
endif()
add_subdirectory(plugins)
add_subdirectory(apps)
