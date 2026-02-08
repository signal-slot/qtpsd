// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationBlncPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "blnc.json")
public:
    // Color Balance
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length <= 3);
        });

        const auto shadows = readBalance(source, &length);
        const auto midtones = readBalance(source, &length);
        const auto highlights = readBalance(source, &length);
        const auto preserveLuminosity = readU8(source, &length);

        QVariantMap result;
        result.insert(u"shadows"_s, shadows);
        result.insert(u"midtones"_s, midtones);
        result.insert(u"highlights"_s, highlights);
        result.insert(u"preserveLuminosity"_s, preserveLuminosity != 0);
        return result;
    }

    QVariant readBalance(QIODevice *source, quint32 *length) const {
        const auto cyanRed = readS16(source, length);
        const auto magentaGreen = readS16(source, length);
        const auto yellowBlue = readS16(source, length);

        QVariantMap result;
        result.insert(u"cyanRed"_s, cyanRed);
        result.insert(u"magentaGreen"_s, magentaGreen);
        result.insert(u"yellowBlue"_s, yellowBlue);
        return result;
    }
};

QT_END_NAMESPACE

#include "blnc.moc"
