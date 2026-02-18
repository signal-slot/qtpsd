// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationU8Plugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "u8.json")
public:
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length == 0);
        });
        QVariant ret = readU8(source, &length);
        // Padding
        skip(source, 3, &length);
        return ret;
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf(4, '\0');
        buf[0] = static_cast<char>(data.value<quint8>());
        return buf;
    }
};

QT_END_NAMESPACE

#include "u8.moc"
