// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDCHANNELIMAGEDATA_H
#define QPSDCHANNELIMAGEDATA_H

#include <QtPsdCore/qpsdabstractimage.h>
#include <QtPsdCore/qpsdchannelinfo.h>
#include <QtPsdCore/qpsdfileheader.h>

QT_BEGIN_NAMESPACE

class QPsdLayerRecord;

class Q_PSDCORE_EXPORT QPsdChannelImageData : public QPsdAbstractImage
{
public:
    QPsdChannelImageData();
    QPsdChannelImageData(const QPsdLayerRecord &record, QIODevice *source);
    QPsdChannelImageData(const QPsdChannelImageData &other);
    QPsdChannelImageData &operator=(const QPsdChannelImageData &other);
    ~QPsdChannelImageData() override;
    void swap(QPsdChannelImageData &other) noexcept { d.swap(other.d); }

    QByteArray imageData() const override;
    bool hasAlpha() const override { return a(); }
    QByteArray transparencyMaskData() const;
    QByteArray userSuppliedLayerMask() const;

    QByteArray channelData(QPsdChannelInfo::ChannelID channelId) const;
    void setChannelData(QPsdChannelInfo::ChannelID channelId, const QByteArray &data);

    Compression channelCompression(QPsdChannelInfo::ChannelID channelId) const;
    void setChannelCompression(QPsdChannelInfo::ChannelID channelId, Compression compression);

#ifdef QT_PSD_RAW_ROUND_TRIP
    QByteArray rawChannelBytes(QPsdChannelInfo::ChannelID channelId) const;
    void setRawChannelBytes(QPsdChannelInfo::ChannelID channelId, const QByteArray &data);
#endif

protected:
    const unsigned char *gray() const override;
    const unsigned char *r() const override;
    const unsigned char *g() const override;
    const unsigned char *b() const override;
    const unsigned char *a() const override;
    const unsigned char *c() const override;
    const unsigned char *m() const override;
    const unsigned char *y() const override;
    const unsigned char *k() const override;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

Q_DECLARE_SHARED(QPsdChannelImageData)

QT_END_NAMESPACE

#endif // QPSDCHANNELIMAGEDATA_H
