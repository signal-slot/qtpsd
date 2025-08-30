// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef QPSDIMAGEITEM_H
#define QPSDIMAGEITEM_H

#include <QtPsdWidget/qpsdabstractitem.h>
#include <QtPsdGui/QPsdImageLayerItem>

QT_BEGIN_NAMESPACE

class QPsdImageItem : public QPsdAbstractItem
{
public:
    QPsdImageItem(const QModelIndex &index, const QPsdImageLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QGraphicsItem *parent = nullptr);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
};

QT_END_NAMESPACE

#endif // QPSDIMAGEITEM_H
