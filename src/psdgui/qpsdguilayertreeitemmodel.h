// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDGUILAYERTREEITEMMODEL_H
#define QPSDGUILAYERTREEITEMMODEL_H

#include <QtPsdGui/qpsdabstractlayeritem.h>

#include <QtPsdCore/QPsdLayerTreeItemModel>

#include <QIdentityProxyModel>

QT_BEGIN_NAMESPACE

class Q_PSDGUI_EXPORT QPsdGuiAbstractLayerTreeItemModel : public QPsdAbstractLayerTreeItemModel {
public:
    virtual const QPsdAbstractLayerItem *layerItem(const QModelIndex &index) const = 0;
};

class Q_PSDGUI_EXPORT QPsdGuiLayerTreeItemModel : public QIdentityProxyModel, public QPsdGuiAbstractLayerTreeItemModel
{
    Q_OBJECT
public:
    enum Roles {
        LayerIdRole = QPsdLayerTreeItemModel::Roles::LayerIdRole,
        NameRole = QPsdLayerTreeItemModel::Roles::NameRole,
        LayerRecordObjectRole = QPsdLayerTreeItemModel::Roles::LayerRecordObjectRole,
        FolderTypeRole = QPsdLayerTreeItemModel::Roles::FolderTypeRole,
        GroupIndexesRole = QPsdLayerTreeItemModel::Roles::GroupIndexesRole,
        ClippingMaskIndexRole = QPsdLayerTreeItemModel::Roles::ClippingMaskIndexRole,
        LayerItemObjectRole,
    };
    enum Column {
        LayerId = QPsdLayerTreeItemModel::LayerId,
        Name = QPsdLayerTreeItemModel::Name,
    };

    explicit QPsdGuiLayerTreeItemModel(QObject *parent = nullptr);
    ~QPsdGuiLayerTreeItemModel();

    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void fromParser(const QPsdParser &parser) override;
    QSize size() const override;

    qint32 layerId(const QModelIndex &index) const override;
    QString layerName(const QModelIndex &index) const override;
    const QPsdLayerRecord *layerRecord(const QModelIndex &index) const override;
    enum FolderType folderType(const QModelIndex &index) const override;
    QList<QPersistentModelIndex> groupIndexes(const QModelIndex &index) const override;
    QPersistentModelIndex clippingMaskIndex(const QModelIndex &index) const override;
    const QPsdAbstractLayerItem *layerItem(const QModelIndex &index) const override;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDGUILAYERTREEITEMMODEL_H
