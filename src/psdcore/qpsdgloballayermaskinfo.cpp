// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdgloballayermaskinfo.h"
#include "qpsdcolorspace.h"

QT_BEGIN_NAMESPACE

class QPsdGlobalLayerMaskInfo::Private : public QSharedData
{
public:
    Private();
    quint32 length;
    quint16 overlayColorSpace;
    QString color;
    QPsdColorSpace colorSpace;
    quint16 opacity;
    Kind kind;
    QByteArray rawData;
};

QPsdGlobalLayerMaskInfo::Private::Private()
    : length(0)
    , overlayColorSpace(0)
    , opacity(0)
    , kind(ColorSelected)
{}

QPsdGlobalLayerMaskInfo::QPsdGlobalLayerMaskInfo()
    : QPsdSection()
    , d(new Private)
{}

QPsdGlobalLayerMaskInfo::QPsdGlobalLayerMaskInfo(QIODevice *source)
    : QPsdGlobalLayerMaskInfo()
{
    // Global layer mask info
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_17115

    // Length of global layer mask info section.
    d->length = readU32(source);
    if (d->length == 0)
        return;

    EnsureSeek es(source, d->length);

    // Overlay color space (undocumented) followed by 4 * 2 byte color components
    d->colorSpace.setId(static_cast<QPsdColorSpace::Id>(readU16(source)));
    d->colorSpace.color().raw.value1 = readU16(source);
    d->colorSpace.color().raw.value2 = readU16(source);
    d->colorSpace.color().raw.value3 = readU16(source);
    d->colorSpace.color().raw.value4 = readU16(source);

    d->overlayColorSpace = d->colorSpace.id();
    d->color = d->colorSpace.toString();

    // Opacity. 0 = transparent, 100 = opaque.
    d->opacity = readU16(source);

    // Kind. 0 = Color selected--i.e. inverted; 1 = Color protected;128 = use value stored per layer. This value is preferred. The others are for backward compatibility with beta versions.
    d->kind = static_cast<Kind>(readU8(source));

    // Filler/padding bytes (length > 13 means extra data beyond the standard 13 bytes)
    if (es.bytesAvailable() > 0)
        d->rawData = readByteArray(source, es.bytesAvailable());
}

QPsdGlobalLayerMaskInfo::QPsdGlobalLayerMaskInfo(const QPsdGlobalLayerMaskInfo &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdGlobalLayerMaskInfo &QPsdGlobalLayerMaskInfo::operator=(const QPsdGlobalLayerMaskInfo &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdGlobalLayerMaskInfo::~QPsdGlobalLayerMaskInfo() = default;

quint32 QPsdGlobalLayerMaskInfo::length() const
{
    return d->length;
}

quint16 QPsdGlobalLayerMaskInfo::overlayColorSpace() const
{
    return d->overlayColorSpace;
}

QString QPsdGlobalLayerMaskInfo::color() const
{
    return d->color;
}

QPsdColorSpace QPsdGlobalLayerMaskInfo::colorSpace() const
{
    return d->colorSpace;
}

quint16 QPsdGlobalLayerMaskInfo::opacity() const
{
    return d->opacity;
}

QPsdGlobalLayerMaskInfo::Kind QPsdGlobalLayerMaskInfo::kind() const
{
    return d->kind;
}

QByteArray QPsdGlobalLayerMaskInfo::rawData() const
{
    return d->rawData;
}

void QPsdGlobalLayerMaskInfo::setOverlayColorSpace(quint16 overlayColorSpace)
{
    d->overlayColorSpace = overlayColorSpace;
}

void QPsdGlobalLayerMaskInfo::setColor(const QString &color)
{
    d->color = color;
}

void QPsdGlobalLayerMaskInfo::setColorSpace(const QPsdColorSpace &colorSpace)
{
    d->colorSpace = colorSpace;
}

void QPsdGlobalLayerMaskInfo::setOpacity(quint16 opacity)
{
    d->opacity = opacity;
}

void QPsdGlobalLayerMaskInfo::setKind(Kind kind)
{
    d->kind = kind;
}

void QPsdGlobalLayerMaskInfo::setRawData(const QByteArray &rawData)
{
    d->rawData = rawData;
}

QT_END_NAMESPACE
