// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdsection.h"
#include "qpsdcolorspace.h"

#include <QtCore/QStringDecoder>

QT_BEGIN_NAMESPACE

class QPsdSection::Private : public QSharedData
{
public:
    QString errorString;
};

QPsdSection::QPsdSection()
    : d(new Private)
{}

QPsdSection::QPsdSection(const QPsdSection &other)
    : d(other.d)
{}

QPsdSection &QPsdSection::operator=(const QPsdSection &other)
{
    if (this != &other)
        d.operator=(other.d);
    return *this;
}

QPsdSection::~QPsdSection() = default;

QString QPsdSection::errorString() const
{
    return d->errorString;
}

void QPsdSection::setErrorString(const QString &errorString)
{
    d->errorString = errorString;
    qWarning() << errorString;
}

QByteArray QPsdSection::readPascalString(QIODevice *source, int padding, quint32 *length)
{
    auto size = readU8(source, length);
    if ((size + 1) % padding > 0) {
        size += padding - (size + 1) % padding;
    }
    QByteArray ret = readByteArray(source, size, length);
    while (ret.endsWith('\0'))
        ret.chop(1);
    return ret;
}

QByteArray QPsdSection::readByteArray(QIODevice *source, quint32 size, quint32 *length)
{
    if (length) {
        if (size > *length) {
            qWarning("readByteArray: requested %u bytes with only %u bytes remaining; clamping",
                     size, *length);
            size = *length;
        }
        *length -= size;
    }
    return source->read(size);
}

QString QPsdSection::readString(QIODevice *source, quint32 *length)
{
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#UnicodeStringDefine
    // All values defined as Unicode string consist of:
    // A 4-byte length field, representing the number of UTF-16 code units in the string (not bytes).
    // The string of Unicode values, two bytes per character and a two byte null for the end of the string.
    const qint32 size = readS32(source, length);
    if (size <= 0) {
        if (size < 0)
            qWarning("readString: negative UTF-16 unit size %d; returning empty string", size);
        return {};
    }

    constexpr qint32 kMaxCodeUnits = 1024 * 1024;
    qint32 safeSize = size;
    if (safeSize > kMaxCodeUnits) {
        qWarning("readString: too large UTF-16 unit size %d; clamping to %d", safeSize, kMaxCodeUnits);
        safeSize = kMaxCodeUnits;
    }
    quint32 byteSize = static_cast<quint32>(safeSize) * 2;
    if (length && byteSize > *length) {
        qWarning("readString: requested %u bytes with only %u bytes remaining; clamping", byteSize, *length);
        byteSize = *length - (*length % 2);
    }

    const auto data = readByteArray(source, byteSize, length);
    QStringDecoder decoder(QStringDecoder::Utf16BE);
    QString ret = decoder.decode(data);
    if (ret.endsWith(QChar::Null))
        ret.chop(1);
    // else
    //     qWarning() << ret.left(80) << "is not null terminated with length" << size << *length;
    return ret;
}

QString QPsdSection::readStringLE(QIODevice *source, quint32 *length)
{
    const quint32 size = readU32LE(source, length);
    if (size == 0)
        return {};

    constexpr quint32 kMaxCodeUnits = 1024 * 1024;
    quint32 safeSize = size;
    if (safeSize > kMaxCodeUnits) {
        qWarning("readStringLE: too large UTF-16 unit size %u; clamping to %u", safeSize, kMaxCodeUnits);
        safeSize = kMaxCodeUnits;
    }
    quint32 byteSize = safeSize * 2;
    if (length && byteSize > *length) {
        qWarning("readStringLE: requested %u bytes with only %u bytes remaining; clamping", byteSize, *length);
        byteSize = *length - (*length % 2);
    }

    const auto data = readByteArray(source, byteSize, length);
    QStringDecoder decoder(QStringDecoder::Utf16LE);
    QString ret = decoder.decode(data);
    if (ret.endsWith(QChar::Null))
        ret.chop(1);
    // else
    //     qWarning() << ret.left(80) << "is not null terminated with length" << size << *length;
    return ret;
}

QRect QPsdSection::readRectangle(QIODevice *source, quint32 *length)
{
    QRect ret;
    qint32 top = readS32(source, length);
    ret.setTop(top);
    qint32 left = readS32(source, length);
    ret.setLeft(left);
    qint32 bottom = readS32(source, length);
    ret.setBottom(bottom - 1);
    qint32 right = readS32(source, length);
    ret.setRight(right - 1);
    return ret;
}

QString QPsdSection::readColor(QIODevice *source, quint32 *length)
{
    uint a = readU16(source, length) / 256;
    uint r = readU16(source, length) / 256;
    uint g = readU16(source, length) / 256;
    uint b = readU16(source, length) / 256;
    skip(source, 2, length);
    return QStringLiteral("#%1%2%3%4")
        .arg(QString::number(a, 16).rightJustified(2, u'0'))
        .arg(QString::number(r, 16).rightJustified(2, u'0'))
        .arg(QString::number(g, 16).rightJustified(2, u'0'))
        .arg(QString::number(b, 16).rightJustified(2, u'0'));
}

double QPsdSection::readPathNumber(QIODevice *source, quint32 *length)
{
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_17587
    const auto a = readS8(source, length);
    const uint32_t b1 = readU8(source, length);
    const uint32_t b2 = readU8(source, length);
    const uint32_t b3 = readU8(source, length);
    const uint32_t b = (b1 << 16) | (b2 << 8) | b3;
    const auto ret = static_cast<double>(a) + static_cast<double>(b) / std::pow(2, 24);
    // qDebug() << a << b1 << b2 << b3 << b << ret;
    return ret;
}

QPsdColorSpace QPsdSection::readColorSpace(QIODevice *source, quint32 *length)
{
    QPsdColorSpace colorSpace;
    
    // Read color space ID (2 bytes)
    colorSpace.setId(static_cast<QPsdColorSpace::Id>(readU16(source, length)));
    
    // Read four 16-bit color values (8 bytes total)
    colorSpace.color().raw.value1 = readU16(source, length);
    colorSpace.color().raw.value2 = readU16(source, length);
    colorSpace.color().raw.value3 = readU16(source, length);
    colorSpace.color().raw.value4 = readU16(source, length);
    
    return colorSpace;
}

QT_END_NAMESPACE
