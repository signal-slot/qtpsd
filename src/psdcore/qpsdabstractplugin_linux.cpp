// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdabstractplugin.h"

#include <QtCore/QLibraryInfo>

#include <dlfcn.h>

QDir QPsdAbstractPlugin::qpsdPluginDir(const QString &type)
{
    // First, try relative to the library location (for development builds)
    Dl_info info;
    if (dladdr((void*)&qpsdPluginDir, &info)) {
        const auto path = info.dli_fname;
        QFileInfo fi(QString::fromUtf8(path));
        QDir libDir = fi.absoluteDir();

        // Try Qt-standard plugin path first: lib64/qt6/plugins or lib/qt6/plugins
        QDir qtPluginsDir = libDir;
        if (qtPluginsDir.cd("qt6/plugins"_L1) && qtPluginsDir.exists(type)) {
            qtPluginsDir.cd(type);
            return qtPluginsDir;
        }

        // Try parent's qt6/plugins (e.g., build/lib64 -> build/lib64/qt6/plugins)
        qtPluginsDir = libDir;
        qtPluginsDir.cdUp();
        if (qtPluginsDir.cd("lib64/qt6/plugins"_L1) && qtPluginsDir.exists(type)) {
            qtPluginsDir.cd(type);
            return qtPluginsDir;
        }
        qtPluginsDir = libDir;
        qtPluginsDir.cdUp();
        if (qtPluginsDir.cd("lib/qt6/plugins"_L1) && qtPluginsDir.exists(type)) {
            qtPluginsDir.cd(type);
            return qtPluginsDir;
        }
    } else {
        qWarning() << u"dladdrに失敗しました。"_s;
    }

    // Fallback: try the standard Qt plugin path (for installed builds)
    QDir pluginsDir(QLibraryInfo::path(QLibraryInfo::PluginsPath));
    if (pluginsDir.exists(type)) {
        pluginsDir.cd(type);
        return pluginsDir;
    }

    return QDir();
}
