// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationLclrPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "lclr.json")
public:
    // Sheet Color setting
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length == 0);
        });
        static const QStringList colors = {
            "transparent", // default
            "#ff0000", // red
            "#ff7f00", // orange
            "#ffff00", // yellow
            "#00ff00", // green
            "#0000ff", // blue
            "#ee82ee", // violet
            "#808080", // gray
            "#9fe2bf", // sea form green
            "#4b0082", // indigo
            "#ff00ff", // magenda
            "#951d61", // fucsia
        };
        // Color. Only the first color setting is used for Photoshop 6.0; the rest are zeros
        quint16 colorIndex = readU16(source, &length);
        Q_ASSERT(colorIndex < colors.size());
        skip(source, 6, &length);
        // return colorIndex;
        return colors.at(colorIndex);
    }

    QByteArray serialize(const QVariant &data) const override {
        static const QStringList colors = {
            "transparent", "#ff0000", "#ff7f00", "#ffff00",
            "#00ff00", "#0000ff", "#ee82ee", "#808080",
            "#9fe2bf", "#4b0082", "#ff00ff", "#951d61",
        };
        const auto colorStr = data.toString();
        quint16 colorIndex = 0;
        int idx = colors.indexOf(colorStr);
        if (idx >= 0)
            colorIndex = static_cast<quint16>(idx);
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        writeU16(&io, colorIndex);
        io.write(QByteArray(6, '\0'));
        return buf;
    }
};

QT_END_NAMESPACE

#include "lclr.moc"
