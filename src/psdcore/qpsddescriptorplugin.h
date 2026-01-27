// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDDESCRIPTORPLUGIN_H
#define QPSDDESCRIPTORPLUGIN_H

#include <QtPsdCore/qpsdcoreglobal.h>
#include <QtPsdCore/qpsdabstractplugin.h>

QT_BEGIN_NAMESPACE

#define QPsdDescriptorFactoryInterface_iid "org.qt-project.Qt.QPsdDescriptorFactoryInterface"

class Q_PSDCORE_EXPORT QPsdDescriptorPlugin : public QPsdAbstractPlugin
{
    Q_OBJECT
public:
    explicit QPsdDescriptorPlugin(QObject *parent = nullptr);

    virtual QVariant parse(QIODevice *source, quint32 *length) const = 0;

    static QByteArrayList keys() {
        return QPsdAbstractPlugin::keys<QPsdDescriptorPlugin>(QPsdDescriptorFactoryInterface_iid, "psddescriptor");
    }
    static QPsdDescriptorPlugin *plugin(const QByteArray &key) {
        return QPsdAbstractPlugin::plugin<QPsdDescriptorPlugin>(QPsdDescriptorFactoryInterface_iid, "psddescriptor", key);
    }
};

QT_END_NAMESPACE

#endif // QPSDDESCRIPTORPLUGIN_H
