// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdcolorspace.h"

#include <QtCore/QtMath>

QT_BEGIN_NAMESPACE

class QPsdColorSpace::Private
{
public:
    QPsdColorSpace::Id id = QPsdColorSpace::Unknown;
    QPsdColorSpace::ColorData color = {};
};

QPsdColorSpace::QPsdColorSpace()
    : d(new Private)
{
}

QPsdColorSpace::QPsdColorSpace(const QPsdColorSpace &other)
    : d(new Private(*other.d))
{
}

QPsdColorSpace &QPsdColorSpace::operator=(const QPsdColorSpace &other)
{
    if (this != &other) {
        *d = *other.d;
    }
    return *this;
}

QPsdColorSpace::~QPsdColorSpace()
{
    delete d;
}

QPsdColorSpace::Id QPsdColorSpace::id() const
{
    return d->id;
}

void QPsdColorSpace::setId(Id id)
{
    d->id = id;
}

const QPsdColorSpace::ColorData &QPsdColorSpace::color() const
{
    return d->color;
}

QPsdColorSpace::ColorData &QPsdColorSpace::color()
{
    return d->color;
}

QString QPsdColorSpace::toString() const
{
    switch (d->id) {
    case Unknown:
        return QStringLiteral("Unknown");
    case RGB:
        // Convert from 16-bit to 8-bit values (0-65535 to 0-255)
        return QStringLiteral("#%1%2%3")
            .arg(QString::number(d->color.rgb.red / 256, 16).rightJustified(2, u'0'))
            .arg(QString::number(d->color.rgb.green / 256, 16).rightJustified(2, u'0'))
            .arg(QString::number(d->color.rgb.blue / 256, 16).rightJustified(2, u'0'));
    case Grayscale:
        // Grayscale uses value from 0-10000, convert to 0-255
        {
            uint gray = d->color.grayscale.gray * 255 / 10000;
            return QStringLiteral("#%1%1%1")
                .arg(QString::number(gray, 16).rightJustified(2, u'0'));
        }
    case CMYK:
        // CMYK: 0 = 100% ink, so invert and convert to RGB approximation
        // This is a simple conversion, not color-managed
        {
            uint c = 255 - (d->color.cmyk.cyan / 256);
            uint m = 255 - (d->color.cmyk.magenta / 256);
            uint y = 255 - (d->color.cmyk.yellow / 256);
            uint k = 255 - (d->color.cmyk.black / 256);
            uint r = c * k / 255;
            uint g = m * k / 255;
            uint b = y * k / 255;
            return QStringLiteral("#%1%2%3")
                .arg(QString::number(r, 16).rightJustified(2, u'0'))
                .arg(QString::number(g, 16).rightJustified(2, u'0'))
                .arg(QString::number(b, 16).rightJustified(2, u'0'));
        }
    case Lab:
        // CIELAB (D50, values stored x100) -> XYZ -> Bradford D50->D65 -> sRGB
        {
            const double L = static_cast<qint16>(d->color.lab.lightness) / 100.0;
            const double a = static_cast<qint16>(d->color.lab.a) / 100.0;
            const double b = static_cast<qint16>(d->color.lab.b) / 100.0;

            const double fy = (L + 16.0) / 116.0;
            const double fx = fy + a / 500.0;
            const double fz = fy - b / 200.0;
            const double e = 216.0 / 24389.0;
            const double k = 24389.0 / 27.0;
            auto finv = [&](double f) {
                const double f3 = f * f * f;
                return f3 > e ? f3 : (116.0 * f - 16.0) / k;
            };
            const double X = finv(fx) * 0.96422; // D50 white point
            const double Y = finv(fy) * 1.0;
            const double Z = finv(fz) * 0.82521;

            // Bradford chromatic adaptation D50 -> D65
            const double X2 = 0.9555766 * X - 0.0230393 * Y + 0.0631636 * Z;
            const double Y2 = -0.0282895 * X + 1.0099416 * Y + 0.0210077 * Z;
            const double Z2 = 0.0122982 * X - 0.0204830 * Y + 1.3299098 * Z;

            const double Rl = 3.2404542 * X2 - 1.5371385 * Y2 - 0.4985314 * Z2;
            const double Gl = -0.9692660 * X2 + 1.8760108 * Y2 + 0.0415560 * Z2;
            const double Bl = 0.0556434 * X2 - 0.2040259 * Y2 + 1.0572252 * Z2;
            auto enc = [](double c) {
                c = qBound(0.0, c, 1.0);
                c = c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
                return uint(qRound(c * 255.0));
            };
            return QStringLiteral("#%1%2%3")
                .arg(QString::number(enc(Rl), 16).rightJustified(2, u'0'))
                .arg(QString::number(enc(Gl), 16).rightJustified(2, u'0'))
                .arg(QString::number(enc(Bl), 16).rightJustified(2, u'0'));
        }
    case HSB:
        // HSB to RGB conversion
        // H: 0-65535 (0-360 degrees), S/B: 0-65535 (0-100%)
        {
            double h = d->color.hsb.hue / 65535.0 * 360.0;
            double s = d->color.hsb.saturation / 65535.0;
            double v = d->color.hsb.brightness / 65535.0;
            
            double c = v * s;
            double x = c * (1 - qAbs(fmod(h / 60.0, 2) - 1));
            double m = v - c;
            
            double r1, g1, b1;
            if (h < 60) {
                r1 = c; g1 = x; b1 = 0;
            } else if (h < 120) {
                r1 = x; g1 = c; b1 = 0;
            } else if (h < 180) {
                r1 = 0; g1 = c; b1 = x;
            } else if (h < 240) {
                r1 = 0; g1 = x; b1 = c;
            } else if (h < 300) {
                r1 = x; g1 = 0; b1 = c;
            } else {
                r1 = c; g1 = 0; b1 = x;
            }
            
            uint r = (r1 + m) * 255;
            uint g = (g1 + m) * 255;
            uint b = (b1 + m) * 255;
            return QStringLiteral("#%1%2%3")
                .arg(QString::number(r, 16).rightJustified(2, u'0'))
                .arg(QString::number(g, 16).rightJustified(2, u'0'))
                .arg(QString::number(b, 16).rightJustified(2, u'0'));
        }
    default:
        // For custom color spaces, return raw values as hex
        return QStringLiteral("#%1%2%3%4")
            .arg(QString::number(d->color.raw.value1 / 256, 16).rightJustified(2, u'0'))
            .arg(QString::number(d->color.raw.value2 / 256, 16).rightJustified(2, u'0'))
            .arg(QString::number(d->color.raw.value3 / 256, 16).rightJustified(2, u'0'))
            .arg(QString::number(d->color.raw.value4 / 256, 16).rightJustified(2, u'0'));
    }
}

bool QPsdColorSpace::isValid() const
{
    // Color space is valid if ID is not Unknown
    return d->id != Unknown;
}

bool QPsdColorSpace::operator==(const QPsdColorSpace &other) const
{
    if (d->id != other.d->id)
        return false;
    
    return d->color.raw.value1 == other.d->color.raw.value1 &&
           d->color.raw.value2 == other.d->color.raw.value2 &&
           d->color.raw.value3 == other.d->color.raw.value3 &&
           d->color.raw.value4 == other.d->color.raw.value4;
}

QT_END_NAMESPACE