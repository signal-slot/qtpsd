// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDIMAGERESOURCEBLOCK_H
#define QPSDIMAGERESOURCEBLOCK_H

#include <QtPsdCore/qpsdsection.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdImageResourceBlock : public QPsdSection
{
public:
    QPsdImageResourceBlock();
    QPsdImageResourceBlock(QIODevice *source);
    QPsdImageResourceBlock(const QPsdImageResourceBlock &other);
    QPsdImageResourceBlock &operator=(const QPsdImageResourceBlock &other);
    ~QPsdImageResourceBlock() override;

    quint16 id() const;
    QByteArray name() const;
    QByteArray data() const;

    void setId(quint16 id);
    void setName(const QByteArray &name);
    void setData(const QByteArray &data);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDIMAGERESOURCEBLOCK_H
