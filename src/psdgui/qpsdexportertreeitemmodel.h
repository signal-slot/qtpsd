// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDEXPORTERTREEITEMMODEL_H
#define QPSDEXPORTERTREEITEMMODEL_H

#include <QtPsdGui/qpsdabstractlayeritem.h>
#include <QtPsdGui/QPsdGuiLayerTreeItemModel>

#include <QtPsdCore/QPsdLayerTreeItemModel>

#include <QIdentityProxyModel>

QT_BEGIN_NAMESPACE

class Q_PSDGUI_EXPORT QPsdExporterAbstractTreeItemModel : public QPsdGuiAbstractLayerTreeItemModel {
public:
    virtual QVariantMap exportHint(const QString& exporterKey) const = 0;
    virtual void setExportHint(const QString& exporterKey, const QVariantMap exportHint) = 0;

    virtual QPsdAbstractLayerItem::ExportHint layerHint(const QModelIndex &index) const = 0;
    virtual void setLayerHint(const QModelIndex &index, const QPsdAbstractLayerItem::ExportHint layerHint) = 0;

    virtual QMap<QString, QPsdAbstractLayerItem::ExportHint> layerHints() const = 0;
    virtual QMap<QString, QVariantMap> exportHints() const = 0;
};

class Q_PSDGUI_EXPORT QPsdExporterTreeItemModel : public QIdentityProxyModel, public QPsdExporterAbstractTreeItemModel
{
    Q_OBJECT
public:
    enum Roles {
        LayerIdRole = QPsdGuiLayerTreeItemModel::Roles::LayerIdRole,
        NameRole = QPsdGuiLayerTreeItemModel::Roles::NameRole,
        LayerRecordObjectRole = QPsdGuiLayerTreeItemModel::Roles::LayerRecordObjectRole,
        FolderTypeRole = QPsdGuiLayerTreeItemModel::Roles::FolderTypeRole,
        GroupIndexesRole = QPsdGuiLayerTreeItemModel::Roles::GroupIndexesRole,
        ClippingMaskIndexRole = QPsdGuiLayerTreeItemModel::Roles::ClippingMaskIndexRole,
        LayerItemObjectRole = QPsdGuiLayerTreeItemModel::Roles::LayerItemObjectRole,
        LayerHintRole,
    };

    explicit QPsdExporterTreeItemModel(QObject *parent = nullptr);
    ~QPsdExporterTreeItemModel();

    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void fromParser(const QPsdParser &parser,
        const QMap<QString, QPsdAbstractLayerItem::ExportHint> layerHints,
        const QMap<QString, QVariantMap> exportHints);
    void fromParser(const QPsdParser &parser) override;
    QSize size() const override;
    QVariantMap exportHint(const QString& exporterKey) const override;
    void setExportHint(const QString& exporterKey, const QVariantMap exportHint);

    qint32 layerId(const QModelIndex &index) const override;
    QString layerName(const QModelIndex &index) const override;
    const QPsdLayerRecord *layerRecord(const QModelIndex &index) const override;
    enum FolderType folderType(const QModelIndex &index) const override;
    QList<QPersistentModelIndex> groupIndexes(const QModelIndex &index) const override;
    QPersistentModelIndex clippingMaskIndex(const QModelIndex &index) const override;
    const QPsdAbstractLayerItem *layerItem(const QModelIndex &index) const override;
    QPsdAbstractLayerItem::ExportHint layerHint(const QModelIndex &index) const override;
    void setLayerHint(const QModelIndex &index, const QPsdAbstractLayerItem::ExportHint layerHint) override;
    QMap<QString, QPsdAbstractLayerItem::ExportHint> layerHints() const override;
    QMap<QString, QVariantMap> exportHints() const override;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDEXPORTERTREEITEMMODEL_H
