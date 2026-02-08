// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsdcolorspace.h>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationPhflPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "phfl.json")
public:
    // Photo Filter
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length <= 3);
        });

        const auto version = readU16(source, &length);
        Q_ASSERT(version == 2 || version == 3);

        QVariantMap result;

        if (version == 2) {
            const auto colorSpace = readColorSpace(source, &length);
            result.insert(u"color"_s, colorSpace.toString());
        } else {
            const auto l = readS32(source, &length);
            const auto a = readS32(source, &length);
            const auto b = readS32(source, &length);
            QVariantMap lab;
            lab.insert(u"l"_s, l);
            lab.insert(u"a"_s, a);
            lab.insert(u"b"_s, b);
            result.insert(u"labColor"_s, lab);
        }

        const auto density = readS32(source, &length);
        const auto preserveLuminosity = readU8(source, &length);

        result.insert(u"density"_s, density);
        result.insert(u"preserveLuminosity"_s, preserveLuminosity != 0);
        return result;
    }
};

QT_END_NAMESPACE

#include "phfl.moc"
