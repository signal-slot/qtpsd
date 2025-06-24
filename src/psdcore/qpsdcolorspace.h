// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDCOLORSPACE_H
#define QPSDCOLORSPACE_H

#include <QtPsdCore/qpsdcoreglobal.h>

#include <QtCore/QString>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdColorSpace
{
public:
    enum Id {
        RGB = 0,
        HSB = 1,
        CMYK = 2,
        Pantone = 3,
        Focoltone = 4,
        Trumatch = 5,
        Toyo = 6,
        Lab = 7,
        Grayscale = 8,
        HKS = 10
    };

    QPsdColorSpace();
    QPsdColorSpace(const QPsdColorSpace &other);
    QPsdColorSpace &operator=(const QPsdColorSpace &other);
    ~QPsdColorSpace();

    Id id() const;
    void setId(Id id);

    union ColorData {
        struct {
            quint16 value1;
            quint16 value2;
            quint16 value3;
            quint16 value4;
        } raw;
        struct {
            quint16 red;
            quint16 green;
            quint16 blue;
            quint16 unused;
        } rgb;
        struct {
            quint16 hue;
            quint16 saturation;
            quint16 brightness;
            quint16 unused;
        } hsb;
        struct {
            quint16 cyan;
            quint16 magenta;
            quint16 yellow;
            quint16 black;
        } cmyk;
        struct {
            quint16 lightness;
            quint16 a;
            quint16 b;
            quint16 unused;
        } lab;
        struct {
            quint16 gray;
            quint16 unused1;
            quint16 unused2;
            quint16 unused3;
        } grayscale;
    };

    const ColorData &color() const;
    ColorData &color();

    QString toString() const;

private:
    class Private;
    Private *d;
};

QT_END_NAMESPACE

#endif // QPSDCOLORSPACE_H