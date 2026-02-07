// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDLAYERINFO_H
#define QPSDLAYERINFO_H

#include <QtPsdCore/qpsdsection.h>
#include <QtPsdCore/qpsdlayerrecord.h>
#include <QtPsdCore/qpsdchannelimagedata.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdLayerInfo : public QPsdSection
{
public:
    QPsdLayerInfo();
    QPsdLayerInfo(QIODevice *source);
    QPsdLayerInfo(QIODevice *source, quint32 length);
    QPsdLayerInfo(const QPsdLayerInfo &other);
    QPsdLayerInfo &operator=(const QPsdLayerInfo &other);
    ~QPsdLayerInfo() override;

    QList<QPsdLayerRecord> records() const;
    QList<QPsdChannelImageData> channelImageData() const;

    void setFileHeader(const QPsdFileHeader &fileHeader);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDLAYERINFO_H
