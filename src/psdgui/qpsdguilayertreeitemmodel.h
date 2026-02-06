// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDGUILAYERTREEITEMMODEL_H
#define QPSDGUILAYERTREEITEMMODEL_H

#include <QtPsdGui/qpsdabstractlayeritem.h>

#include <QtPsdCore/QPsdLayerTreeItemModel>
#include <QtGui/QImage>

QT_BEGIN_NAMESPACE

class Q_PSDGUI_EXPORT QPsdGuiLayerTreeItemModel : public QPsdLayerTreeItemModel
{
    Q_OBJECT
public:
    enum Roles {
        LayerItemObjectRole = ClippingMaskIndexRole + 1
    };

    explicit QPsdGuiLayerTreeItemModel(QObject *parent = nullptr);
    ~QPsdGuiLayerTreeItemModel();

    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void fromParser(const QPsdParser &parser) override;

    const QPsdAbstractLayerItem *layerItem(const QModelIndex &index) const;

    QImage mergedImage() const;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDGUILAYERTREEITEMMODEL_H
