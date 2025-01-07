// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdplacedlayer.h"
#include "qpsddescriptor.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/QSharedData>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdPlacedLayer, "qt.psdcore.placedlayer")

class QPsdPlacedLayer::Private : public QSharedData
{
public:
    Private() = default;
    Private(const Private &other);

    QByteArray uniqueID;
    QList<double> transform;
};

QPsdPlacedLayer::Private::Private(const Private &other)
    : QSharedData(other)
    , uniqueID(other.uniqueID)
    , transform(other.transform)
{}

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
    auto pageNumber = readU32(source, &length);
    qCDebug(lcQPsdPlacedLayer) << "pageNumber" << pageNumber;

    // Total pages
    auto totalPages = readU32(source, &length);
    qCDebug(lcQPsdPlacedLayer) << "totalPages" << totalPages;

    // Anit alias policy
    auto antiAliasPolicy = readU32(source, &length);
    qCDebug(lcQPsdPlacedLayer) << "antiAliasPolicy" << antiAliasPolicy;

    // Placed layer type: 0 = unknown, 1 = vector, 2 = raster, 3 = image stack
    auto placedLayerType = readU32(source, &length);
    qCDebug(lcQPsdPlacedLayer) << "placedLayerType" << placedLayerType;

    //Transformation: 8 doubles for x,y location of transform points
    d->transform.reserve(8);
    d->transform.append(readDouble(source, &length)); // transform1
    d->transform.append(readDouble(source, &length)); // transform2
    d->transform.append(readDouble(source, &length)); // transform3
    d->transform.append(readDouble(source, &length)); // transform4
    d->transform.append(readDouble(source, &length)); // transform5
    d->transform.append(readDouble(source, &length)); // transform6
    d->transform.append(readDouble(source, &length)); // transform7
    d->transform.append(readDouble(source, &length)); // transform8

    // Warp version ( = 0 )
    auto warpVersion = readU32(source, &length);
    qCDebug(lcQPsdPlacedLayer) << warpVersion;
    Q_ASSERT(warpVersion == 0);

    //Warp descriptor version ( = 16 )
    auto warpDescriptorVersion = readU32(source, &length);
    Q_ASSERT(warpDescriptorVersion == 16);

    // Descriptor for warping information
    QPsdDescriptor worpDescriptor(source, &length);
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

QList<double> QPsdPlacedLayer::transform() const
{
    return d->transform;
}

void QPsdPlacedLayer::swap(QPsdPlacedLayer &other) noexcept
{
    d.swap(other.d);
}

QT_END_NAMESPACE
