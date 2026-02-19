// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdplacedlayerdata.h"

#include <QtCore/QLoggingCategory>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdPlacedLayerData, "qt.psdcore.placedlayerdata")

class QPsdPlacedLayerData::Private : public QSharedData
{
public:
    QPsdDescriptor descriptor;
    QByteArray rawData;
};

QPsdPlacedLayerData::QPsdPlacedLayerData()
    : QPsdSection()
    , d(new Private)
{}

QPsdPlacedLayerData::QPsdPlacedLayerData(QIODevice *source, quint32 length)
    : QPsdPlacedLayerData()
{
    // Placed Layer Data (Photoshop CS3)
    auto cleanup = qScopeGuard([&] {
        // Q_ASSERT(length == 0);
    });

    // Identifier ( = 'soLD' )
    auto type = readByteArray(source, 4, &length);
    Q_ASSERT(type == "soLD");

    // Version ( = 4 )
    auto version = readU32(source, &length);
    Q_ASSERT(version == 4);

    // Descriptor Version ( = 16)
    auto descriptorVersion = readU32(source, &length);
    Q_ASSERT(descriptorVersion == 16);

    // Descriptor of placed layer information
    d->descriptor = QPsdDescriptor(source, &length);
    qDebug() << d->descriptor.data().keys();
}

QPsdPlacedLayerData::QPsdPlacedLayerData(const QPsdPlacedLayerData &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdPlacedLayerData &QPsdPlacedLayerData::operator=(const QPsdPlacedLayerData &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdPlacedLayerData::~QPsdPlacedLayerData() = default;

QPsdDescriptor QPsdPlacedLayerData::descriptor() const
{
    return d->descriptor;
}

QByteArray QPsdPlacedLayerData::rawData() const
{
    return d->rawData;
}

void QPsdPlacedLayerData::setRawData(const QByteArray &data)
{
    d->rawData = data;
}

void QPsdPlacedLayerData::write(QIODevice *dest) const
{
    if (!d->rawData.isEmpty()) {
        dest->write(d->rawData);
        return;
    }
    dest->write("soLD", 4);
    writeU32(dest, 4);   // version
    writeU32(dest, 16);  // descriptor version
    d->descriptor.write(dest);
}

QT_END_NAMESPACE
