// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdabstractplugin.h"

#include <dlfcn.h>
#include <QtCore/QStringView>

QDir QPsdAbstractPlugin::qpsdPluginDir(const QString &type)
{
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
                if (ret.dirName() == QLatin1StringView("plugins"))
                    return ret;
            }
            return QDir();
        };
        pluginsDir.cdUp();
        pluginsDir = findPluginsDir(pluginsDir);
        pluginsDir.cd(type);
        return pluginsDir;
    } else {
        qWarning() << u"dladdrに失敗しました。"_s;
    }
    return QDir();
}
