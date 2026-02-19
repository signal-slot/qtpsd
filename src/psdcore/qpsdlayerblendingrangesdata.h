// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDLAYERBLENDINGRANGESDATA_H
#define QPSDLAYERBLENDINGRANGESDATA_H

#include <QtPsdCore/qpsdsection.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdLayerBlendingRangesData : public QPsdSection
{
public:
    using Range = QPair<quint8, quint8>;
    QPsdLayerBlendingRangesData();
    QPsdLayerBlendingRangesData(QIODevice *source);
    QPsdLayerBlendingRangesData(const QPsdLayerBlendingRangesData &other);
    QPsdLayerBlendingRangesData &operator=(const QPsdLayerBlendingRangesData &other);
    ~QPsdLayerBlendingRangesData() override;

    Range grayBlendSourceRange() const;
    Range grayBlendDestinationRange() const;
    QList<Range> channelSourceRanges() const;
    QList<Range> channelDestinationRanges() const;
    QByteArray rawData() const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDLAYERBLENDINGRANGESDATA_H
