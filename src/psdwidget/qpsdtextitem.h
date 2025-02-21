// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef QPSDTEXTITEM_H
#define QPSDTEXTITEM_H

#include <QtPsdWidget/qpsdabstractitem.h>
#include <QtPsdGui/QPsdTextLayerItem>

QT_BEGIN_NAMESPACE

class QPsdTextItem : public QPsdAbstractItem
{
    Q_OBJECT
public:
    QPsdTextItem(const QModelIndex &index, const QPsdTextLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

QT_END_NAMESPACE

#endif // QPSDTEXTITEM_H
