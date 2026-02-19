// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDPLACEDLAYERDATA_H
#define QPSDPLACEDLAYERDATA_H

#include <QtPsdCore/qpsdsection.h>
#include <QtPsdCore/qpsddescriptor.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdPlacedLayerData : public QPsdSection
{
public:
    QPsdPlacedLayerData();
    QPsdPlacedLayerData(QIODevice *source, quint32 length);
    QPsdPlacedLayerData(const QPsdPlacedLayerData &other);
    QPsdPlacedLayerData &operator=(const QPsdPlacedLayerData &other);
    void swap(QPsdPlacedLayerData &other) noexcept { d.swap(other.d); }
    ~QPsdPlacedLayerData() override;

    QPsdDescriptor descriptor() const;

    QByteArray rawData() const;
    void setRawData(const QByteArray &data);

    void write(QIODevice *dest) const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

Q_DECLARE_SHARED(QPsdPlacedLayerData)

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QPsdPlacedLayerData)
#endif // QPSDPLACEDLAYERDATA_H
