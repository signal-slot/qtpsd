// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdlayerandmaskinformation.h"
#include "qpsdfileheader.h"

QT_BEGIN_NAMESPACE

class QPsdLayerAndMaskInformation::Private : public QSharedData
{
public:
    Private();
    QPsdLayerInfo layerInfo;
    QPsdGlobalLayerMaskInfo globalLayerMaskInfo;
    QHash<QByteArray, QVariant> additionalLayerInformation;
};

QPsdLayerAndMaskInformation::Private::Private()
{}

QPsdLayerAndMaskInformation::QPsdLayerAndMaskInformation()
    : QPsdSection()
    , d(new Private)
{}

QPsdLayerAndMaskInformation::QPsdLayerAndMaskInformation(QIODevice *source)
    : QPsdLayerAndMaskInformation()
{
    // Layer and Mask Information Section
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_75067

    // Length of the layer and mask information section. (**PSB** length is 8 bytes.)
    auto length = readU32(source);
    EnsureSeek es(source, length);

    if (length == 0) {
        return;
    }
    d->layerInfo = QPsdLayerInfo(source);
    d->globalLayerMaskInfo = QPsdGlobalLayerMaskInfo(source);

    while (es.bytesAvailable() > 12) {
        QPsdAdditionalLayerInformation ali(source, 4);
        d->additionalLayerInformation.insert(ali.key(), ali.data());
    }
}

QPsdLayerAndMaskInformation::QPsdLayerAndMaskInformation(const QPsdLayerAndMaskInformation &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdLayerAndMaskInformation &QPsdLayerAndMaskInformation::operator=(const QPsdLayerAndMaskInformation &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdLayerAndMaskInformation::~QPsdLayerAndMaskInformation() = default;

QPsdLayerInfo QPsdLayerAndMaskInformation::layerInfo() const
{
    return d->layerInfo;
}

QPsdGlobalLayerMaskInfo QPsdLayerAndMaskInformation::globalLayerMaskInfo() const
{
    return d->globalLayerMaskInfo;
}

QHash<QByteArray, QVariant> QPsdLayerAndMaskInformation::additionalLayerInformation() const
{
    return d->additionalLayerInformation;
}

void QPsdLayerAndMaskInformation::setFileHeader(const QPsdFileHeader &fileHeader)
{
    d->layerInfo.setFileHeader(fileHeader);
}

QT_END_NAMESPACE
