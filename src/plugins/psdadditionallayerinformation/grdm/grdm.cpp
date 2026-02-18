// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsdcolorspace.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationGrdmPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "grdm.json")
public:
    // Gradient Map
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            if (length > 0)
                skip(source, length, &length);
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

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        const auto map = data.toMap();

        const bool hasMethod = map.contains(u"method"_s);
        writeU16(&io, hasMethod ? 3 : 1);  // version
        writeU8(&io, map.value(u"reverse"_s).toBool() ? 1 : 0);
        writeU8(&io, map.value(u"dither"_s).toBool() ? 1 : 0);

        if (hasMethod) {
            const auto method = map.value(u"method"_s).toString().toLatin1().leftJustified(4, '\0', true);
            io.write(method);
        }

        writeString(&io, map.value(u"name"_s).toString());

        const auto colorStops = map.value(u"colorStops"_s).toList();
        writeU16(&io, static_cast<quint16>(colorStops.size()));
        for (const auto &s : colorStops) {
            const auto stop = s.toMap();
            writeU32(&io, stop.value(u"location"_s).toUInt());
            writeU32(&io, stop.value(u"midpoint"_s).toUInt());
            io.write(QByteArray(10, '\0'));  // color space (lost during parse)
            io.write(QByteArray(2, '\0'));   // unknown padding
        }

        const auto transparencyStops = map.value(u"transparencyStops"_s).toList();
        writeU16(&io, static_cast<quint16>(transparencyStops.size()));
        for (const auto &s : transparencyStops) {
            const auto stop = s.toMap();
            writeU32(&io, stop.value(u"location"_s).toUInt());
            writeU32(&io, stop.value(u"midpoint"_s).toUInt());
            writeU16(&io, static_cast<quint16>(stop.value(u"opacity"_s).toUInt()));
        }

        writeU16(&io, 2);  // expansionCount
        writeU16(&io, static_cast<quint16>(map.value(u"interpolation"_s).toUInt()));
        writeU16(&io, 32);  // len
        writeU16(&io, static_cast<quint16>(map.value(u"gradientMode"_s).toUInt()));
        writeU32(&io, map.value(u"randomSeed"_s).toUInt());
        writeU16(&io, map.value(u"showTransparency"_s).toBool() ? 1 : 0);
        writeU16(&io, map.value(u"useVectorColor"_s).toBool() ? 1 : 0);
        writeU32(&io, map.value(u"roughness"_s).toUInt());
        writeU16(&io, static_cast<quint16>(map.value(u"colorModel"_s).toUInt()));

        const auto minColor = map.value(u"minColor"_s).toList();
        for (int i = 0; i < 4; ++i)
            writeU16(&io, static_cast<quint16>(i < minColor.size() ? minColor.at(i).toUInt() : 0));

        const auto maxColor = map.value(u"maxColor"_s).toList();
        for (int i = 0; i < 4; ++i)
            writeU16(&io, static_cast<quint16>(i < maxColor.size() ? maxColor.at(i).toUInt() : 0));

        io.write(QByteArray(2, '\0'));  // final padding

        return buf;
    }
};

QT_END_NAMESPACE

#include "grdm.moc"
