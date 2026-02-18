// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsdvectorstrokecontentsetting.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationVscgPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "vscg.json")
public:
    QVariant parse(QIODevice *source , quint32 length) const override {
        return QVariant::fromValue(QPsdVectorStrokeContentSetting(source, length));
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        data.value<QPsdVectorStrokeContentSetting>().write(&io);
        return buf;
    }
};

QT_END_NAMESPACE

#include "vscg.moc"
