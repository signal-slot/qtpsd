// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef QPSDADJUSTMENTITEM_H
#define QPSDADJUSTMENTITEM_H

#include <QtPsdWidget/qpsdabstractitem.h>
#include <QtPsdGui/QPsdAdjustmentLayerItem>

QT_BEGIN_NAMESPACE

// Renders a Photoshop adjustment layer by reading back the pixels already
// composited below it (backbuffer readback, same technique as
// drawCustomBlended) and applying the adjustment math in place.
class Q_PSDWIDGET_EXPORT QPsdAdjustmentItem : public QPsdAbstractItem
{
public:
    QPsdAdjustmentItem(const QModelIndex &index, const QPsdAdjustmentLayerItem *layer,
                       const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group,
                       const QSize &canvasSize, QGraphicsItem *parent = nullptr);
    ~QPsdAdjustmentItem();

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDADJUSTMENTITEM_H
