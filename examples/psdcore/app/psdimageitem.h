// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PSDIMAGEITEM_H
#define PSDIMAGEITEM_H

#include "psdabstractitem.h"
#include <QtPsdGui/QPsdImageLayerItem>

class PsdImageItem : public PsdAbstractItem
{
    Q_OBJECT
public:
    PsdImageItem(const QPsdImageLayerItem *psdData, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // PSDIMAGEITEM_H
