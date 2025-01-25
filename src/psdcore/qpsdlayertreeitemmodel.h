// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDLAYERTREEITEMMODEL_H
#define QPSDLAYERTREEITEMMODEL_H

#include "qpsdparser.h"

#include <QAbstractItemModel>
#include <QFileInfo>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdLayerTreeItemModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Roles {
        LayerIdRole = Qt::UserRole + 1,
        NameRole,
        LayerRecordObjectRole,
        FolderTypeRole,
        GroupIndexesRole,
        ClippingMaskIndexRole,
    };
    enum Column {
        LayerId = 0,
        Name,
        FolderType,
    };
    enum FolderType {
        NotFolder = 0,
        OpenFolder,
        ClosedFolder,
    };

    explicit QPsdLayerTreeItemModel(QObject *parent = nullptr);
    virtual ~QPsdLayerTreeItemModel();

    QHash<int, QByteArray> roleNames() const override;

    // Basic functionality:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    virtual void fromParser(const QPsdParser &parser);
    QSize size() const;

    qint32 layerId(const QModelIndex &index) const;
    QString layerName(const QModelIndex &index) const;
    const QPsdLayerRecord *layerRecord(const QModelIndex &index) const;
    enum FolderType folderType(const QModelIndex &index) const;
    QList<QPersistentModelIndex> groupIndexes(const QModelIndex &index) const;
    QPersistentModelIndex clippingMaskIndex(const QModelIndex &index) const;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDLAYERTREEITEMMODEL_H
