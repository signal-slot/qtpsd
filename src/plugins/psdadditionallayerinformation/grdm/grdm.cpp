// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsdcolorspace.h>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationGrdmPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "grdm.json")
public:
    // Gradient Map
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length <= 3);
        });

        const auto version = readU16(source, &length);
        Q_ASSERT(version == 1 || version == 3);
        const auto reversed = readU8(source, &length);
        Q_UNUSED(reversed);
        const auto dithered = readU8(source, &length);
        Q_UNUSED(dithered);
        if (version == 3) {
            // method == Gcls, Lnr, Perc or Smoo
            const auto method = readByteArray(source, 4, &length);
            Q_UNUSED(method);
        }
        const auto name = readString(source, &length);
        Q_UNUSED(name);

        const auto countColorStops = readU16(source, &length);
        for (quint16 i = 0; i < countColorStops; i++) {
            const auto location = readU32(source, &length);
            Q_UNUSED(location);
            const auto midpoint = readU32(source, &length);
            Q_UNUSED(midpoint);
            const auto colorSpace = readColorSpace(source, &length);
            const auto color = colorSpace.toString();
            Q_UNUSED(color); // TODO: Store gradient stop color when Grdm structure is implemented

            skip(source, 2, &length); // Unknown padding
        }

        const auto countTransparencyStops = readU16(source, &length);
        for (quint16 i = 0; i < countTransparencyStops; i++) {
            const auto location = readU32(source, &length);
            Q_UNUSED(location);
            const auto midpoint = readU32(source, &length);
            Q_UNUSED(midpoint);
            const auto tramsparency = readU16(source, &length);
            Q_UNUSED(tramsparency);
        }

        const auto expansionCount = readU16(source, &length);
        Q_ASSERT(expansionCount == 2);

        const auto interpolation = readU16(source, &length);
        Q_UNUSED(interpolation);
        const auto len = readU16(source, &length);
        Q_ASSERT(len == 32);
        const auto gradientMode = readU16(source, &length);
        Q_UNUSED(gradientMode);
        const auto randomSeed = readU32(source, &length);
        Q_UNUSED(randomSeed);
        const auto showTransparency = readU16(source, &length);
        Q_UNUSED(showTransparency);
        const auto useVectorColor = readU16(source, &length);
        Q_UNUSED(useVectorColor);
        const auto roughness = readU32(source, &length);
        Q_UNUSED(roughness);
        const auto colorModel = readU16(source, &length);
        Q_UNUSED(colorModel);

        const auto minColor1 = readU16(source, &length);
        Q_UNUSED(minColor1);
        const auto minColor2= readU16(source, &length);
        Q_UNUSED(minColor2);
        const auto minColor3 = readU16(source, &length);
        Q_UNUSED(minColor3);
        const auto minColor4 = readU16(source, &length);
        Q_UNUSED(minColor4);

        const auto maxColor1 = readU16(source, &length);
        Q_UNUSED(maxColor1);
        const auto maxColor2 = readU16(source, &length);
        Q_UNUSED(maxColor2);
        const auto maxColor3 = readU16(source, &length);
        Q_UNUSED(maxColor3);
        const auto maxColor4 = readU16(source, &length);
        Q_UNUSED(maxColor4);

        skip(source, 2, &length);

        return {};
    }
};

QT_END_NAMESPACE

#include "grdm.moc"
