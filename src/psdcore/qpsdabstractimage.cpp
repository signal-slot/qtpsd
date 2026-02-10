// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdabstractimage.h"
#include "qpsdfileheader.h"

#include <cmath>
#include <QtEndian>

QT_BEGIN_NAMESPACE

class QPsdAbstractImage::Private : public QSharedData
{
public:
    quint32 width = 0;
    quint32 height = 0;
    quint8 opacity = 0;
    QPsdFileHeader header;
};

QPsdAbstractImage::QPsdAbstractImage()
    : QPsdSection()
    , d(new Private)
{}

QPsdAbstractImage::QPsdAbstractImage(const QPsdAbstractImage &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdAbstractImage &QPsdAbstractImage::operator=(const QPsdAbstractImage &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdAbstractImage::~QPsdAbstractImage() = default;

quint32 QPsdAbstractImage::width() const
{
    return d->width;
}

void QPsdAbstractImage::setWidth(quint32 width)
{
    d->width = width;
}

quint32 QPsdAbstractImage::height() const
{
    return d->height;
}

void QPsdAbstractImage::setHeight(quint32 height)
{
    d->height = height;
}

quint16 QPsdAbstractImage::depth() const
{
    // Return header depth if set (non-zero), otherwise default to 8
    quint16 headerDepth = d->header.depth();
    return headerDepth ? headerDepth : 8;
}

quint8 QPsdAbstractImage::opacity() const
{
    return d->opacity;
}

void QPsdAbstractImage::setOpacity(quint8 opacity)
{
    d->opacity = opacity;
}

QPsdFileHeader QPsdAbstractImage::header() const
{
    return d->header;
}

void QPsdAbstractImage::setHeader(const QPsdFileHeader &header)
{
    d->header = header;
}

QByteArray QPsdAbstractImage::readRLE(QIODevice *source, int height, quint32 *length)
{
    QByteArray ret;
    QList<qint16> byteCounts;
    for (int y = 0; y < height; y++) {
        byteCounts.append(readS16(source, length));
    }
    for (qint16 byteCount : byteCounts) {
        EnsureSeek es(source, byteCount);
        while (es.bytesAvailable() > 0) {
            auto size = readS8(source, length);
            if (size == -128) {
                // ignore size == -128 for padding
            } else if (size < 0) {
                ret.append(-size + 1, readByteArray(source, 1, length).at(0));
            } else if (size >= 0) {
                ret.append(readByteArray(source, size + 1, length));
            }
        }
    }
    return ret;
}

QByteArray QPsdAbstractImage::readZip(QIODevice *source, quint32 *length)
{
    quint32 beLength = qToBigEndian(*length);
    QByteArray zipData = readByteArray(source, *length, length);
    zipData.prepend(reinterpret_cast<char *>(&beLength), sizeof(quint32));

    return qUncompress(zipData);
}

QByteArray QPsdAbstractImage::toImage(QPsdFileHeader::ColorMode colorMode) const
{
    QByteArray ret;
    const auto size = width() * height();
    const auto bytesPerChannel = depth() / 8;
    switch (colorMode) {
    case QPsdFileHeader::Bitmap:
    case QPsdFileHeader::Grayscale:
    case QPsdFileHeader::Indexed:
        // For bitmap and grayscale, imageData() contains the raw pixel data
        // For indexed, this returns palette indices that need to be converted to RGB using the color table
        // but we should verify it matches the expected size
        ret = imageData();
        break;
    case QPsdFileHeader::RGB: {
        auto pr = r();
        auto pg = g();
        auto pb = b();
        auto pa = a();
        double o = opacity();

        if (bytesPerChannel == 0 || bytesPerChannel == 1) {
            // 8-bit per channel
            for (quint32 i = 0; i < size; i++) {
                ret.append(*pb++);
                ret.append(*pg++);
                ret.append(*pr++);
                if (pa) {
                    ret.append(*pa++ * o / 0xff);
                }
            }
        } else if (bytesPerChannel == 2) {
            // 16-bit per channel
            for (quint32 i = 0; i < size; i++) {
                // Append 2 bytes for each channel
                ret.append(reinterpret_cast<const char*>(pb), 2);
                pb += 2;
                ret.append(reinterpret_cast<const char*>(pg), 2);
                pg += 2;
                ret.append(reinterpret_cast<const char*>(pr), 2);
                pr += 2;
                if (pa) {
                    // For 16-bit, we need to scale opacity differently
                    quint16 alpha = *reinterpret_cast<const quint16*>(pa);
                    alpha = static_cast<quint16>(alpha * o / 0xff);
                    ret.append(reinterpret_cast<const char*>(&alpha), 2);
                    pa += 2;
                }
            }
        } else if (bytesPerChannel == 4) {
            // 32-bit per channel (float) - PSD stores floats in big-endian
            for (quint32 i = 0; i < size; i++) {
                // Convert from big-endian to native endianness
                quint32 bVal = qFromBigEndian<quint32>(pb);
                quint32 gVal = qFromBigEndian<quint32>(pg);
                quint32 rVal = qFromBigEndian<quint32>(pr);
                ret.append(reinterpret_cast<const char*>(&bVal), 4);
                pb += 4;
                ret.append(reinterpret_cast<const char*>(&gVal), 4);
                pg += 4;
                ret.append(reinterpret_cast<const char*>(&rVal), 4);
                pr += 4;
                if (pa) {
                    quint32 aVal = qFromBigEndian<quint32>(pa);
                    float alpha;
                    memcpy(&alpha, &aVal, 4);
                    alpha = alpha * static_cast<float>(o / 0xff);
                    ret.append(reinterpret_cast<const char*>(&alpha), 4);
                    pa += 4;
                }
            }
        } else {
            qWarning() << "bytesPerChannel" << bytesPerChannel << "not supported";
        }
        break; }
    case QPsdFileHeader::CMYK: {
        if (bytesPerChannel == 1) {
            auto pc = c();  // Channel 0 = Cyan
            if (pc) {
                auto pm = m();  // Channel 1 = Magenta
                auto py = y();  // Channel 2 = Yellow
                auto pk = k();  // Channel 3 = Black (K)

                const auto size = width() * height();
                for (quint32 i = 0; i < size; i++) {
                    // CMYK order for QImage::Format_CMYK8888
                    ret.append(255 - *pc++);  // C
                    ret.append(255 - *pm++);  // M
                    ret.append(255 - *py++);  // Y
                    ret.append(255 - *pk++);  // K
                }
            } else {
                // Fallback: single-channel data treated as grayscale CMYK
                const auto data = imageData();
                for (quint32 i = 0; i < size; i++) {
                    quint8 value = static_cast<quint8>(data[i]);
                    quint8 cmykValue = value ? 0 : 255;
                    ret.append(cmykValue);  // C
                    ret.append(cmykValue);  // M
                    ret.append(cmykValue);  // Y
                    ret.append(cmykValue);  // K
                }
            }
        } else if (bytesPerChannel == 2) {
            // 16-bit CMYK - convert to 8-bit
            auto pc = c();  // Channel 0 = Cyan
            auto pm = m();  // Channel 1 = Magenta
            auto py = y();  // Channel 2 = Yellow
            auto pk = k();  // Channel 3 = Black (K)

            const auto size = width() * height();
            for (quint32 i = 0; i < size; i++) {
                // Read 16-bit values and convert to 8-bit
                quint16 c16 = *reinterpret_cast<const quint16*>(pc);
                quint16 m16 = *reinterpret_cast<const quint16*>(pm);
                quint16 y16 = *reinterpret_cast<const quint16*>(py);
                quint16 k16 = *reinterpret_cast<const quint16*>(pk);

                // Convert 16-bit to 8-bit by taking high byte
                // PSD stores CMYK inverted, so we need to invert back
                ret.append(255 - (c16 >> 8));  // C
                ret.append(255 - (m16 >> 8));  // M
                ret.append(255 - (y16 >> 8));  // Y
                ret.append(255 - (k16 >> 8));  // K

                pc += 2;
                pm += 2;
                py += 2;
                pk += 2;
            }
        } else {
            qWarning() << "bytesPerChannel" << bytesPerChannel << "not supported";
        }
        break; }
    case QPsdFileHeader::Lab: {
        if (bytesPerChannel == 1) {
            // 8-bit Lab
            auto pL = b();  // Lightness channel
            auto pa = r();     // a channel (green-red)
            auto pb = g();     // b channel (blue-yellow)

            const auto size = width() * height();
            for (quint32 i = 0; i < size; i++) {
                // Convert Lab to RGB
                // L: 0-255 maps to 0-100
                // a,b: 0-255 maps to -128 to +127 (128 is neutral)
                float L = (*pL++) * 100.0f / 255.0f;
                float a = (*pa++) - 128.0f;
                float b = (*pb++) - 128.0f;

                // Lab to XYZ conversion
                float fy = (L + 16.0f) / 116.0f;
                float fx = a / 500.0f + fy;
                float fz = fy - b / 200.0f;

                // Helper function for f^-1
                auto finv = [](float t) -> float {
                    const float delta = 6.0f / 29.0f;
                    if (t > delta) {
                        return t * t * t;
                    } else {
                        return 3.0f * delta * delta * (t - 4.0f / 29.0f);
                    }
                };

                // D65 illuminant
                const float Xn = 0.95047f;
                const float Yn = 1.00000f;
                const float Zn = 1.08883f;

                float X = Xn * finv(fx);
                float Y = Yn * finv(fy);
                float Z = Zn * finv(fz);

                // XYZ to RGB (sRGB matrix)
                float R = 3.2404542f * X - 1.5371385f * Y - 0.4985314f * Z;
                float G = -0.9692660f * X + 1.8760108f * Y + 0.0415560f * Z;
                float B = 0.0556434f * X - 0.2040259f * Y + 1.0572252f * Z;

                // Apply gamma correction and clamp
                auto gammaCorrect = [](float c) -> quint8 {
                    if (c <= 0.0031308f) {
                        c = 12.92f * c;
                    } else {
                        c = 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
                    }
                    c = qBound(0.0f, c, 1.0f);
                    return static_cast<quint8>(c * 255.0f);
                };

                ret.append(gammaCorrect(R));
                ret.append(gammaCorrect(G));
                ret.append(gammaCorrect(B));
            }
        } else if (bytesPerChannel == 2) {
            // 16-bit Lab
            auto pL = gray();  // Lightness channel
            auto pa = r();     // a channel
            auto pb = g();     // b channel

            const auto size = width() * height();
            for (quint32 i = 0; i < size; i++) {
                // Read 16-bit values
                quint16 L16 = *reinterpret_cast<const quint16*>(pL);
                quint16 a16 = *reinterpret_cast<const quint16*>(pa);
                quint16 b16 = *reinterpret_cast<const quint16*>(pb);

                // Convert to 8-bit for now (can be improved later)
                quint8 L8 = L16 >> 8;
                quint8 a8 = a16 >> 8;
                quint8 b8 = b16 >> 8;

                // Convert Lab to RGB using same logic as 8-bit
                float L = L8 * 100.0f / 255.0f;
                float a = a8 - 128.0f;
                float b = b8 - 128.0f;

                // Lab to XYZ conversion
                float fy = (L + 16.0f) / 116.0f;
                float fx = a / 500.0f + fy;
                float fz = fy - b / 200.0f;

                // Helper function for f^-1
                auto finv = [](float t) -> float {
                    const float delta = 6.0f / 29.0f;
                    if (t > delta) {
                        return t * t * t;
                    } else {
                        return 3.0f * delta * delta * (t - 4.0f / 29.0f);
                    }
                };

                // D65 illuminant
                const float Xn = 0.95047f;
                const float Yn = 1.00000f;
                const float Zn = 1.08883f;

                float X = Xn * finv(fx);
                float Y = Yn * finv(fy);
                float Z = Zn * finv(fz);

                // XYZ to RGB (sRGB matrix)
                float R = 3.2404542f * X - 1.5371385f * Y - 0.4985314f * Z;
                float G = -0.9692660f * X + 1.8760108f * Y + 0.0415560f * Z;
                float B = 0.0556434f * X - 0.2040259f * Y + 1.0572252f * Z;

                // Apply gamma correction and clamp
                auto gammaCorrect = [](float c) -> quint8 {
                    if (c <= 0.0031308f) {
                        c = 12.92f * c;
                    } else {
                        c = 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
                    }
                    c = qBound(0.0f, c, 1.0f);
                    return static_cast<quint8>(c * 255.0f);
                };

                ret.append(gammaCorrect(R));
                ret.append(gammaCorrect(G));
                ret.append(gammaCorrect(B));

                pL += 2;
                pa += 2;
                pb += 2;
            }
        } else {
            qWarning() << "bytesPerChannel" << bytesPerChannel << "not supported for Lab color mode";
        }
        break; }
    case QPsdFileHeader::Multichannel: {
        // Multichannel mode - convert first channel to grayscale
        // This mode is typically used for spot colors in printing
        if (bytesPerChannel == 1) {
            // 8-bit multichannel
            auto pChan0 = gray();  // Use first channel
            const auto size = width() * height();
            for (quint32 i = 0; i < size; i++) {
                ret.append(*pChan0++);
            }
        } else if (bytesPerChannel == 2) {
            // 16-bit multichannel - convert to 8-bit
            auto pChan0 = gray();  // Use first channel
            const auto size = width() * height();
            for (quint32 i = 0; i < size; i++) {
                quint16 val16 = *reinterpret_cast<const quint16*>(pChan0);
                ret.append(val16 >> 8);  // Take high byte
                pChan0 += 2;
            }
        } else {
            qWarning() << "bytesPerChannel" << bytesPerChannel << "not supported for Multichannel mode";
        }
        break; }
    case QPsdFileHeader::Duotone: {
        // Duotone mode - treat as grayscale for display
        // The actual duotone colors are stored in the color mode data section
        if (bytesPerChannel == 1) {
            // 8-bit duotone
            auto pGray = gray();
            const auto size = width() * height();
            for (quint32 i = 0; i < size; i++) {
                ret.append(*pGray++);
            }
        } else if (bytesPerChannel == 2) {
            // 16-bit duotone - convert to 8-bit
            auto pGray = gray();
            const auto size = width() * height();
            for (quint32 i = 0; i < size; i++) {
                quint16 val16 = *reinterpret_cast<const quint16*>(pGray);
                ret.append(val16 >> 8);  // Take high byte
                pGray += 2;
            }
        } else {
            qWarning() << "bytesPerChannel" << bytesPerChannel << "not supported for Duotone mode";
        }
        break; }
    default:
        qFatal("Color mode %d not supported", colorMode);
    }
    return ret;
}

QT_END_NAMESPACE
