// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDLAYERANDMASKINFORMATION_H
#define QPSDLAYERANDMASKINFORMATION_H

#include <QtPsdCore/qpsdsection.h>
#include <QtPsdCore/qpsdlayerinfo.h>
#include <QtPsdCore/qpsdgloballayermaskinfo.h>
#include <QtPsdCore/qpsdadditionallayerinformation.h>
#include <QtPsdCore/qpsdfileheader.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdLayerAndMaskInformation : public QPsdSection
{
public:
    QPsdLayerAndMaskInformation();
    QPsdLayerAndMaskInformation(QIODevice *source);
    QPsdLayerAndMaskInformation(const QPsdLayerAndMaskInformation &other);
    QPsdLayerAndMaskInformation &operator=(const QPsdLayerAndMaskInformation &other);
    ~QPsdLayerAndMaskInformation() override;

    QPsdLayerInfo layerInfo() const;
    QPsdGlobalLayerMaskInfo globalLayerMaskInfo() const;
    QHash<QByteArray, QVariant> additionalLayerInformation() const;

    void setFileHeader(const QPsdFileHeader &fileHeader);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDLAYERANDMASKINFORMATION_H
