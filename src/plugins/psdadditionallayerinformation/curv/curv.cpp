// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationCurvPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "curv.json")
public:
    // Curve
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length <= 3);
        });

        const auto v1 = readU8(source, &length);
        Q_UNUSED(v1);
        const auto version = readU16(source, &length);
        Q_ASSERT(version == 1);
        const auto channelsVersion = readU16(source, &length);
        Q_ASSERT(channelsVersion == 1 || channelsVersion == 4 || channelsVersion == 0);
        const auto channels = readU16(source, &length);

        QVariantMap result;

        if (channelsVersion != 4) {
            if (channels & 1) {
                result.insert(u"rgb"_s, readCurve(source, &length));
            }
            if (channels & 2) {
                result.insert(u"red"_s, readCurve(source, &length));
            }
            if (channels & 4) {
                result.insert(u"green"_s, readCurve(source, &length));
            }
            if (channels & 8) {
                result.insert(u"blue"_s, readCurve(source, &length));
            }
        } else {
            QVariantList curvesList;
            for (quint16 i = 0; i < channels; i++) {
                curvesList.append(readCurve(source, &length));
            }
            result.insert(u"curves"_s, curvesList);
        }

        const auto signature = readByteArray(source, 4, &length);
        Q_ASSERT(signature == "Crv ");
        const auto version2 = readU16(source, &length);
        Q_UNUSED(version2);
        const auto version3 = readU16(source, &length);
        Q_UNUSED(version3);
        const auto channels2 = readU16(source, &length);
        QVariantList extraCurves;
        for (quint16 i = 0; i < channels2; i++) {
            const auto index = readU16(source, &length);
            QVariantMap curveEntry;
            curveEntry.insert(u"channelIndex"_s, index);
            curveEntry.insert(u"points"_s, readCurve(source, &length));
            extraCurves.append(curveEntry);
        }
        if (!extraCurves.isEmpty())
            result.insert(u"extraCurves"_s, extraCurves);

        return result;
    }

    QVariant readCurve(QIODevice *source, quint32 *length) const {
        const auto count = readU16(source, length);
        Q_ASSERT(count * 2 <= *length);
        QVariantList points;
        for (quint16 i = 0; i < count; i++) {
            const auto input = readS16(source, length);
            const auto output = readS16(source, length);
            QVariantMap point;
            point.insert(u"input"_s, input);
            point.insert(u"output"_s, output);
            points.append(point);
        }
        return points;
    }
};

QT_END_NAMESPACE

#include "curv.moc"
