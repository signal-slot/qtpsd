// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationU32Plugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "u32.json")
public:
    // Layer ID
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length == 0);
        });
        return readU32(source, &length);
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

#include "u32.moc"
