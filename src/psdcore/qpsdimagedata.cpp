// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdimagedata.h"
#include "qpsdfileheader.h"

QT_BEGIN_NAMESPACE

class QPsdImageData::Private : public QSharedData
{
public:
    QByteArray imageData;
};

QPsdImageData::QPsdImageData()
    : QPsdAbstractImage()
    , d(new Private)
{}

QPsdImageData::QPsdImageData(const QPsdFileHeader &header, QIODevice *source)
    : QPsdImageData()
{
    setHeader(header);
    setWidth(header.width());
    setHeight(header.height());

    // Image Data Section
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_89817
    quint32 length = source->bytesAvailable();
    auto cleanup = qScopeGuard([&] {
        Q_ASSERT(length == 0);
    });

    // Compression method:
    // 0 = Raw image data
    // 1 = RLE compressed the image data starts with the byte counts for all the scan lines (rows * channels), with each count stored as a two-byte value. The RLE compressed data follows, with each scan line compressed separately. The RLE compression is the same compression algorithm used by the Macintosh ROM routine PackBits , and the TIFF standard.
    // 2 = ZIP without prediction
    // 3 = ZIP with prediction.
    Compression compression = static_cast<Compression>(readU16(source, &length));
    // The color data.
    switch (compression) {
    case RawData:
        d->imageData = source->readAll();
        length = 0;
        break;
    case RLE:
        d->imageData = readRLE(source, header.height() * header.channels(), &length);
        break;
    case ZipWithPrediction:
    case ZipWithoutPrediction:
        d->imageData = readZip(source, &length);
        break;
    default:
        qFatal("not supported");
    }
}

QPsdImageData::QPsdImageData(const QPsdImageData &other)
    : QPsdAbstractImage(other)
    , d(other.d)
{}

QPsdImageData &QPsdImageData::operator=(const QPsdImageData &other)
{
    if (this != &other) {
        QPsdAbstractImage::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdImageData::~QPsdImageData() = default;

QByteArray QPsdImageData::imageData() const
{
    return d->imageData;
}

const unsigned char *QPsdImageData::gray() const
{
    return reinterpret_cast<const unsigned char *>(d->imageData.constData());
}

const unsigned char *QPsdImageData::r() const
{
    return g() + width() * height() * depth() / 8;
}

const unsigned char *QPsdImageData::g() const
{
    return b() + width() * height() * depth() / 8;
}

const unsigned char *QPsdImageData::b() const
{
    return gray();
}

const unsigned char *QPsdImageData::c() const
{
    return gray();
}

const unsigned char *QPsdImageData::m() const
{
    return c() + width() * height() * depth() / 8;
}

const unsigned char *QPsdImageData::y() const
{
    return m() + width() * height() * depth() / 8;
}

const unsigned char *QPsdImageData::k() const
{
    return y() + width() * height() * depth() / 8;
}


QT_END_NAMESPACE
