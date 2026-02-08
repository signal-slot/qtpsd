// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationHue2Plugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "hue2.json")
public:
    // New Hue/saturation, Photoshop 5.0
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length <= 3);
        });

       const auto version = readU16(source, &length);
        Q_ASSERT(version == 2);

        const auto master = readHue(source, &length);
        const auto reds = readHue(source, &length);
        const auto yellows = readHue(source, &length);
        const auto greens = readHue(source, &length);
        const auto cyans = readHue(source, &length);
        const auto blues = readHue(source, &length);
        const auto magentas = readHue(source, &length);

        QVariantMap result;
        result.insert(u"master"_s, master);
        result.insert(u"reds"_s, reds);
        result.insert(u"yellows"_s, yellows);
        result.insert(u"greens"_s, greens);
        result.insert(u"cyans"_s, cyans);
        result.insert(u"blues"_s, blues);
        result.insert(u"magentas"_s, magentas);
        return result;
    }

    QVariant readHue(QIODevice *source, quint32 *length) const {
        const auto a = readS16(source, length);
        const auto b = readS16(source, length);
        const auto c = readS16(source, length);
        const auto d = readS16(source, length);
        const auto hue = readS16(source, length);
        const auto saturation = readS16(source, length);
        const auto lightness = readS16(source, length);

        QVariantMap result;
        result.insert(u"a"_s, a);
        result.insert(u"b"_s, b);
        result.insert(u"c"_s, c);
        result.insert(u"d"_s, d);
        result.insert(u"hue"_s, hue);
        result.insert(u"saturation"_s, saturation);
        result.insert(u"lightness"_s, lightness);
        return result;
    }
};

QT_END_NAMESPACE

#include "hue2.moc"
