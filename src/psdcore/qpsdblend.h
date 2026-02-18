// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDBLENDH_H
#define QPSDBLENDH_H

#include <QtPsdCore/qpsdcoreglobal.h>

QT_BEGIN_NAMESPACE

// Blend mode key:
// 'pass' = pass through, 'norm' = normal, 'diss' = dissolve, 'dark' = darken, 'mul ' = multiply
// , 'idiv' = color burn, 'lbrn' = linear burn, 'dkCl' = darker color, 'lite' = lighten
// , 'scrn' = screen, 'div ' = color dodge, 'lddg' = linear dodge, 'lgCl' = lighter color
// , 'over' = overlay, 'sLit' = soft light, 'hLit' = hard light, 'vLit' = vivid light/
// , 'lLit' = linear light, 'pLit' = pin light, 'hMix' = hard mix, 'diff' = difference
// , 'smud' = exclusion, 'fsub' = subtract, 'fdiv' = divide 'hue ' = hue, 'sat ' = saturation
// , 'colr' = color, 'lum ' = luminosity,

namespace QPsdBlend {
    Q_NAMESPACE_EXPORT(Q_PSDCORE_EXPORT)
    enum Mode {
        Invalid,
        PassThrough,
        Normal,
        Dissolve,
        Darken,
        Multiply,
        ColorBurn,
        LinearBurn,
        DarkerColor,
        Lighten,
        Screen,
        ColorDodge,
        LinearDodge,
        LighterColor,
        Overlay,
        SoftLight,
        HardLight,
        VividLight,
        LinearLight,
        PinLight,
        HardMix,
        Difference,
        Exclusion,
        Subtract,
        Divide,
        Hue,
        Saturation,
        Color,
        Luminosity,
    };

    Q_PSDCORE_EXPORT Mode from(const QByteArray &key);
    Q_PSDCORE_EXPORT QByteArray toKey(Mode mode);
}

#endif // QPSDBLENDH_H
