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
        const auto dithered = readU8(source, &length);
        QVariantMap result;
        result.insert(u"reverse"_s, reversed != 0);
        result.insert(u"dither"_s, dithered != 0);

        if (version == 3) {
            const auto method = readByteArray(source, 4, &length);
            result.insert(u"method"_s, QString::fromLatin1(method));
        }
        const auto name = readString(source, &length);
        result.insert(u"name"_s, name);

        const auto countColorStops = readU16(source, &length);
        QVariantList colorStops;
        for (quint16 i = 0; i < countColorStops; i++) {
            const auto location = readU32(source, &length);
            const auto midpoint = readU32(source, &length);
            const auto colorSpace = readColorSpace(source, &length);
            skip(source, 2, &length); // Unknown padding

            QVariantMap stop;
            stop.insert(u"location"_s, location);
            stop.insert(u"midpoint"_s, midpoint);
            stop.insert(u"color"_s, colorSpace.toString());
            colorStops.append(stop);
        }
        result.insert(u"colorStops"_s, colorStops);

        const auto countTransparencyStops = readU16(source, &length);
        QVariantList transparencyStops;
        for (quint16 i = 0; i < countTransparencyStops; i++) {
            const auto location = readU32(source, &length);
            const auto midpoint = readU32(source, &length);
            const auto transparency = readU16(source, &length);

            QVariantMap stop;
            stop.insert(u"location"_s, location);
            stop.insert(u"midpoint"_s, midpoint);
            stop.insert(u"opacity"_s, transparency);
            transparencyStops.append(stop);
        }
        result.insert(u"transparencyStops"_s, transparencyStops);

        const auto expansionCount = readU16(source, &length);
        Q_ASSERT(expansionCount == 2);

        const auto interpolation = readU16(source, &length);
        result.insert(u"interpolation"_s, interpolation);
        const auto len = readU16(source, &length);
        Q_ASSERT(len == 32);
        const auto gradientMode = readU16(source, &length);
        result.insert(u"gradientMode"_s, gradientMode);
        const auto randomSeed = readU32(source, &length);
        result.insert(u"randomSeed"_s, randomSeed);
        const auto showTransparency = readU16(source, &length);
        result.insert(u"showTransparency"_s, showTransparency != 0);
        const auto useVectorColor = readU16(source, &length);
        result.insert(u"useVectorColor"_s, useVectorColor != 0);
        const auto roughness = readU32(source, &length);
        result.insert(u"roughness"_s, roughness);

        const auto colorModel = readU16(source, &length);
        result.insert(u"colorModel"_s, colorModel);

        const auto minColor1 = readU16(source, &length);
        const auto minColor2 = readU16(source, &length);
        const auto minColor3 = readU16(source, &length);
        const auto minColor4 = readU16(source, &length);

        const auto maxColor1 = readU16(source, &length);
        const auto maxColor2 = readU16(source, &length);
        const auto maxColor3 = readU16(source, &length);
        const auto maxColor4 = readU16(source, &length);

        QVariantList minColor = {minColor1, minColor2, minColor3, minColor4};
        QVariantList maxColor = {maxColor1, maxColor2, maxColor3, maxColor4};
        result.insert(u"minColor"_s, minColor);
        result.insert(u"maxColor"_s, maxColor);

        skip(source, 2, &length);

        return result;
    }
};

QT_END_NAMESPACE

#include "grdm.moc"
