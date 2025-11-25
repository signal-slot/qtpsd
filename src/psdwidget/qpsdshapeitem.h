// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef QPSDSHAPEITEM_H
#define QPSDSHAPEITEM_H

#include <QtPsdWidget/qpsdabstractitem.h>
#include <QtPsdGui/QPsdShapeLayerItem>

QT_BEGIN_NAMESPACE

class QPsdShapeItem : public QPsdAbstractItem
{
public:
    QPsdShapeItem(const QModelIndex &index, const QPsdShapeLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QGraphicsItem *parent = nullptr);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
};

QT_END_NAMESPACE

#endif // QPSDSHAPEITEM_H
