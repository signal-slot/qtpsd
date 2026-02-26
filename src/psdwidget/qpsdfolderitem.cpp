// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdfolderitem.h"
#include <QtGui/QPainter>

QT_BEGIN_NAMESPACE

QPsdFolderItem::QPsdFolderItem(const QModelIndex &index, const QPsdFolderLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QGraphicsItem *parent)
    : QPsdAbstractItem(index, psdData, maskItem, group, parent)
{}

void QPsdFolderItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const auto layer = this->layer<QPsdFolderLayerItem>();
    if (layer->artboardRect().isEmpty())
        return;
    if (layer->artboardBackground() == Qt::transparent)
        return;

    setMask(painter);

    // Use local coordinates (0,0) since artboardRect() is in absolute canvas coordinates
    // but the painter is in item-local coordinates
    const QRectF localRect(QPointF(0, 0), QSizeF(layer->artboardRect().size()));

    auto f = painter->font();
    f.setPointSize(24);
    painter->setFont(f);
    painter->setPen(Qt::white);
    painter->drawText(localRect.adjusted(0, -10, 0, -10).topLeft(), name());

    painter->setPen(Qt::NoPen);
    painter->setBrush(layer->artboardBackground());
    painter->drawRect(localRect);
}

QT_END_NAMESPACE
