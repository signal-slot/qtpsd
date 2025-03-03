// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDABSTRACTPLUGIN_H
#define QPSDABSTRACTPLUGIN_H

#include <QtPsdCore/qpsdcoreglobal.h>
#include <QtPsdCore/qpsdsection.h>

#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QLoggingCategory>
#include <QtCore/QPluginLoader>
#include <QtCore/QString>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdAbstractPlugin : public QObject, protected QPsdSection
{
    Q_OBJECT
    Q_PROPERTY(QByteArray key READ key WRITE setKey NOTIFY keyChanged)
public:
    explicit QPsdAbstractPlugin(QObject *parent = nullptr);
    ~QPsdAbstractPlugin() override;

    QByteArray key() const;

public Q_SLOTS:
    void setKey(const QByteArray &key);

Q_SIGNALS:
    void keyChanged(const QByteArray &key);

protected:
    template <typename T>
    static QByteArrayList keys(const char *iid, const char *subdir) {
        static QByteArrayList ret = [&]() {
            QByteArrayList keys;
            QDir pluginsDir = qpsdPluginDir(QLatin1StringView(subdir));
            for (const QString &fileName : pluginsDir.entryList(
#ifdef Q_OS_LINUX
                     QStringList() << "*.so"_L1,
#endif
#ifdef Q_OS_WIN
                     QStringList() << "*.dll"_L1,
#endif
                     QDir::Files)) {
                QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));
                const auto json = loader.metaData();
                Q_ASSERT(json.value("IID"_L1).toString() == QLatin1StringView(iid));
                const auto metaData = json.value("MetaData"_L1).toObject();
                const auto jsonKeys = metaData.value("Keys"_L1).toArray();
                for (const QJsonValue &jsonKey : jsonKeys) {
                    keys.append(jsonKey.toString().toUtf8());
                }
            }
            return keys;
        }();
        return ret;
    }

    template <typename T>
    static T *plugin(const char *iid, const char *subdir, const QByteArray &key) {
        static QHash<QByteArray, T *> plugins = [&]() {
            QHash<QByteArray, T *> ret;
            QDir pluginsDir = qpsdPluginDir(QLatin1StringView(subdir));
            for (const QString &fileName : pluginsDir.entryList(
#ifdef Q_OS_LINUX
                     QStringList() << "*.so"_L1,
#endif
#ifdef Q_OS_WIN
                     QStringList() << "*.dll"_L1,
#endif
                     QDir::Files)) {
                QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));
                const auto json = loader.metaData();
                Q_ASSERT(json.value("IID"_L1).toString() == QLatin1StringView(iid));
                const auto metaData = json.value("MetaData"_L1).toObject();
                const auto jsonKeys = metaData.value("Keys"_L1).toArray();
                QObject *object = loader.instance();
                if (!object) {
                    qFatal("%s", qPrintable(loader.errorString()));
                }
                auto plugin = qobject_cast<T *>(object);
                Q_ASSERT(plugin);
                for (const QJsonValue &jsonKey : jsonKeys) {
                    ret.insert(jsonKey.toString().toLatin1(), plugin);
                }
            }
            return ret;
        }();

        auto ret = plugins.value(key);
        if (ret)
            ret->setKey(key);
        return qobject_cast<T *>(ret);
    }

private:
    static QDir qpsdPluginDir(const QString &type);
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDABSTRACTPLUGIN_H
