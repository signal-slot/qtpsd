// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsddescriptor.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationV16DescriptorPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "v16descriptor.json")
public:
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            // Q_ASSERT(length == 0);
        });
        // Version ( = 16 for Photoshop 6.0)
        auto version = readU32(source, &length);
        Q_ASSERT(version == 16);
        QPsdDescriptor descriptor(source, &length);
        return QVariant::fromValue(descriptor);
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        writeU32(&io, 16);
        data.value<QPsdDescriptor>().write(&io);
        return buf;
    }
};

QT_END_NAMESPACE

#include "v16descriptor.moc"
