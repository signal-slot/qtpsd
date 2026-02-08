// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdadjustmentlayeritem.h"

QT_BEGIN_NAMESPACE

class QPsdAdjustmentLayerItem::Private
{
public:
    QByteArray adjustmentKey;
};

QPsdAdjustmentLayerItem::QPsdAdjustmentLayerItem(const QPsdLayerRecord &record, const QByteArray &adjustmentKey)
    : QPsdAbstractLayerItem(record)
    , d(new Private)
{
    d->adjustmentKey = adjustmentKey;
}

QPsdAdjustmentLayerItem::~QPsdAdjustmentLayerItem() = default;

QByteArray QPsdAdjustmentLayerItem::adjustmentKey() const
{
    return d->adjustmentKey;
}

QT_END_NAMESPACE
