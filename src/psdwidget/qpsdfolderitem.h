// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef QPSDFOLDERITEM_H
#define QPSDFOLDERITEM_H

#include <QtPsdWidget/qpsdabstractitem.h>
#include <QtPsdGui/QPsdFolderLayerItem>

QT_BEGIN_NAMESPACE

class QPsdFolderItem : public QPsdAbstractItem
{
public:
    QPsdFolderItem(const QModelIndex &index, const QPsdFolderLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QGraphicsItem *parent = nullptr);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
};

QT_END_NAMESPACE

#endif // QPSDFOLDERITEM_H
