// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsddescriptor.h>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationClrlPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "clrl.json")
public:
    // Color Lookup (Photoshop CS6)
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            // Q_ASSERT(length == 0);
        });
        // Version ( = 1)
        auto version1 = readU16(source, &length);
        Q_ASSERT(version1 == 1);
        // Version ( = 16 for Photoshop 6.0)
        auto version = readU32(source, &length);
        Q_ASSERT(version == 16);
        QPsdDescriptor descriptor(source, &length);
        return QVariant::fromValue(descriptor);
    }
};

QT_END_NAMESPACE

#include "clrl.moc"
