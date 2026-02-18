// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationMixrPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "mixr.json")
public:
    // Channel Mixer
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length <= 3);
        });

        const auto monochrome = readU16(source, &length) != 0;

        QVariantMap result;
        result.insert(u"monochrome"_s, monochrome);

        if (!monochrome) {
            result.insert(u"red"_s, readMixer(source, &length));
            result.insert(u"green"_s, readMixer(source, &length));
            result.insert(u"blue"_s, readMixer(source, &length));
        }
        result.insert(u"gray"_s, readMixer(source, &length));

        if (monochrome) {
            skip(source, 3 * 5 * 2, &length);
        }

        return result;
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        const auto map = data.toMap();
        const bool monochrome = map.value(u"monochrome"_s).toBool();
        writeU16(&io, monochrome ? 1 : 0);
        const auto writeMixerEntry = [&](const QString &key) {
            const auto m = map.value(key).toMap();
            writeS16(&io, static_cast<qint16>(m.value(u"red"_s).toInt()));
            writeS16(&io, static_cast<qint16>(m.value(u"green"_s).toInt()));
            writeS16(&io, static_cast<qint16>(m.value(u"blue"_s).toInt()));
            writeU16(&io, 0); // unknown v1
            writeS16(&io, static_cast<qint16>(m.value(u"constant"_s).toInt()));
        };
        if (!monochrome) {
            writeMixerEntry(u"red"_s);
            writeMixerEntry(u"green"_s);
            writeMixerEntry(u"blue"_s);
        }
        writeMixerEntry(u"gray"_s);
        if (monochrome) {
            io.write(QByteArray(3 * 5 * 2, '\0'));
        }
        return buf;
    }

    QVariant readMixer(QIODevice *source, quint32 *length) const {
        const auto red = readS16(source, length);
        const auto green = readS16(source, length);
        const auto blue = readS16(source, length);

        const auto v1 = readU16(source, length);
        Q_UNUSED(v1);

        const auto constant = readS16(source, length);

        QVariantMap result;
        result.insert(u"red"_s, red);
        result.insert(u"green"_s, green);
        result.insert(u"blue"_s, blue);
        result.insert(u"constant"_s, constant);
        return result;
    }
};

QT_END_NAMESPACE

#include "mixr.moc"
