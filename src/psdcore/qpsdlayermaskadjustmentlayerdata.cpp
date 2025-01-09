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
    QString errorString;  // Store error messages for invalid reads
};

QPsdLayerMaskAdjustmentLayerData::Private::Private()
    : defaultColor(0)
    , flags(0)
    , errorString()
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
    //
    // Note on partial reads:
    // The layer mask data can have variable length and structure based on flags.
    // When length == 2, we have padding bytes that we must consume, but we continue
    // parsing afterward to handle any remaining data. This prevents infinite loops
    // that could occur if bytes aren't fully consumed.
    //
    // Size of the data: Check the size and flags to determine what is or is not present. If zero, the following fields are not present
    quint32 length = readU32(source);
    if (length == 0)
        return;

    // Verify length is not unreasonably large (prevent integer overflow in calculations)
    if (length > 0x7FFFFFFF) {  // Max reasonable size for a layer mask section
        setErrorString("Layer mask data length is unreasonably large");
        return;
    }

    // Verify minimum required length for basic structure (rectangle + defaultColor + flags = 10 bytes)
    if (length < 10) {
        setErrorString("Insufficient data for layer mask basic structure");
        return;
    }

    auto cleanup = qScopeGuard([&] {
        if (length != 0) {
            qWarning() << "Layer mask data had" << length << "bytes remaining";
        }
    });
    EnsureSeek es(source, length);

    // Rectangle enclosing layer mask: Top, left, bottom, right (8 bytes)
    d->rect = readRectangle(source, &length);

    // Default color. 0 or 255 (1 byte)
    d->defaultColor = readU8(source, &length);

    // Flags. (1 byte)
    // bit 0 = position relative to layer
    // bit 1 = layer mask disabled
    // bit 2 = invert layer mask when blending (Obsolete)
    // bit 3 = indicates that the user mask actually came from rendering other data
    // bit 4 = indicates that the user and/or vector masks have parameters applied to them
    d->flags = readU8(source, &length);

    // Mask Parameters. Only present if bit 4 of Flags set above.
    if (d->flags & 0x10) {
        auto maskParameters = readU8(source, &length);
        Q_UNUSED(maskParameters); // TODO
        qDebug() << "maskParameters" << maskParameters;
    }

    // Padding. Only present if size = 20. Otherwise the following is present
    // Note: We don't return early here to ensure all bytes are properly consumed
    if (length == 2) {
        skip(source, 2, &length);
        // After skipping padding, length should be 0
        if (length != 0) {
            qWarning() << "Unexpected bytes remaining after padding:" << length;
        }
        return;  // No more data to process after padding
    }

    // Only continue parsing if we have enough data for the extended structure
    // Note: We only get here if length != 2 and we have more data to process
    if (length >= 10) {  // realFlags(1) + realBackground(1) + rectangle(8)
        // Real Flags. Same as Flags information above.
        auto realFlags = readU8(source, &length);
        Q_UNUSED(realFlags); // TODO

        // Real user mask background. 0 or 255.
        auto realUserMaskBackground = readU8(source, &length);
        Q_UNUSED(realUserMaskBackground); // TODO

        // Rectangle enclosing layer mask: Top, left, bottom, right.
        auto rect = readRectangle(source, &length);
        Q_UNUSED(rect); // TODO
    } else if (length > 0) {
        // We have some bytes left but not enough for the full extended structure
        qWarning() << "Incomplete extended layer mask data structure:" << length << "bytes remaining";
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

QT_END_NAMESPACE
