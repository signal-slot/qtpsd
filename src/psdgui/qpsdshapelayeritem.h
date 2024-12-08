// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDSHAPELAYERITEM_H
#define QPSDSHAPELAYERITEM_H

#include <QtPsdGui/qpsdabstractlayeritem.h>

QT_BEGIN_NAMESPACE

class Q_PSDGUI_EXPORT QPsdShapeLayerItem : public QPsdAbstractLayerItem
{
public:
    QPsdShapeLayerItem(const QPsdLayerRecord &record);
    QPsdShapeLayerItem();
    ~QPsdShapeLayerItem() override;
    Type type() const override { return Shape; }

    QPen pen() const;
    QBrush brush() const;
    QPsdAbstractLayerItem::PathInfo pathInfo() const;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDSHAPELAYERITEM_H
