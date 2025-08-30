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
    const auto layer = this->layer<QPsdFolderLayerItem>();
    if (layer->artboardRect().isEmpty())
        return;
    if (layer->artboardBackground() == Qt::transparent)
        return;

    auto f = painter->font();
    f.setPointSize(24);
    painter->setFont(f);
    painter->setPen(Qt::white);
    painter->drawText(layer->artboardRect().adjusted(0, -10, 0, -10).topLeft(), name());

    painter->setBrush(layer->artboardBackground());
    painter->drawRect(layer->artboardRect());
}

QT_END_NAMESPACE
