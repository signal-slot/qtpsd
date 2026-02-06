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
        QDir pluginsDir = fi.absoluteDir();

        std::function<QDir(QDir)> findPluginsDir;
        findPluginsDir = [&](QDir dir) -> QDir{
            if (dir.exists("plugins"_L1)) {
                dir.cd("plugins"_L1);
                return dir;
            }

            const auto subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const auto &subDir : subDirs) {
                QDir newDir = dir.absoluteFilePath(subDir);
                auto ret = findPluginsDir(newDir);
                if (ret.dirName() == "plugins"_L1)
                    return ret;
            }
            return QDir();
        };
        pluginsDir.cdUp();
        pluginsDir = findPluginsDir(pluginsDir);
        if (pluginsDir.exists(type)) {
            pluginsDir.cd(type);
            return pluginsDir;
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
