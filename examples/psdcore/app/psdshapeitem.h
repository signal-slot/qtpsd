// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PSDSHAPEITEM_H
#define PSDSHAPEITEM_H

#include "psdabstractitem.h"
#include <QtPsdGui/QPsdShapeLayerItem>

class PsdShapeItem : public PsdAbstractItem
{
    Q_OBJECT
public:
    PsdShapeItem(const QPsdShapeLayerItem *psdData, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // PSDSHAPEITEM_H
