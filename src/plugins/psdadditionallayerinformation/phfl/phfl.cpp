// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsdcolorspace.h>

#include <QtCore/QBuffer>

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
            // Preserve raw color space data for round-trip
            QByteArray colorBytes;
            {
                QBuffer colorBuf(&colorBytes);
                colorBuf.open(QIODevice::WriteOnly);
                writeColorSpace(&colorBuf, colorSpace);
            }
            result.insert(u"colorBytes"_s, colorBytes);
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

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        const auto map = data.toMap();
        if (map.contains(u"labColor"_s)) {
            writeU16(&io, 3); // version 3
            const auto lab = map.value(u"labColor"_s).toMap();
            writeS32(&io, lab.value(u"l"_s).toInt());
            writeS32(&io, lab.value(u"a"_s).toInt());
            writeS32(&io, lab.value(u"b"_s).toInt());
        } else {
            writeU16(&io, 2);
            const auto colorBytes = map.value(u"colorBytes"_s).toByteArray();
            if (!colorBytes.isEmpty()) {
                io.write(colorBytes);
            } else {
                io.write(QByteArray(10, '\0'));
            }
        }
        writeS32(&io, map.value(u"density"_s).toInt());
        writeU8(&io, map.value(u"preserveLuminosity"_s).toBool() ? 1 : 0);
        return buf;
    }
};

QT_END_NAMESPACE

#include "phfl.moc"
