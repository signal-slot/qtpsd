// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDIMAGEDATA_H
#define QPSDIMAGEDATA_H

#include <QtPsdCore/qpsdabstractimage.h>

QT_BEGIN_NAMESPACE

class QPsdFileHeader;

class Q_PSDCORE_EXPORT QPsdImageData : public QPsdAbstractImage
{
public:
    QPsdImageData();
    QPsdImageData(const QPsdFileHeader &header, QIODevice *source);
    QPsdImageData(const QPsdImageData &other);
    QPsdImageData &operator=(const QPsdImageData &other);
    ~QPsdImageData() override;
    void swap(QPsdImageData &other) noexcept { d.swap(other.d); }

    QByteArray imageData() const override;
    bool hasAlpha() const override;

protected:
    const unsigned char *a() const override;
    const unsigned char *gray() const override;
    const unsigned char *r() const override;
    const unsigned char *g() const override;
    const unsigned char *b() const override;
    const unsigned char *c() const override;
    const unsigned char *m() const override;
    const unsigned char *y() const override;
    const unsigned char *k() const override;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

Q_DECLARE_SHARED(QPsdImageData)

QT_END_NAMESPACE

#endif // QPSDIMAGEDATA_H
