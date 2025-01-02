// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDIMAGESTORE_H
#define QPSDIMAGESTORE_H

#include <QtPsdGui/qpsdguiglobal.h>

#include <QtCore/QDir>

QT_BEGIN_NAMESPACE

class Q_PSDGUI_EXPORT QPsdImageStore
{
public:
    QPsdImageStore(const QDir &dir = {}, const QString &path = {});
    QPsdImageStore(const QPsdImageStore &other);
    ~QPsdImageStore();

    QPsdImageStore &operator=(const QPsdImageStore &other);

    QString save(const QString &filename, const QImage &image, const char *format);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDIMAGESTORE_H
