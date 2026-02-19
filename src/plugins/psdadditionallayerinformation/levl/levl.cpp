// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsddescriptor.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationLevlPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "levl.json")
public:
    // Levels
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length == 0);
        });

        Q_ASSERT(length == 0x0278);

        const auto rgb = readLevels(source, &length);
        const auto red = readLevels(source, &length);
        const auto green = readLevels(source, &length);
        const auto blue = readLevels(source, &length);

        // has many unknown data.. (592 bytes)
        const auto unknownData = readByteArray(source, length, &length);

        QVariantMap result;
        result.insert(u"rgb"_s, rgb);
        result.insert(u"red"_s, red);
        result.insert(u"green"_s, green);
        result.insert(u"blue"_s, blue);
        result.insert(u"unknownData"_s, unknownData);
        return result;
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        const auto map = data.toMap();
        const auto writeLevels = [&](const QString &key) {
            const auto l = map.value(key).toMap();
            writeU16(&io, static_cast<quint16>(l.value(u"shadowInput"_s).toUInt()));
            writeU16(&io, static_cast<quint16>(l.value(u"highlightInput"_s).toUInt()));
            writeU16(&io, static_cast<quint16>(l.value(u"shadowOutput"_s).toUInt()));
            writeU16(&io, static_cast<quint16>(l.value(u"highlightOutput"_s).toUInt()));
            writeU16(&io, static_cast<quint16>(l.value(u"midtoneInput"_s).toUInt()));
        };
        writeLevels(u"rgb"_s);
        writeLevels(u"red"_s);
        writeLevels(u"green"_s);
        writeLevels(u"blue"_s);
        // Unknown data (592 bytes typically)
        const auto unknownData = map.value(u"unknownData"_s).toByteArray();
        if (!unknownData.isEmpty()) {
            io.write(unknownData);
        } else {
            io.write(QByteArray(592, '\0'));
        }
        return buf;
    }

    QVariant readLevels(QIODevice *source, quint32 *length) const {
        const auto shadowInput = readU16(source, length);
        const auto highlightInput = readU16(source, length);
        const auto shadowOutput = readU16(source, length);
        const auto highlightOutput = readU16(source, length);
        const auto midtoneInput = readU16(source, length);

        QVariantMap result;
        result.insert(u"shadowInput"_s, shadowInput);
        result.insert(u"highlightInput"_s, highlightInput);
        result.insert(u"shadowOutput"_s, shadowOutput);
        result.insert(u"highlightOutput"_s, highlightOutput);
        result.insert(u"midtoneInput"_s, midtoneInput);
        return result;
    }
};

QT_END_NAMESPACE

#include "levl.moc"
