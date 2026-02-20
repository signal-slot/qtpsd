// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDIMPORTERPLUGIN_H
#define QPSDIMPORTERPLUGIN_H

#include <QtPsdImporter/qpsdimporterglobal.h>
#include <QtPsdExporter/qpsdexportertreeitemmodel.h>

#include <QtPsdCore/qpsdabstractplugin.h>

#include <QtCore/QVariantMap>
#include <QtGui/QIcon>

QT_BEGIN_NAMESPACE

QT_FORWARD_DECLARE_CLASS(QWidget)

#define QPsdImporterFactoryInterface_iid "org.qt-project.Qt.QPsdImporterFactoryInterface"

class Q_PSDIMPORTER_EXPORT QPsdImporterPlugin : public QPsdAbstractPlugin
{
    Q_OBJECT
public:
    explicit QPsdImporterPlugin(QObject *parent = nullptr);
    virtual ~QPsdImporterPlugin();

    virtual int priority() const = 0;
    virtual QIcon icon() const { return QIcon(); }
    virtual QString name() const = 0;

    // Show plugin-specific import dialog. Returns options map on acceptance.
    // Returns empty map if the user cancelled.
    virtual QVariantMap execImportDialog(QWidget *parent) const = 0;

    // Perform import with the given options (no GUI).
    virtual bool importFrom(QPsdExporterTreeItemModel *model,
                            const QVariantMap &options) const = 0;

    static QByteArrayList keys() {
        return QPsdAbstractPlugin::keys<QPsdImporterPlugin>(QPsdImporterFactoryInterface_iid, "psdimporter");
    }
    static QPsdImporterPlugin *plugin(const QByteArray &key) {
        return QPsdAbstractPlugin::plugin<QPsdImporterPlugin>(QPsdImporterFactoryInterface_iid, "psdimporter", key);
    }
};

QT_END_NAMESPACE

#endif // QPSDIMPORTERPLUGIN_H
