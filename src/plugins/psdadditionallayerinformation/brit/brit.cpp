// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationBritPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "brit.json")
public:
    // Brightness/Contrast
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length == 0);
        });

        const auto brightness = readS16(source, &length);
        const auto contrast = readS16(source, &length);
        const auto mean = readU16(source, &length);
        const auto lab = readU8(source, &length);
        // padding
        skip(source, 1, &length);

        QVariantMap result;
        result.insert(u"brightness"_s, brightness);
        result.insert(u"contrast"_s, contrast);
        result.insert(u"meanValue"_s, mean);
        result.insert(u"labColorOnly"_s, lab != 0);
        return result;
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        const auto map = data.toMap();
        writeS16(&io, static_cast<qint16>(map.value(u"brightness"_s).toInt()));
        writeS16(&io, static_cast<qint16>(map.value(u"contrast"_s).toInt()));
        writeU16(&io, static_cast<quint16>(map.value(u"meanValue"_s).toUInt()));
        writeU8(&io, map.value(u"labColorOnly"_s).toBool() ? 1 : 0);
        io.write(QByteArray(1, '\0')); // padding
        return buf;
    }
};

QT_END_NAMESPACE

#include "brit.moc"
