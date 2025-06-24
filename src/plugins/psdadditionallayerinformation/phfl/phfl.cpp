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

        if (version == 2) {
            const auto colorSpace = readColorSpace(source, &length);
            const auto color = colorSpace.toString();
            Q_UNUSED(color); // TODO: Store photo filter color when Phfl structure is implemented
        } else {
            const auto l = readS32(source, &length);
            const auto a = readS32(source, &length);
            const auto b = readS32(source, &length);
            Q_UNUSED(l);
            Q_UNUSED(a);
            Q_UNUSED(b);
        }
        
        const auto density = readS32(source, &length);
        Q_UNUSED(density);
        const auto preserveLuminosity = readU8(source, &length);
        Q_UNUSED(preserveLuminosity);

        return {};
    }
};

QT_END_NAMESPACE

#include "phfl.moc"
