#include "qpsdabstractplugin.h"
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QCoreApplication>
#include <dlfcn.h>

QT_BEGIN_NAMESPACE

QDir QPsdAbstractPlugin::qpsdPluginDir(const QString &type)
{
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(&qpsdPluginDir), &info)) {
        QDir dir = QFileInfo(QString::fromUtf8(info.dli_fname)).dir();
        dir.cd(QLatin1StringView("../PlugIns"));
        dir.cd(type);
        return dir;
    }
    return QDir();
}

QT_END_NAMESPACE
