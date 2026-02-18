// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdblend.h"

QT_BEGIN_NAMESPACE

namespace QPsdBlend {
Mode from(const QByteArray &key) {
    static const QHash<QByteArray, Mode> converter =
        {
         { "pass", PassThrough },
         { "norm", Normal },
         { "diss", Dissolve },
         { "dark", Darken },
         { "mul ", Multiply },
         { "idiv", ColorBurn },
         { "lbrn", LinearBurn },
         { "dkCl", DarkerColor },
         { "lite", Lighten },
         { "scrn", Screen },
         { "div ", ColorDodge },
         { "lddg", LinearDodge },
         { "lgCl", LighterColor },
         { "over", Overlay },
         { "sLit", SoftLight },
         { "hLit", HardLight },
         { "vLit", VividLight },
         { "lLit", LinearLight },
         { "pLit", PinLight },
         { "hMix", HardMix },
         { "diff", Difference },
         { "smud", Exclusion },
         { "fsub", Subtract },
         { "fdiv", Divide },
         { "hue ", Hue },
         { "sat ", Saturation },
         { "colr", Color },
         { "lum ", Luminosity },
         };
    return converter.value(key, Invalid);
}

QByteArray toKey(Mode mode) {
    static const QHash<Mode, QByteArray> converter =
        {
         { PassThrough, "pass" },
         { Normal, "norm" },
         { Dissolve, "diss" },
         { Darken, "dark" },
         { Multiply, "mul " },
         { ColorBurn, "idiv" },
         { LinearBurn, "lbrn" },
         { DarkerColor, "dkCl" },
         { Lighten, "lite" },
         { Screen, "scrn" },
         { ColorDodge, "div " },
         { LinearDodge, "lddg" },
         { LighterColor, "lgCl" },
         { Overlay, "over" },
         { SoftLight, "sLit" },
         { HardLight, "hLit" },
         { VividLight, "vLit" },
         { LinearLight, "lLit" },
         { PinLight, "pLit" },
         { HardMix, "hMix" },
         { Difference, "diff" },
         { Exclusion, "smud" },
         { Subtract, "fsub" },
         { Divide, "fdiv" },
         { Hue, "hue " },
         { Saturation, "sat " },
         { Color, "colr" },
         { Luminosity, "lum " },
         };
    return converter.value(mode, "norm");
}
}

QT_END_NAMESPACE
