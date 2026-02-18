// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsdmetadataitem.h>

#include <QtCore/QBuffer>

QT_BEGIN_NAMESPACE

class QPsdAdditionalLayerInformationShmdPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "shmd.json")
public:
    // Metadata setting
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            Q_ASSERT(length == 0);
        });
        // Count of metadata items to follow
        auto count = readU32(source, &length);

        QVariantList ret;
        while (count-- > 0) {
            QPsdMetadataItem item(source, &length);
            ret.append(QVariant::fromValue(item));
        }
        return ret;
    }

    QByteArray serialize(const QVariant &data) const override {
        QByteArray buf;
        QBuffer io(&buf);
        io.open(QIODevice::WriteOnly);
        const auto list = data.toList();
        writeU32(&io, list.size());
        for (const auto &item : list) {
            item.value<QPsdMetadataItem>().write(&io);
        }
        return buf;
    }
};

QT_END_NAMESPACE

#include "shmd.moc"
