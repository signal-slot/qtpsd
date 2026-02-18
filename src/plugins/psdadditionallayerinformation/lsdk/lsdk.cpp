// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsdblend.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationLsdkPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "lsdk.json")
public:
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length == 0);
        });

        QVariant ret;
        const auto code = readU32(source, &length);
        switch (code) {
        case 1:
        case 2:
            ret = code;
            break;
        case 3:
            ret = code;
            break;
        default:
            qFatal("Unknown lsdk code %d", code);
        }
        if (length > 0) {
            const auto signature = readByteArray(source, 4, &length);
            Q_ASSERT(signature == "8BIM");

            const auto key = readByteArray(source, 4, &length);
            QPsdBlend::Mode mode = QPsdBlend::from(key);
            Q_ASSERT(mode != QPsdBlend::Invalid);

            skip(source, 4, &length);
        }

        return ret;
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        writeU32(&io, data.value<quint32>());
        return buf;
    }
};

QT_END_NAMESPACE

#include "lsdk.moc"
