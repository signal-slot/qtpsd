// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdlayermaskadjustmentlayerdata.h"

QT_BEGIN_NAMESPACE

class QPsdLayerMaskAdjustmentLayerData::Private : public QSharedData
{
public:
    Private();
    QRect rect;
    quint8 defaultColor;
    quint8 flags;
    QRect realUserMaskRect;
    QByteArray rawData;
};

QPsdLayerMaskAdjustmentLayerData::Private::Private()
    : defaultColor(0)
    , flags(0)
{}

QPsdLayerMaskAdjustmentLayerData::QPsdLayerMaskAdjustmentLayerData()
    : QPsdSection()
    , d(new Private)
{}

QPsdLayerMaskAdjustmentLayerData::QPsdLayerMaskAdjustmentLayerData(QIODevice *source)
    : QPsdLayerMaskAdjustmentLayerData()
{
    // Layer mask / adjustment layer data
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_22582

    // Size of the data: Check the size and flags to determine what is or is not present. If zero, the following fields are not present
    auto length = readU32(source);
    if (length == 0)
        return;
    d->rawData = source->peek(length);
    auto cleanup = qScopeGuard([&] {
        Q_ASSERT(length <= 3);
    });
    EnsureSeek es(source, length);

    // Rectangle enclosing layer mask: Top, left, bottom, right
    d->rect = readRectangle(source, &length);

    // Default color. 0 or 255
    d->defaultColor = readU8(source, &length);

    // Flags.
    // bit 0 = position relative to layer
    // bit 1 = layer mask disabled
    // bit 2 = invert layer mask when blending (Obsolete)
    // bit 3 = indicates that the user mask actually came from rendering other data
    // bit 4 = indicates that the user and/or vector masks have parameters applied to them
    d->flags = readU8(source, &length);

    if (length >= 18) {
        // Real Flags. Same as Flags information above.
        auto realFlags = readU8(source, &length);
        Q_UNUSED(realFlags); // TODO
        // Real user mask background. 0 or 255.
        auto realUserMaskBackground = readU8(source, &length);
        Q_UNUSED(realUserMaskBackground); // TODO

        // Rectangle enclosing layer mask: Top, left, bottom, right.
        d->realUserMaskRect = readRectangle(source, &length);
    }

    // Mask Parameters. Only present if bit 4 of Flags set above.
    if (d->flags & 0x10) {
        auto maskParameters = readU8(source, &length);

        if (maskParameters & 0x01) {
            auto userMaskDensity = readU8(source, &length);
            Q_UNUSED(userMaskDensity);
        }
        if (maskParameters & 0x02) {
            auto userMaskFeather = readDouble(source, &length);
            Q_UNUSED(userMaskFeather);
        }
        if (maskParameters & 0x04) {
            auto vectorMaskDensity = readU8(source, &length);
            Q_UNUSED(vectorMaskDensity);
        }
        if (maskParameters & 0x08) {
            auto vectorMaskFeather = readDouble(source, &length);
            Q_UNUSED(vectorMaskFeather);
        }
    }
}

QPsdLayerMaskAdjustmentLayerData::QPsdLayerMaskAdjustmentLayerData(const QPsdLayerMaskAdjustmentLayerData &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdLayerMaskAdjustmentLayerData &QPsdLayerMaskAdjustmentLayerData::operator=(const QPsdLayerMaskAdjustmentLayerData &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdLayerMaskAdjustmentLayerData::~QPsdLayerMaskAdjustmentLayerData() = default;

bool QPsdLayerMaskAdjustmentLayerData::isEmpty() const
{
    return d->rect.isEmpty();
}

QRect QPsdLayerMaskAdjustmentLayerData::rect() const
{
    return d->rect;
}

quint8 QPsdLayerMaskAdjustmentLayerData::defaultColor() const
{
    return d->defaultColor;
}

bool QPsdLayerMaskAdjustmentLayerData::isPositionRelativeToLayer() const
{
    return d->flags & 0x01;
}

bool QPsdLayerMaskAdjustmentLayerData::isLayerMaskDisabled() const
{
    return d->flags & 0x02;
}

bool QPsdLayerMaskAdjustmentLayerData::isLayerMaskFromRenderingOtherData() const
{
    return d->flags & 0x08;
}

bool QPsdLayerMaskAdjustmentLayerData::isLayerMaskFromVectorData() const
{
    return d->flags & 0x10;
}

QRect QPsdLayerMaskAdjustmentLayerData::realUserMaskRect() const
{
    if (d->realUserMaskRect.isEmpty()) {
        return d->rect;
    } else {
        return d->realUserMaskRect;
    }
}

QByteArray QPsdLayerMaskAdjustmentLayerData::rawData() const
{
    return d->rawData;
}

QT_END_NAMESPACE
