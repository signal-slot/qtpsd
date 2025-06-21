// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDABSTRACTIMAGE_H
#define QPSDABSTRACTIMAGE_H

#include <QtPsdCore/qpsdsection.h>
#include <QtPsdCore/qpsdfileheader.h>

QT_BEGIN_NAMESPACE

class QPsdFileHeader;

class Q_PSDCORE_EXPORT QPsdAbstractImage : public QPsdSection
{
public:
    QPsdAbstractImage();
    QPsdAbstractImage(const QPsdAbstractImage &other);
    QPsdAbstractImage &operator=(const QPsdAbstractImage &other);
    void swap(QPsdAbstractImage &other) noexcept { d.swap(other.d); }
    ~QPsdAbstractImage() override;

    quint32 width() const;
    quint32 height() const;
    quint16 depth() const;
    quint8 opacity() const;

    QPsdFileHeader header() const;
    void setHeader(const QPsdFileHeader &header);

    virtual QByteArray imageData() const = 0;
    virtual bool hasAlpha() const { return false; }
    QByteArray toImage(QPsdFileHeader::ColorMode colorMode) const;

protected:
    void setWidth(quint32 width);
    void setHeight(quint32 height);
    void setOpacity(quint8 opacity);

    virtual const unsigned char *gray() const = 0;
    virtual const unsigned char *r() const = 0;
    virtual const unsigned char *g() const = 0;
    virtual const unsigned char *b() const = 0;
    virtual const unsigned char *a() const { return nullptr; }
    virtual const unsigned char *c() const { return nullptr; }
    virtual const unsigned char *m() const { return nullptr; }
    virtual const unsigned char *y() const { return nullptr; }
    virtual const unsigned char *k() const { return nullptr; }

    enum Compression {
        RawData = 0,
        RLE = 1,
        ZipWithoutPrediction = 2,
        ZipWithPrediction = 3,
    };
    static QByteArray readRLE(QIODevice *source, int height, quint32 *length);
    static QByteArray readZip(QIODevice *source, quint32 *length);

private:
    class Private;
public:
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDABSTRACTIMAGE_H
