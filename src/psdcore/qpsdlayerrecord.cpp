// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdlayerrecord.h"
#include "qpsdadditionallayerinformation.h"

QT_BEGIN_NAMESPACE

class QPsdLayerRecord::Private : public QSharedData
{
public:
    Private();
    QRect rect;
    QList<QPsdChannelInfo> channelInfo;
    QPsdBlend::Mode blendMode;
    quint8 opacity;
    Clipping clipping;
    quint8 flags;
    QPsdLayerMaskAdjustmentLayerData layerMaskAdjustmentLayerData;
    QPsdLayerBlendingRangesData layerBlendingRangesData;
    QByteArray name;
    QHash<QByteArray, QVariant> additionalLayerInformation;
    QPsdChannelImageData imageData;
};

QPsdLayerRecord::Private::Private()
    : opacity(0)
    , clipping(Base)
    , flags(0)
{}

QPsdLayerRecord::QPsdLayerRecord()
    : QPsdSection()
    , d(new Private)
{}

QPsdLayerRecord::QPsdLayerRecord(QIODevice *source)
    : QPsdLayerRecord()
{
    // Layer records
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_13084

    // Rectangle containing the contents of the layer. Specified as top, left, bottom, right coordinates
    d->rect = readRectangle(source);

    // Number of channels in the layer
    const auto channels = readU16(source);

    // Channel information.
    for (int i = 0; i < channels; i++) {
        d->channelInfo.append(QPsdChannelInfo(source));
    }

    // Blend mode signature: '8BIM'
    const auto signature = source->read(4);
    if (signature != "8BIM") {
        qWarning() << signature;
        setErrorString("Signature mismatch"_L1);
        return;
    }

    // Blend mode key:
    d->blendMode = QPsdBlend::from(readByteArray(source, 4));

    // Opacity. 0 = transparent ... 255 = opaque
    d->opacity = readU8(source);

    // Clipping: 0 = base, 1 = non-base
    d->clipping = static_cast<Clipping>(readU8(source));

    // Flags:
    // bit 0 = transparency protected;
    // bit 1 = visible;
    // bit 2 = obsolete;
    // bit 3 = 1 for Photoshop 5.0 and later, tells if bit 4 has useful information;
    // bit 4 = pixel data irrelevant to appearance of document
    d->flags = readU8(source);

    // Filler (zero)
    skip(source, 1);

    // Length of the extra data field ( = the total length of the next five fields).
    auto length = readU32(source);
    EnsureSeek es(source, length);

    // Layer mask data: See See Layer mask / adjustment layer data for structure. Can be 40 bytes, 24 bytes, or 4 bytes if no layer mask.
    d->layerMaskAdjustmentLayerData = QPsdLayerMaskAdjustmentLayerData(source);

    // Layer blending ranges: See See Layer blending ranges data.
    d->layerBlendingRangesData = QPsdLayerBlendingRangesData(source);

    // Layer name: Pascal string, padded to a multiple of 4 bytes.
    d->name = readPascalString(source, 4);

    while (es.bytesAvailable() > 12) {
        QPsdAdditionalLayerInformation ali(source);
        d->additionalLayerInformation.insert(ali.key(), ali.data());
    }
}

QPsdLayerRecord::QPsdLayerRecord(const QPsdLayerRecord &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdLayerRecord &QPsdLayerRecord::operator=(const QPsdLayerRecord &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdLayerRecord::~QPsdLayerRecord() = default;

QRect QPsdLayerRecord::rect() const
{
    return d->rect;
}

QList<QPsdChannelInfo> QPsdLayerRecord::channelInfo() const
{
    return d->channelInfo;
}

QPsdBlend::Mode QPsdLayerRecord::blendMode() const
{
    return d->blendMode;
}

quint8 QPsdLayerRecord::opacity() const
{
    return d->opacity;
}

QPsdLayerRecord::Clipping QPsdLayerRecord::clipping() const
{
    return d->clipping;
}

bool QPsdLayerRecord::isTransparencyProtected() const
{
    return d->flags & 0x01;
}

bool QPsdLayerRecord::isVisible() const
{
    return d->flags & 0x02;
}

bool QPsdLayerRecord::hasPixelDataIrrelevantToAppearanceDocument() const
{
    return d->flags & 0x08;
}

bool QPsdLayerRecord::isPixelDataIrrelevantToAppearanceDocument() const
{
    return d->flags & 0x10;
}

QPsdLayerMaskAdjustmentLayerData QPsdLayerRecord::layerMaskAdjustmentLayerData() const
{
    return d->layerMaskAdjustmentLayerData;
}

QPsdLayerBlendingRangesData QPsdLayerRecord::layerBlendingRangesData() const
{
    return d->layerBlendingRangesData;
}

QByteArray QPsdLayerRecord::name() const
{
    return d->name;
}

QHash<QByteArray, QVariant> QPsdLayerRecord::additionalLayerInformation() const
{
    return d->additionalLayerInformation;
}

QPsdChannelImageData QPsdLayerRecord::imageData() const
{
    return d->imageData;
}

void QPsdLayerRecord::setImageData(const QPsdChannelImageData &imageData)
{
    d->imageData = imageData;
}

QT_END_NAMESPACE
