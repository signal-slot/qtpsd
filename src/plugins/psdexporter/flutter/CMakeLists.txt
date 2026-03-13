# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

qt_internal_add_plugin(QPsdExporterFlutterPlugin
    OUTPUT_NAME qflutter
    PLUGIN_TYPE psdexporter
    SOURCES
        flutter.cpp
    LIBRARIES
        Qt::Core
        Qt::PsdExporter
)

qt_internal_add_resource(QPsdExporterFlutterPlugin "flutter"
    PREFIX "/flutter"
    FILES
        flutter.png
        qtpsd_path.dart
)
