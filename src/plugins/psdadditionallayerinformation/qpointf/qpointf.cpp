// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

#include <QtCore/QBuffer>
#include <QtCore/QPointF>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationQpointFPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "qpointf.json")
public:
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length == 0);
        });
        // 2 double values for the reference point
        const auto x = readDouble(source, &length);
        const auto y = readDouble(source, &length);
        return QPointF(x, y);
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        const auto pt = data.toPointF();
        writeDouble(&io, pt.x());
        writeDouble(&io, pt.y());
        return buf;
    }
};

QT_END_NAMESPACE

#include "qpointf.moc"
