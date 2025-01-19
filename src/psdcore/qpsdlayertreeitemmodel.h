// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDLAYERTREEITEMMODEL_H
#define QPSDLAYERTREEITEMMODEL_H

#include "qpsdparser.h"

#include <QAbstractItemModel>
#include <QFileInfo>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdAbstractLayerTreeItemModel {
public:
    enum FolderType {
        NotFolder = 0,
        OpenFolder,
        ClosedFolder,
    };

    virtual void fromParser(const QPsdParser &parser) = 0;
    virtual QSize size() const = 0;
    virtual int layerId(const QModelIndex &index) const = 0;
    virtual QString layerName(const QModelIndex &index) const = 0;
    virtual const QPsdLayerRecord *layerRecord(const QModelIndex &index) const = 0;
    virtual enum FolderType folderType(const QModelIndex &index) const = 0;
    virtual QList<QPersistentModelIndex> groupIndexes(const QModelIndex &index) const = 0;
    virtual QPersistentModelIndex clippingMaskIndex(const QModelIndex &index) const = 0;
};

class Q_PSDCORE_EXPORT QPsdLayerTreeItemModel : public QAbstractItemModel, public QPsdAbstractLayerTreeItemModel
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

    explicit QPsdLayerTreeItemModel(QObject *parent = nullptr);
    virtual ~QPsdLayerTreeItemModel();

    QHash<int, QByteArray> roleNames() const override;

    // Basic functionality:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void fromParser(const QPsdParser &parser) override;
    QSize size() const override;

    qint32 layerId(const QModelIndex &index) const override;
    QString layerName(const QModelIndex &index) const override;
    const QPsdLayerRecord *layerRecord(const QModelIndex &index) const override;
    enum FolderType folderType(const QModelIndex &index) const override;
    QList<QPersistentModelIndex> groupIndexes(const QModelIndex &index) const override;
    QPersistentModelIndex clippingMaskIndex(const QModelIndex &index) const override;

signals:
    void parserReady(const QPsdParser &parser);

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDLAYERTREEITEMMODEL_H
