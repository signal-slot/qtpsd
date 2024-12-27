// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "psdfolderitem.h"
#include <QtGui/QPainter>

PsdFolderItem::PsdFolderItem(const QPsdFolderLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, QWidget *parent)
    : PsdAbstractItem(psdData, maskItem, parent)
{}

void PsdFolderItem::paintEvent(QPaintEvent *event)
{
    PsdAbstractItem::paintEvent(event);

    const auto layer = this->layer<QPsdFolderLayerItem>();
    if (layer->artboardRect().isEmpty())
        return;
    if (layer->artboardBackground() == Qt::transparent)
        return;

    QPainter painter(this);
    auto f = font();
    f.setPointSize(24);
    painter.setFont(f);
    painter.setPen(Qt::white);
    painter.drawText(layer->artboardRect().adjusted(0, -10, 0, -10).topLeft(), name());

    painter.setBrush(layer->artboardBackground());
    painter.drawRect(layer->artboardRect());
}
