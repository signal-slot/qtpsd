# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: BSD-3-Clause

qt_internal_add_app(psdexporter
    SOURCES
        main.cpp
        mainwindow.h mainwindow.cpp mainwindow.ui
        psdwidget.h psdwidget.cpp psdwidget.ui
        psdview.h psdview.cpp
        psdabstractitem.h psdabstractitem.cpp
        psdtextitem.h psdtextitem.cpp
        psdshapeitem.h psdshapeitem.cpp
        psdimageitem.h psdimageitem.cpp
        psdfolderitem.h psdfolderitem.cpp
        exportdialog.h exportdialog.cpp exportdialog.ui
        psdtreeitemmodel.h psdtreeitemmodel.cpp
    LIBRARIES
        Qt::Widgets
        Qt::PsdGui
    ENABLE_AUTOGEN_TOOLS
        uic
)

target_link_libraries(psdexporter PRIVATE
    Qt::Widgets
    Qt::PsdCore
    Qt::PsdGui
)
