// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDLAYERRECORD_H
#define QPSDLAYERRECORD_H

#include <QtPsdCore/qpsdsection.h>
#include <QtPsdCore/qpsdchannelinfo.h>
#include <QtPsdCore/qpsdlayermaskadjustmentlayerdata.h>
#include <QtPsdCore/qpsdlayerblendingrangesdata.h>
#include <QtPsdCore/qpsdchannelimagedata.h>
#include <QtPsdCore/qpsdblend.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdLayerRecord : public QPsdSection
{
    friend class QPsdLayerInfo;
public:
    enum Clipping {
        Base,
        NonBase,
    };

    QPsdLayerRecord();
    QPsdLayerRecord(QIODevice *source);
    QPsdLayerRecord(const QPsdLayerRecord &other);
    QPsdLayerRecord &operator=(const QPsdLayerRecord &other);
    ~QPsdLayerRecord() override;

    QRect rect() const;
    QList<QPsdChannelInfo> channelInfo() const;
    QPsdBlend::Mode blendMode() const;
    quint8 opacity() const;
    Clipping clipping() const;
    bool isTransparencyProtected() const;
    bool isVisible() const;
    bool hasPixelDataIrrelevantToAppearanceDocument() const;
    bool isPixelDataIrrelevantToAppearanceDocument() const;
    // quint8 flags() const;
    QPsdLayerMaskAdjustmentLayerData layerMaskAdjustmentLayerData() const;
    QPsdLayerBlendingRangesData layerBlendingRangesData() const;
    QByteArray name() const;
    QHash<QByteArray, QVariant> additionalLayerInformation() const;

    void setRect(const QRect &rect);
    void setChannelInfo(const QList<QPsdChannelInfo> &channelInfo);
    void setBlendMode(QPsdBlend::Mode blendMode);
    void setOpacity(quint8 opacity);
    void setClipping(Clipping clipping);
    void setFlags(quint8 flags);
    void setName(const QByteArray &name);
    void setAdditionalLayerInformation(const QHash<QByteArray, QVariant> &info);
    void setLayerMaskAdjustmentLayerData(const QPsdLayerMaskAdjustmentLayerData &data);
    void setLayerBlendingRangesData(const QPsdLayerBlendingRangesData &data);

    QPsdChannelImageData imageData() const;
    void setImageData(const QPsdChannelImageData &imageData);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(const QPsdLayerRecord*)

#endif // QPSDLAYERRECORD_H
