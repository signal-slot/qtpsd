// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDADDITIONALLAYERINFORMATIONPLUGIN_H
#define QPSDADDITIONALLAYERINFORMATIONPLUGIN_H

#include <QtPsdCore/qpsdabstractplugin.h>

QT_BEGIN_NAMESPACE

#define QPsdAdditionalLayerInformationFactoryInterface_iid "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface"

class Q_PSDCORE_EXPORT QPsdAdditionalLayerInformationPlugin : public QPsdAbstractPlugin
{
    Q_OBJECT
public:
    explicit QPsdAdditionalLayerInformationPlugin(QObject *parent = nullptr);

    virtual QVariant parse(QIODevice *source , quint32 length) const = 0;
    virtual QByteArray serialize(const QVariant &data) const { Q_UNUSED(data); return {}; }

    static QByteArrayList keys() {
        return QPsdAbstractPlugin::keys<QPsdAdditionalLayerInformationPlugin>(QPsdAdditionalLayerInformationFactoryInterface_iid, "psdadditionallayerinformation");
    }
    static QPsdAdditionalLayerInformationPlugin *plugin(const QByteArray &key) {
        return QPsdAbstractPlugin::plugin<QPsdAdditionalLayerInformationPlugin>(QPsdAdditionalLayerInformationFactoryInterface_iid, "psdadditionallayerinformation", key);
    }
};

QT_END_NAMESPACE

#endif // QPSDADDITIONALLAYERINFORMATIONPLUGIN_H
