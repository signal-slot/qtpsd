// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDTEXTLAYERITEM_H
#define QPSDTEXTLAYERITEM_H

#include <QtPsdGui/qpsdabstractlayeritem.h>

#include <QtGui/QColor>
#include <QtGui/QFont>

QT_BEGIN_NAMESPACE

class Q_PSDGUI_EXPORT QPsdTextLayerItem : public QPsdAbstractLayerItem
{
public:
    struct Run {
        QString text;
        QFont font;
        QColor color;
        Qt::Alignment alignment = Qt::AlignVCenter;
    };
    QPsdTextLayerItem(const QPsdLayerRecord &record);
    QPsdTextLayerItem();
    ~QPsdTextLayerItem() override;
    Type type() const override { return Text; }

    QList<Run> runs() const;
    QRectF bounds() const;
    QRectF fontAdjustedBounds() const;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDTEXTLAYERITEM_H
