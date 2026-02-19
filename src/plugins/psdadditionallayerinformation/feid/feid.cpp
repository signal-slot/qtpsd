// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationFeidPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "feid.json")
public:
    // Filter Effect
    QVariant parse(QIODevice *source , quint32 length) const override {
        // Save raw bytes for lossless round-trip
        const QByteArray rawData = source->read(length);
        length = 0;
        return rawData;
    }
};

QT_END_NAMESPACE

#include "feid.moc"
