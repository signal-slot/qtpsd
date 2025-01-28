// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PSDTEXTITEM_H
#define PSDTEXTITEM_H

#include "psdabstractitem.h"
#include <QtPsdGui/QPsdTextLayerItem>

class PsdTextItem : public PsdAbstractItem
{
    Q_OBJECT
public:
    PsdTextItem(const QModelIndex &index, const QPsdTextLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // PSDTEXTITEM_H
