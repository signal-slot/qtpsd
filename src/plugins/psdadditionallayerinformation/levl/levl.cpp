// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsddescriptor.h>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationLevlPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "levl.json")
public:
    // Levels
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length == 0);
        });

        Q_ASSERT(length == 0x0278);

        const auto rgb = readLevels(source, &length);
        const auto red = readLevels(source, &length);
        const auto green = readLevels(source, &length);
        const auto blue = readLevels(source, &length);

        // has many unknown data.. (592 bytes)
        skip(source, length, &length);

        QVariantMap result;
        result.insert(u"rgb"_s, rgb);
        result.insert(u"red"_s, red);
        result.insert(u"green"_s, green);
        result.insert(u"blue"_s, blue);
        return result;
    }

    QVariant readLevels(QIODevice *source, quint32 *length) const {
        const auto shadowInput = readU16(source, length);
        const auto highlightInput = readU16(source, length);
        const auto shadowOutput = readU16(source, length);
        const auto highlightOutput = readU16(source, length);
        const auto midtoneInput = readU16(source, length);

        QVariantMap result;
        result.insert(u"shadowInput"_s, shadowInput);
        result.insert(u"highlightInput"_s, highlightInput);
        result.insert(u"shadowOutput"_s, shadowOutput);
        result.insert(u"highlightOutput"_s, highlightOutput);
        result.insert(u"midtoneInput"_s, midtoneInput);
        return result;
    }
};

QT_END_NAMESPACE

#include "levl.moc"
