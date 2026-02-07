// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdlayerinfo.h"
#include "qpsdfileheader.h"

QT_BEGIN_NAMESPACE

class QPsdLayerInfo::Private : public QSharedData
{
public:
    Private();
    void parse(QIODevice *source, quint32 length);

    QList<QPsdLayerRecord> records;
    QList<QPsdChannelImageData> channelImageData;
};

QPsdLayerInfo::Private::Private()
{}

void QPsdLayerInfo::Private::parse(QIODevice *source, quint32 length)
{
    EnsureSeek es(source, length);

    const auto count = readS16(source);

    for (int i = 0; i < std::abs(count); i++) {
        records.append(QPsdLayerRecord(source));
    }

    for (const QPsdLayerRecord &record : records) {
        QPsdChannelImageData imageData(record, source);
        channelImageData.append(imageData);
    }
}

QPsdLayerInfo::QPsdLayerInfo()
    : QPsdSection()
    , d(new Private)
{}

QPsdLayerInfo::QPsdLayerInfo(QIODevice *source)
    : QPsdLayerInfo()
{
    // Layer info
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_16000

    // Length of the layers info section, rounded up to a multiple of 2. (**PSB** length is 8 bytes.)
    auto length = readU32(source);
    d->parse(source, length);
}

QPsdLayerInfo::QPsdLayerInfo(QIODevice *source, quint32 length)
    : QPsdLayerInfo()
{
    d->parse(source, length);
}

QPsdLayerInfo::QPsdLayerInfo(const QPsdLayerInfo &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdLayerInfo &QPsdLayerInfo::operator=(const QPsdLayerInfo &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdLayerInfo::~QPsdLayerInfo() = default;

QList<QPsdLayerRecord> QPsdLayerInfo::records() const
{
    return d->records;
}

QList<QPsdChannelImageData> QPsdLayerInfo::channelImageData() const
{
    return d->channelImageData;
}

void QPsdLayerInfo::setFileHeader(const QPsdFileHeader &fileHeader)
{
    // Set the file header on all channel image data entries
    for (int i = 0; i < d->channelImageData.size(); ++i) {
        d->channelImageData[i].setHeader(fileHeader);
    }

    // Also update records with their corresponding image data
    for (int i = 0; i < d->records.size() && i < d->channelImageData.size(); ++i) {
        d->records[i].setImageData(d->channelImageData[i]);
    }
}

QT_END_NAMESPACE
