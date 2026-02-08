// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDADJUSTMENTLAYERITEM_H
#define QPSDADJUSTMENTLAYERITEM_H

#include <QtPsdGui/qpsdabstractlayeritem.h>

QT_BEGIN_NAMESPACE

class Q_PSDGUI_EXPORT QPsdAdjustmentLayerItem : public QPsdAbstractLayerItem
{
public:
    QPsdAdjustmentLayerItem(const QPsdLayerRecord &record, const QByteArray &adjustmentKey);
    ~QPsdAdjustmentLayerItem() override;
    Type type() const override { return Adjustment; }

    QByteArray adjustmentKey() const;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDADJUSTMENTLAYERITEM_H
