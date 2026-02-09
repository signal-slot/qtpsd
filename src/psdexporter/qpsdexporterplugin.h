// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDEXPORTERPLUGIN_H
#define QPSDEXPORTERPLUGIN_H

#include <QtPsdExporter/qpsdexporterglobal.h>
#include <QtPsdExporter/qpsdexportertreeitemmodel.h>

#include <QtPsdCore/qpsdabstractplugin.h>
#include <QtPsdGui/qpsdfolderlayeritem.h>
#include <QtPsdGui/qpsdtextlayeritem.h>
#include <QtPsdGui/qpsdshapelayeritem.h>
#include <QtPsdGui/qpsdimagelayeritem.h>

#include <QtCore/QMimeDatabase>
#include <QtGui/QIcon>

QT_BEGIN_NAMESPACE

#define QPsdExporterFactoryInterface_iid "org.qt-project.Qt.QPsdExporterFactoryInterface"

class Q_PSDEXPORTER_EXPORT QPsdExporterPlugin : public QPsdAbstractPlugin
{
    Q_OBJECT
public:
    enum ExportType {
        File,
        Directory,
    };
    explicit QPsdExporterPlugin(QObject *parent = nullptr);
    virtual ~QPsdExporterPlugin();

    virtual int priority() const = 0;
    virtual QIcon icon() const { return QIcon(); }
    virtual QString name() const = 0;
    virtual ExportType exportType() const = 0;
    virtual QHash<QString, QString> filters() const { return {}; }

    virtual bool exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const = 0;

    const QPsdExporterTreeItemModel *model() const;
    void setModel(const QPsdExporterTreeItemModel *model) const;

    static QByteArrayList keys() {
        return QPsdAbstractPlugin::keys<QPsdExporterPlugin>(QPsdExporterFactoryInterface_iid, "psdexporter");
    }
    static QPsdExporterPlugin *plugin(const QByteArray &key) {
        return QPsdAbstractPlugin::plugin<QPsdExporterPlugin>(QPsdExporterFactoryInterface_iid, "psdexporter", key);
    }

    static QString toUpperCamelCase(const QString &str, const QString &separator = QString());
protected:
    static QString toLowerCamelCase(const QString &str);
    static QString toSnakeCase(const QString &str);
    static QString toKebabCase(const QString &str);

    static QString imageFileName(const QString &name, const QString &format, const QByteArray &uniqueId = {});
    bool generateMaps() const;

protected:
    static QMimeDatabase mimeDatabase;

    mutable QHash<const QPersistentModelIndex, QRect> childrenRectMap;
    mutable QHash<const QPersistentModelIndex, QRect> indexRectMap;
    mutable QMultiMap<const QPersistentModelIndex, QPersistentModelIndex> indexMergeMap;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDEXPORTERPLUGIN_H
