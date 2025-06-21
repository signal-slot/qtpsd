// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdchannelimagedata.h"
#include "qpsdlayerrecord.h"

QT_BEGIN_NAMESPACE

class QPsdChannelImageData::Private : public QSharedData
{
public:
    Private();
    QHash<QPsdChannelInfo::ChannelID, QByteArray> imageData;

    const unsigned char *data(QPsdChannelInfo::ChannelID channelID) const {
        if (!imageData.contains(channelID))
            return nullptr;
        return reinterpret_cast<const unsigned char *>(imageData.value(channelID).constData());
    }

};

QPsdChannelImageData::Private::Private()
{}

QPsdChannelImageData::QPsdChannelImageData()
    : QPsdAbstractImage()
    , d(new Private)
{}

QPsdChannelImageData::QPsdChannelImageData(const QPsdLayerRecord &record, QIODevice *source)
    : QPsdChannelImageData()
{
    setWidth(record.rect().width());
    setHeight(record.rect().height());
    setOpacity(record.opacity());

    // Channel image data
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_26431

    for (const auto &channelInfo : record.channelInfo()) {
        auto id = channelInfo.id();
        auto length = channelInfo.length();
        EnsureSeek es(source, length);
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length == 0);
        });

        if (id < -3 || id > 3) {
            qWarning() << record.name() << record.blendMode() << id << length << "not supported";
        }
        // Compression. 0 = Raw Data, 1 = RLE compressed, 2 = ZIP without prediction, 3 = ZIP with prediction.
        Compression compression = static_cast<Compression>(readU16(source, &length));

        if (es.bytesAvailable() <= 0)
            continue;

        // Image data.
        switch (compression) {
        case RawData:
            // If the compression code is 0, the image data is just the raw image data,
            // whose size is calculated as (LayerBottom-LayerTop)* (LayerRight-LayerLeft)
            // (from the first field in See Layer records).
            d->imageData.insert(id, readByteArray(source, length, &length));
            break;
        case RLE: {
            // If the compression code is 1,
            // the image data starts with the byte counts for all the scan lines in the channel
            // (LayerBottom-LayerTop) , with each count stored as a two-byte value.
            // (**PSB** each count stored as a four-byte value.)
            // The RLE compressed data follows, with each scan line compressed separately.
            // The RLE compression is the same compression algorithm used by the Macintosh
            // ROM routine PackBits, and the TIFF standard.
            auto height = record.rect().height();
            if (id == QPsdChannelInfo::UserSuppliedLayerMask) {
                height = record.layerMaskAdjustmentLayerData().rect().height();
            } else if (id == QPsdChannelInfo::RealUserSuppliedLayerMask) {
                height = record.layerMaskAdjustmentLayerData().realUserMaskRect().height();
            }

            d->imageData.insert(id, readRLE(source, height, &length));
            break; }
        case ZipWithPrediction:
        case ZipWithoutPrediction:
            d->imageData.insert(id, readZip(source, &length));
            break;
        default:
            qFatal("Compression %d not supported", compression);
        }
        // If the layer's size, and therefore the data, is odd, a pad byte will be inserted at the end of the row.
        // If the layer is an adjustment layer, the channel data is undefined (probably all white.)
    }
}

QPsdChannelImageData::QPsdChannelImageData(const QPsdChannelImageData &other)
    : QPsdAbstractImage(other)
    , d(other.d)
{}

QPsdChannelImageData &QPsdChannelImageData::operator=(const QPsdChannelImageData &other)
{
    if (this != &other) {
        QPsdAbstractImage::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdChannelImageData::~QPsdChannelImageData() = default;

QByteArray QPsdChannelImageData::imageData() const
{
    return d->imageData.value(QPsdChannelInfo::Red);
}

QByteArray QPsdChannelImageData::transparencyMaskData() const
{
    return d->imageData.contains(QPsdChannelInfo::TransparencyMask) ? d->imageData.value(QPsdChannelInfo::TransparencyMask) : QByteArray();
}

QByteArray QPsdChannelImageData::userSuppliedLayerMask() const
{
    return d->imageData.contains(QPsdChannelInfo::UserSuppliedLayerMask) ? d->imageData.value(QPsdChannelInfo::UserSuppliedLayerMask) : QByteArray();
}

const unsigned char *QPsdChannelImageData::gray() const
{
    return r();
}

const unsigned char *QPsdChannelImageData::r() const
{
    return d->data(QPsdChannelInfo::Red);
}

const unsigned char *QPsdChannelImageData::g() const
{
    return d->data(QPsdChannelInfo::Green);
}

const unsigned char *QPsdChannelImageData::b() const
{
    return d->data(QPsdChannelInfo::Blue);
}

const unsigned char *QPsdChannelImageData::a() const
{
    // Check for transparency mask first (channel -1), then alpha channel (channel 3)
    const unsigned char *alpha = d->data(QPsdChannelInfo::TransparencyMask);
    if (!alpha) {
        alpha = d->data(QPsdChannelInfo::Alpha);
    }
    return alpha;
}

QT_END_NAMESPACE
