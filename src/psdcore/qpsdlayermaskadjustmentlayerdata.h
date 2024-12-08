// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDLAYERMASKADJUSTMENTLAYERDATA_H
#define QPSDLAYERMASKADJUSTMENTLAYERDATA_H

#include <QtPsdCore/qpsdsection.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdLayerMaskAdjustmentLayerData : public QPsdSection
{
public:
    QPsdLayerMaskAdjustmentLayerData();
    QPsdLayerMaskAdjustmentLayerData(QIODevice *source);
    QPsdLayerMaskAdjustmentLayerData(const QPsdLayerMaskAdjustmentLayerData &other);
    QPsdLayerMaskAdjustmentLayerData &operator=(const QPsdLayerMaskAdjustmentLayerData &other);
    ~QPsdLayerMaskAdjustmentLayerData() override;

    bool isEmpty() const;
    QRect rect() const;
    quint8 defaultColor() const;
    bool isPositionRelativeToLayer() const;
    bool isLayerMaskDisabled() const;
    // Obsolete
    // inline bool isInvertLayerMaskWhenBlending() const;
    bool isLayerMaskFromRenderingOtherData() const;
    bool isLayerMaskFromVectorData() const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDLAYERMASKADJUSTMENTLAYERDATA_H
