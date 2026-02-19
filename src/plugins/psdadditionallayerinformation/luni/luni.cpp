// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

#include <QtCore/QBuffer>
#include <QtCore/QStringEncoder>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationLuniPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "luni.json")
public:
    // Unicode Layer name
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            if (length == 2)
                skip(source, 2, &length); // a two byte null for the end of the string.
            Q_ASSERT(length == 0);
        });

        return readString(source, &length);
    }

    QByteArray serialize(const QVariant &data) const override {
        // luni format: S32(charCount) + UTF-16BE chars, padded to 4-byte boundary
        // charCount = number of actual characters (no null terminator counted)
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        const QString str = data.toString();
        QStringEncoder encoder(QStringEncoder::Utf16BE);
        QByteArray encoded = encoder.encode(str);
        writeS32(&io, encoded.size() / 2);
        io.write(encoded);
        // Pad to 4-byte boundary (trailing null bytes)
        int remainder = buf.size() % 4;
        if (remainder != 0)
            io.write(QByteArray(4 - remainder, '\0'));
        return buf;
    }
};

QT_END_NAMESPACE

#include "luni.moc"
