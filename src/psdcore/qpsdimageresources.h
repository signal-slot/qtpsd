// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDIMAGERESOURCES_H
#define QPSDIMAGERESOURCES_H

#include <QtPsdCore/qpsdsection.h>
#include <QtPsdCore/qpsdimageresourceblock.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdImageResources : public QPsdSection
{
public:
    QPsdImageResources();
    QPsdImageResources(QIODevice *source);
    QPsdImageResources(const QPsdImageResources &other);
    QPsdImageResources &operator=(const QPsdImageResources &other);
    ~QPsdImageResources() override;

    QList<QPsdImageResourceBlock> imageResourceBlocks() const;
    void setImageResourceBlocks(const QList<QPsdImageResourceBlock> &blocks);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDIMAGERESOURCES_H
