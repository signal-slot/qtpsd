// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsdeffectslayer.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationLrFXPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "lrfx.json")
public:
    // Effects Layer info
    QVariant parse(QIODevice *source , quint32 length) const override {
        // Save raw bytes for lossless round-trip
        const qint64 startPos = source->pos();
        const quint32 totalLength = length;
        QPsdEffectsLayer ret(source, &length);
        if (length > 0) {
            // Keep parser forward-progress even if some effect payload is not decoded.
            skip(source, length, &length);
        }
        const qint64 endPos = source->pos();
        // Read raw bytes for round-trip
        source->seek(startPos);
        ret.setRawData(source->read(totalLength));
        source->seek(endPos);
        return QVariant::fromValue(ret);
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        data.value<QPsdEffectsLayer>().write(&io);
        return buf;
    }
};

QT_END_NAMESPACE

#include "lrfx.moc"
