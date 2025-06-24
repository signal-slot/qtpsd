// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdcolorspace.h"

#include <QtCore/QtMath>

QT_BEGIN_NAMESPACE

class QPsdColorSpace::Private
{
public:
    QPsdColorSpace::Id id = QPsdColorSpace::RGB;
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
        // Lab color space - simplified conversion to RGB
        // L: 0-10000, a/b: -12800 to 12700
        // This is a very basic approximation
        {
            double L = d->color.lab.lightness / 100.0;
            double a = (static_cast<qint16>(d->color.lab.a) + 12800) / 255.0 - 50.0;
            double b = (static_cast<qint16>(d->color.lab.b) + 12800) / 255.0 - 50.0;
            // Simplified Lab to RGB (not accurate, just for display)
            uint r = qBound(0.0, L + a * 2, 255.0);
            uint g = qBound(0.0, L - a - b, 255.0);
            uint bl = qBound(0.0, L + b * 2, 255.0);
            return QStringLiteral("#%1%2%3")
                .arg(QString::number(r, 16).rightJustified(2, u'0'))
                .arg(QString::number(g, 16).rightJustified(2, u'0'))
                .arg(QString::number(bl, 16).rightJustified(2, u'0'));
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

QT_END_NAMESPACE