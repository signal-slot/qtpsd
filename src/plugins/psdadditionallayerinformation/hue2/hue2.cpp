// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationHue2Plugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "hue2.json")
public:
    // New Hue/saturation, Photoshop 5.0
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            if (length > 3)
                qWarning("hue2: %u bytes remaining after parse", length);
        });

       const auto version = readU16(source, &length);
        Q_ASSERT(version == 2);

        const auto master = readHue(source, &length);
        const auto reds = readHue(source, &length);
        const auto yellows = readHue(source, &length);
        const auto greens = readHue(source, &length);
        const auto cyans = readHue(source, &length);
        const auto blues = readHue(source, &length);
        const auto magentas = readHue(source, &length);

        // Read any remaining bytes (colorize flag, range data, padding)
        QByteArray trailingData;
        if (length > 0)
            trailingData = readByteArray(source, length, &length);

        QVariantMap result;
        result.insert(u"master"_s, master);
        result.insert(u"reds"_s, reds);
        result.insert(u"yellows"_s, yellows);
        result.insert(u"greens"_s, greens);
        result.insert(u"cyans"_s, cyans);
        result.insert(u"blues"_s, blues);
        result.insert(u"magentas"_s, magentas);
        if (!trailingData.isEmpty())
            result.insert(u"trailingData"_s, trailingData);
        return result;
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        const auto map = data.toMap();
        writeU16(&io, 2); // version
        const QStringList keys = {u"master"_s, u"reds"_s, u"yellows"_s, u"greens"_s, u"cyans"_s, u"blues"_s, u"magentas"_s};
        for (const auto &key : keys) {
            const auto h = map.value(key).toMap();
            writeS16(&io, static_cast<qint16>(h.value(u"a"_s).toInt()));
            writeS16(&io, static_cast<qint16>(h.value(u"b"_s).toInt()));
            writeS16(&io, static_cast<qint16>(h.value(u"c"_s).toInt()));
            writeS16(&io, static_cast<qint16>(h.value(u"d"_s).toInt()));
            writeS16(&io, static_cast<qint16>(h.value(u"hue"_s).toInt()));
            writeS16(&io, static_cast<qint16>(h.value(u"saturation"_s).toInt()));
            writeS16(&io, static_cast<qint16>(h.value(u"lightness"_s).toInt()));
        }
        const auto trailingData = map.value(u"trailingData"_s).toByteArray();
        if (!trailingData.isEmpty())
            io.write(trailingData);
        return buf;
    }

    QVariant readHue(QIODevice *source, quint32 *length) const {
        const auto a = readS16(source, length);
        const auto b = readS16(source, length);
        const auto c = readS16(source, length);
        const auto d = readS16(source, length);
        const auto hue = readS16(source, length);
        const auto saturation = readS16(source, length);
        const auto lightness = readS16(source, length);

        QVariantMap result;
        result.insert(u"a"_s, a);
        result.insert(u"b"_s, b);
        result.insert(u"c"_s, c);
        result.insert(u"d"_s, d);
        result.insert(u"hue"_s, hue);
        result.insert(u"saturation"_s, saturation);
        result.insert(u"lightness"_s, lightness);
        return result;
    }
};

QT_END_NAMESPACE

#include "hue2.moc"
