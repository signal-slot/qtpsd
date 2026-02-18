// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdplacedlayer.h"
#include "qpsddescriptor.h"

#include <QtCore/QLoggingCategory>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdPlacedLayer, "qt.psdcore.placedlayer")

class QPsdPlacedLayer::Private : public QSharedData
{
public:
    QByteArray uniqueID;
    quint32 pageNumber = 1;
    quint32 totalPages = 1;
    quint32 antiAliasPolicy = 16;
    quint32 placedLayerType = 0;
    double transform[8] = {};
    QPsdDescriptor warpDescriptor;
};

QPsdPlacedLayer::QPsdPlacedLayer()
    : QPsdSection()
    , d(new Private)
{}

QPsdPlacedLayer::QPsdPlacedLayer(QIODevice *source, quint32 length)
    : QPsdPlacedLayer()
{
    // Placed Layer (replaced by SoLd in Photoshop CS3)
    auto cleanup = qScopeGuard([&] {
        // Q_ASSERT(length == 0);
    });

    // Type ( = 'plcL' )
    const auto type = readByteArray(source, 4, &length);
    Q_ASSERT(type == "plcL");

    // Version ( = 3)
    const auto version = readU32(source, &length);
    Q_ASSERT(version == 3);

    // Unique ID as a pascal string
    d->uniqueID = readPascalString(source, 1, &length);
    qCDebug(lcQPsdPlacedLayer) << "uniqueID" << d->uniqueID;

    //Page number
    d->pageNumber = readU32(source, &length);
    qCDebug(lcQPsdPlacedLayer) << "pageNumber" << d->pageNumber;

    // Total pages
    d->totalPages = readU32(source, &length);
    qCDebug(lcQPsdPlacedLayer) << "totalPages" << d->totalPages;

    // Anit alias policy
    d->antiAliasPolicy = readU32(source, &length);
    qCDebug(lcQPsdPlacedLayer) << "antiAliasPolicy" << d->antiAliasPolicy;

    // Placed layer type: 0 = unknown, 1 = vector, 2 = raster, 3 = image stack
    d->placedLayerType = readU32(source, &length);
    qCDebug(lcQPsdPlacedLayer) << "placedLayerType" << d->placedLayerType;

    //Transformation: 8 doubles for x,y location of transform points
    d->transform[0] = readDouble(source, &length);
    d->transform[1] = readDouble(source, &length);
    d->transform[2] = readDouble(source, &length);
    d->transform[3] = readDouble(source, &length);
    d->transform[4] = readDouble(source, &length);
    d->transform[5] = readDouble(source, &length);
    d->transform[6] = readDouble(source, &length);
    d->transform[7] = readDouble(source, &length);

    // Warp version ( = 0 )
    auto warpVersion = readU32(source, &length);
    qCDebug(lcQPsdPlacedLayer) << warpVersion;
    Q_ASSERT(warpVersion == 0);

    //Warp descriptor version ( = 16 )
    auto warpDescriptorVersion = readU32(source, &length);
    Q_ASSERT(warpDescriptorVersion == 16);

    // Descriptor for warping information
    d->warpDescriptor = QPsdDescriptor(source, &length);
}

QPsdPlacedLayer::QPsdPlacedLayer(const QPsdPlacedLayer &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdPlacedLayer &QPsdPlacedLayer::operator=(const QPsdPlacedLayer &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdPlacedLayer::~QPsdPlacedLayer() = default;

QByteArray QPsdPlacedLayer::uniqueId() const
{
    return d->uniqueID;
}

void QPsdPlacedLayer::write(QIODevice *dest) const
{
    dest->write("plcL", 4);
    writeU32(dest, 3); // version
    writePascalString(dest, d->uniqueID, 1);
    writeU32(dest, d->pageNumber);
    writeU32(dest, d->totalPages);
    writeU32(dest, d->antiAliasPolicy);
    writeU32(dest, d->placedLayerType);
    for (int i = 0; i < 8; ++i)
        writeDouble(dest, d->transform[i]);
    writeU32(dest, 0);  // warp version
    writeU32(dest, 16); // warp descriptor version
    d->warpDescriptor.write(dest);
}

QT_END_NAMESPACE
