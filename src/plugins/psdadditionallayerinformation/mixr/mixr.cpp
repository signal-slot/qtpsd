// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationMixrPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "mixr.json")
public:
    // Channel Mixer
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length <= 3);
        });

        const auto monochrome = readU16(source, &length) != 0;

        QVariantMap result;
        result.insert(u"monochrome"_s, monochrome);

        if (!monochrome) {
            result.insert(u"red"_s, readMixer(source, &length));
            result.insert(u"green"_s, readMixer(source, &length));
            result.insert(u"blue"_s, readMixer(source, &length));
        }
        result.insert(u"gray"_s, readMixer(source, &length));

        if (monochrome) {
            skip(source, 3 * 5 * 2, &length);
        }

        return result;
    }

    QVariant readMixer(QIODevice *source, quint32 *length) const {
        const auto red = readS16(source, length);
        const auto green = readS16(source, length);
        const auto blue = readS16(source, length);

        const auto v1 = readU16(source, length);
        Q_UNUSED(v1);

        const auto constant = readS16(source, length);

        QVariantMap result;
        result.insert(u"red"_s, red);
        result.insert(u"green"_s, green);
        result.insert(u"blue"_s, blue);
        result.insert(u"constant"_s, constant);
        return result;
    }
};

QT_END_NAMESPACE

#include "mixr.moc"
