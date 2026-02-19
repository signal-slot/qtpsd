// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdlayerblendingrangesdata.h"

QT_BEGIN_NAMESPACE

class QPsdLayerBlendingRangesData::Private : public QSharedData
{
public:
    Private();
    Range grayBlendSourceRange;
    Range grayBlendDestinationRange;
    QList<Range> channelSourceRanges;
    QList<Range> channelDestinationRanges;
    QByteArray rawData;
};

QPsdLayerBlendingRangesData::Private::Private()
{}

QPsdLayerBlendingRangesData::QPsdLayerBlendingRangesData()
    : QPsdSection()
    , d(new Private)
{}

QPsdLayerBlendingRangesData::QPsdLayerBlendingRangesData(QIODevice *source)
    : QPsdLayerBlendingRangesData()
{
    // Layer blending ranges data
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_21332

    // Length of layer blending ranges data
    auto length = readU32(source);
    if (length > 0)
        d->rawData = source->peek(length);
    EnsureSeek es(source, length);

    // Composite gray blend source. Contains 2 black values followed by 2 white values. Present but irrelevant for Lab & Grayscale.
    d->grayBlendSourceRange = qMakePair(readU8(source), readU8(source));
    d->grayBlendDestinationRange = qMakePair(readU8(source), readU8(source));

    while (es.bytesAvailable() > 8) {
        d->channelSourceRanges.append(qMakePair(readU8(source), readU8(source)));
        d->channelDestinationRanges.append(qMakePair(readU8(source), readU8(source)));
    }
}

QPsdLayerBlendingRangesData::QPsdLayerBlendingRangesData(const QPsdLayerBlendingRangesData &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdLayerBlendingRangesData &QPsdLayerBlendingRangesData::operator=(const QPsdLayerBlendingRangesData &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdLayerBlendingRangesData::~QPsdLayerBlendingRangesData() = default;

QPsdLayerBlendingRangesData::Range QPsdLayerBlendingRangesData::grayBlendSourceRange() const
{
    return d->grayBlendSourceRange;
}

QPsdLayerBlendingRangesData::Range QPsdLayerBlendingRangesData::grayBlendDestinationRange() const
{
    return d->grayBlendDestinationRange;
}

QList<QPsdLayerBlendingRangesData::Range> QPsdLayerBlendingRangesData::channelSourceRanges() const
{
    return d->channelSourceRanges;
}

QList<QPsdLayerBlendingRangesData::Range> QPsdLayerBlendingRangesData::channelDestinationRanges() const
{
    return d->channelDestinationRanges;
}

QByteArray QPsdLayerBlendingRangesData::rawData() const
{
    return d->rawData;
}

QT_END_NAMESPACE
