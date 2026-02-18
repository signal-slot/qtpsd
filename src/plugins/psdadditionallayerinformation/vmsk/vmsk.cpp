// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsdvectormasksetting.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationVmskPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "vmsk.json")
public:
    // Vector mask setting
    QVariant parse(QIODevice *source , quint32 length) const override {
        return QVariant::fromValue(QPsdVectorMaskSetting(source, length));
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        data.value<QPsdVectorMaskSetting>().write(&io);
        return buf;
    }
};

QT_END_NAMESPACE

#include "vmsk.moc"
