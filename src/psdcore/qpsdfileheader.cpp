// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdfileheader.h"

QT_BEGIN_NAMESPACE

class QPsdFileHeader::Private : public QSharedData
{
public:
    Private();
    quint16 channels;
    quint32 height;
    quint32 width;
    quint16 depth;
    ColorMode colorMode;
};

QPsdFileHeader::Private::Private()
    : channels(0)
    , height(0)
    , width(0)
    , depth(0)
    , colorMode(Bitmap)
{}

QPsdFileHeader::QPsdFileHeader()
    : QPsdSection()
    , d(new Private)
{}

QPsdFileHeader::QPsdFileHeader(QIODevice *source)
    : QPsdFileHeader()
{
    // File Header Section
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_19840

    // Signature: always equal to '8BPS' . Do not try to read the file if the signature does not match this value.
    const auto signature = source->read(4);
    if (signature != "8BPS") {
        qWarning() << signature;
        setErrorString("Signature mismatch"_L1);
        return;
    }

    // Version: always equal to 1. Do not try to read the file if the version does not match this value. (**PSB** version is 2.)
    const auto version = readU16(source);
    if (version != 1) {
        qWarning() << version;
        source->close();
        setErrorString("Version error"_L1);
        return;
    }

    // Reserved: must be zero.
    skip(source, 6);

    // The number of channels in the image, including any alpha channels. Supported range is 1 to 56.
    d->channels = readU16(source);

    // The height of the image in pixels. Supported range is 1 to 30,000.
    d->height = readU32(source);

    // The width of the image in pixels. Supported range is 1 to 30,000
    d->width = readU32(source);

    // Depth: the number of bits per channel. Supported values are 1, 8, 16 and 32.
    d->depth = readU16(source);

    // The color mode of the file. Supported values are: Bitmap = 0; Grayscale = 1; Indexed = 2; RGB = 3; CMYK = 4; Multichannel = 7; Duotone = 8; Lab = 9.
    d->colorMode = static_cast<ColorMode>(readU16(source));
}

QPsdFileHeader::QPsdFileHeader(const QPsdFileHeader &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdFileHeader &QPsdFileHeader::operator=(const QPsdFileHeader &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdFileHeader::~QPsdFileHeader() = default;

quint16 QPsdFileHeader::channels() const
{
    return d->channels;
}

quint32 QPsdFileHeader::height() const
{
    return d->height;
}

quint32 QPsdFileHeader::width() const
{
    return d->width;
}

quint16 QPsdFileHeader::depth() const
{
    return d->depth;
}

QPsdFileHeader::ColorMode QPsdFileHeader::colorMode() const
{
    return d->colorMode;
}

QT_END_NAMESPACE
