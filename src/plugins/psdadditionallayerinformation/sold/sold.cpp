// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsdplacedlayerdata.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationSoLdPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "sold.json")
public:
    // Placed Layer (replaced by SoLd in Photoshop CS3)
    QVariant parse(QIODevice *source , quint32 length) const override {
        // Save raw bytes for lossless round-trip
        const qint64 startPos = source->pos();
        const quint32 totalLength = length;
        QPsdPlacedLayerData ret(source, length);
        const qint64 endPos = source->pos();
        source->seek(startPos);
        ret.setRawData(source->read(totalLength));
        source->seek(endPos);
        return QVariant::fromValue(ret);
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        data.value<QPsdPlacedLayerData>().write(&io);
        return buf;
    }
};

QT_END_NAMESPACE

#include "sold.moc"
