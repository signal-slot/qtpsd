// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationSelcPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "selc.json")
public:
    // Selective color
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length <= 3);
        });

       const auto version = readU16(source, &length);
        Q_ASSERT(version == 1);

        const auto mode = readU16(source, &length);

        skip(source, 8, &length);

        QVariantMap result;
        result.insert(u"mode"_s, mode == 0 ? u"relative"_s : u"absolute"_s);
        result.insert(u"reds"_s, readSelectiveColors(source, &length));
        result.insert(u"yellows"_s, readSelectiveColors(source, &length));
        result.insert(u"greens"_s, readSelectiveColors(source, &length));
        result.insert(u"cyans"_s, readSelectiveColors(source, &length));
        result.insert(u"blues"_s, readSelectiveColors(source, &length));
        result.insert(u"magentas"_s, readSelectiveColors(source, &length));
        result.insert(u"whites"_s, readSelectiveColors(source, &length));
        result.insert(u"neutrals"_s, readSelectiveColors(source, &length));
        result.insert(u"blacks"_s, readSelectiveColors(source, &length));

        return result;
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        const auto map = data.toMap();
        writeU16(&io, 1); // version
        const quint16 mode = map.value(u"mode"_s).toString() == u"absolute"_s ? 1 : 0;
        writeU16(&io, mode);
        io.write(QByteArray(8, '\0')); // padding
        const QStringList keys = {u"reds"_s, u"yellows"_s, u"greens"_s, u"cyans"_s, u"blues"_s, u"magentas"_s, u"whites"_s, u"neutrals"_s, u"blacks"_s};
        for (const auto &key : keys) {
            const auto sc = map.value(key).toMap();
            writeS16(&io, static_cast<qint16>(sc.value(u"c"_s).toInt()));
            writeS16(&io, static_cast<qint16>(sc.value(u"m"_s).toInt()));
            writeS16(&io, static_cast<qint16>(sc.value(u"y"_s).toInt()));
            writeS16(&io, static_cast<qint16>(sc.value(u"k"_s).toInt()));
        }
        return buf;
    }

    QVariant readSelectiveColors(QIODevice *source, quint32 *length) const {
        const auto c = readS16(source, length);
        const auto m = readS16(source, length);
        const auto y = readS16(source, length);
        const auto k = readS16(source, length);

        QVariantMap result;
        result.insert(u"c"_s, c);
        result.insert(u"m"_s, m);
        result.insert(u"y"_s, y);
        result.insert(u"k"_s, k);
        return result;
    }
};

QT_END_NAMESPACE

#include "selc.moc"
