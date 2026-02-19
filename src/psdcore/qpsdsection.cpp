// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdsection.h"
#include "qpsdcolorspace.h"

#include <QtCore/QStringDecoder>
#include <QtCore/QStringEncoder>

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

void QPsdSection::writeString(QIODevice *dest, const QString &str)
{
    // Unicode string: U32 charCount + UTF-16BE code units
    // The charCount includes the null terminator
    if (str.isEmpty()) {
        writeS32(dest, 0);
        return;
    }
    QStringEncoder encoder(QStringEncoder::Utf16BE);
    QByteArray encoded = encoder.encode(str);
    // charCount = number of UTF-16 code units + 1 for null terminator
    qint32 charCount = encoded.size() / 2 + 1;
    writeS32(dest, charCount);
    dest->write(encoded);
    // Write null terminator (2 bytes)
    dest->write(QByteArray(2, '\0'));
}

void QPsdSection::writeColorSpace(QIODevice *dest, const QPsdColorSpace &cs)
{
    writeU16(dest, static_cast<quint16>(cs.id()));
    writeU16(dest, cs.color().raw.value1);
    writeU16(dest, cs.color().raw.value2);
    writeU16(dest, cs.color().raw.value3);
    writeU16(dest, cs.color().raw.value4);
}

void QPsdSection::writePathNumber(QIODevice *dest, double value)
{
    // Fixed-point 8.24 format: 1 byte integer + 3 bytes fraction
    auto a = static_cast<qint8>(value);
    double frac = value - static_cast<double>(a);
    if (frac < 0) {
        a--;
        frac += 1.0;
    }
    auto b = static_cast<quint32>(frac * std::pow(2, 24) + 0.5);
    if (b > 0xFFFFFF)
        b = 0xFFFFFF;
    writeU8(dest, static_cast<quint8>(a));
    writeU8(dest, static_cast<quint8>((b >> 16) & 0xFF));
    writeU8(dest, static_cast<quint8>((b >> 8) & 0xFF));
    writeU8(dest, static_cast<quint8>(b & 0xFF));
}

QByteArray QPsdSection::encodePackBits(const QByteArray &rawData, int rowWidth, int height)
{
    // PSD RLE format:
    // height × uint16 byte counts (per scan line)
    // followed by PackBits-encoded data for each scan line

    QByteArray result;
    QList<QByteArray> encodedRows;
    encodedRows.reserve(height);

    for (int y = 0; y < height; ++y) {
        const int offset = y * rowWidth;
        const char *row = rawData.constData() + offset;
        const int len = qMin(rowWidth, rawData.size() - offset);

        QByteArray encoded;
        int i = 0;
        while (i < len) {
            // Look for a run of identical bytes (minimum 2)
            int runStart = i;
            while (i + 1 < len && row[i] == row[i + 1] && i - runStart < 127) {
                ++i;
            }
            int runLen = i - runStart + 1;

            if (runLen >= 2) {
                // Run: emit [-(runLen-1)] [byte]
                encoded.append(static_cast<char>(-(runLen - 1)));
                encoded.append(row[runStart]);
                ++i;
            } else {
                // Literal sequence: collect non-run bytes
                int litStart = runStart;
                i = runStart;
                while (i < len) {
                    // Check if a run of 2+ starts here
                    if (i + 1 < len && row[i] == row[i + 1]) {
                        break;
                    }
                    ++i;
                    if (i - litStart >= 128)
                        break;
                }
                int litLen = i - litStart;
                encoded.append(static_cast<char>(litLen - 1));
                encoded.append(row + litStart, litLen);
            }
        }
        encodedRows.append(encoded);
    }

    // Write byte counts (uint16 per row)
    for (const auto &row : encodedRows) {
        quint16 be = qToBigEndian(static_cast<quint16>(row.size()));
        result.append(reinterpret_cast<const char *>(&be), 2);
    }
    // Write encoded row data
    for (const auto &row : encodedRows) {
        result.append(row);
    }

    return result;
}

QT_END_NAMESPACE
