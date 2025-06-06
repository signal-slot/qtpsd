// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

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

        const auto brightness = readU16(source, &length);
        Q_UNUSED(brightness);
        const auto contrast = readU16(source, &length);
        Q_UNUSED(contrast);        
        const auto mean = readU16(source, &length);
        Q_UNUSED(mean);
        const auto lab = readU8(source, &length);
        Q_UNUSED(lab);
        // padding
        skip(source, 1, &length);

        return QVariant{};
    }
};

QT_END_NAMESPACE

#include "brit.moc"
