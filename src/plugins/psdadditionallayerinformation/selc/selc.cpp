// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationSelcPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "selc.json")
public:
    // Selective color
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length <= 3);
        });

       const auto version = readU16(source, &length);
        Q_ASSERT(version == 1);

        const auto mode = readU16(source, &length);

        skip(source, 8, &length);

        QVariantMap result;
        result.insert(u"mode"_s, mode == 0 ? u"relative"_s : u"absolute"_s);
        result.insert(u"reds"_s, readSelectiveColors(source, &length));
        result.insert(u"yellows"_s, readSelectiveColors(source, &length));
        result.insert(u"greens"_s, readSelectiveColors(source, &length));
        result.insert(u"cyans"_s, readSelectiveColors(source, &length));
        result.insert(u"blues"_s, readSelectiveColors(source, &length));
        result.insert(u"magentas"_s, readSelectiveColors(source, &length));
        result.insert(u"whites"_s, readSelectiveColors(source, &length));
        result.insert(u"neutrals"_s, readSelectiveColors(source, &length));
        result.insert(u"blacks"_s, readSelectiveColors(source, &length));

        return result;
    }

    QVariant readSelectiveColors(QIODevice *source, quint32 *length) const {
        const auto c = readS16(source, length);
        const auto m = readS16(source, length);
        const auto y = readS16(source, length);
        const auto k = readS16(source, length);

        QVariantMap result;
        result.insert(u"c"_s, c);
        result.insert(u"m"_s, m);
        result.insert(u"y"_s, y);
        result.insert(u"k"_s, k);
        return result;
    }
};

QT_END_NAMESPACE

#include "selc.moc"
