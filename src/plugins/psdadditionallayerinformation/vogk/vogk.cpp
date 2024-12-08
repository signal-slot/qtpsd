// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsddescriptor.h>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationVogkPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "vogk.json")
public:
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            // Q_ASSERT(length == 0);
        });
        // Version ( = 1 for Photoshop CC)
        auto version = readU32(source, &length);
        Q_ASSERT(version == 1);

        // Version ( = 16 )
        version = readU32(source, &length);
        Q_ASSERT(version == 16);

        QPsdDescriptor descriptor(source, &length);
        return QVariant::fromValue(descriptor);
    }
};

QT_END_NAMESPACE

#include "vogk.moc"
