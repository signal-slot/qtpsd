// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsdlinkedlayer.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationLnk_Plugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "lnk_.json")
public:
    // Linked Layer
    QVariant parse(QIODevice *source , quint32 length) const override {
        return QVariant::fromValue(QPsdLinkedLayer(source, length));
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        data.value<QPsdLinkedLayer>().write(&io);
        return buf;
    }
};

QT_END_NAMESPACE

#include "lnk_.moc"
