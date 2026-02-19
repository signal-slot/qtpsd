// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

#include <QtCore/QBuffer>

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
        const auto version = readU16(source, &length);
        Q_ASSERT(version == 1);
        const auto channelsVersion = readU16(source, &length);
        Q_ASSERT(channelsVersion == 1 || channelsVersion == 4 || channelsVersion == 0);
        const auto channels = readU16(source, &length);

        QVariantMap result;
        result.insert(u"v1"_s, v1);
        result.insert(u"channelsVersion"_s, channelsVersion);

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
        const auto version3 = readU16(source, &length);
        result.insert(u"version2"_s, version2);
        result.insert(u"version3"_s, version3);
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

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        const auto map = data.toMap();

        const auto writeCurve = [&](const QVariantList &points) {
            writeU16(&io, static_cast<quint16>(points.size()));
            for (const auto &p : points) {
                const auto pt = p.toMap();
                writeS16(&io, static_cast<qint16>(pt.value(u"input"_s).toInt()));
                writeS16(&io, static_cast<qint16>(pt.value(u"output"_s).toInt()));
            }
        };

        writeU8(&io, static_cast<quint8>(map.value(u"v1"_s).toUInt()));
        writeU16(&io, 1);  // version

        const auto channelsVersion = static_cast<quint16>(map.value(u"channelsVersion"_s, 1).toUInt());
        if (channelsVersion == 4) {
            const auto curves = map.value(u"curves"_s).toList();
            writeU16(&io, 4);
            writeU16(&io, static_cast<quint16>(curves.size()));
            for (const auto &c : curves)
                writeCurve(c.toList());
        } else {
            quint16 channels = 0;
            if (map.contains(u"rgb"_s)) channels |= 1;
            if (map.contains(u"red"_s)) channels |= 2;
            if (map.contains(u"green"_s)) channels |= 4;
            if (map.contains(u"blue"_s)) channels |= 8;
            writeU16(&io, channelsVersion);
            writeU16(&io, channels);
            if (channels & 1) writeCurve(map.value(u"rgb"_s).toList());
            if (channels & 2) writeCurve(map.value(u"red"_s).toList());
            if (channels & 4) writeCurve(map.value(u"green"_s).toList());
            if (channels & 8) writeCurve(map.value(u"blue"_s).toList());
        }

        io.write("Crv ", 4);
        writeU16(&io, static_cast<quint16>(map.value(u"version2"_s).toUInt()));
        writeU16(&io, static_cast<quint16>(map.value(u"version3"_s).toUInt()));

        const auto extraCurves = map.value(u"extraCurves"_s).toList();
        writeU16(&io, static_cast<quint16>(extraCurves.size()));
        for (const auto &entry : extraCurves) {
            const auto e = entry.toMap();
            writeU16(&io, static_cast<quint16>(e.value(u"channelIndex"_s).toUInt()));
            writeCurve(e.value(u"points"_s).toList());
        }

        return buf;
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
