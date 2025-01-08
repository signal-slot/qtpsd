// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdabstractplugin.h"

#include <dlfcn.h>

QDir QPsdAbstractPlugin::qpsdPluginDir(const QString &type)
{
    Dl_info info;
    if (dladdr((void*)&qpsdPluginDir, &info)) {
        const auto path = info.dli_fname;
        QFileInfo fi(QString::fromUtf8(path));
        QDir pluginsDir = fi.absoluteDir();
        pluginsDir.cdUp();
        pluginsDir.cd("Plugins"_L1);
        pluginsDir.cd(type);
        return pluginsDir;
    } else {
        qWarning() << "dladdrに失敗しました。";
    }
    return QDir();
}
