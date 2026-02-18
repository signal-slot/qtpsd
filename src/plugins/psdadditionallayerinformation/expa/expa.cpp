// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationExpaPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "expa.json")
public:
    // Exposure
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length <= 3);
        });

        const auto version = readU16(source, &length);
        Q_ASSERT(version == 1);
        const auto exposure = readFloat(source, &length);
        const auto offset = readFloat(source, &length);
        const auto gamma = readFloat(source, &length);

        QVariantMap result;
        result.insert(u"exposure"_s, static_cast<double>(exposure));
        result.insert(u"offset"_s, static_cast<double>(offset));
        result.insert(u"gamma"_s, static_cast<double>(gamma));
        return result;
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        const auto map = data.toMap();
        writeU16(&io, 1); // version
        writeFloat(&io, static_cast<float>(map.value(u"exposure"_s).toDouble()));
        writeFloat(&io, static_cast<float>(map.value(u"offset"_s).toDouble()));
        writeFloat(&io, static_cast<float>(map.value(u"gamma"_s).toDouble()));
        return buf;
    }
};

QT_END_NAMESPACE

#include "expa.moc"
