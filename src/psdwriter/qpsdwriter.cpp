// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdwriter.h"
#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>
#include <QtPsdCore/qpsdblend.h>

#include <QtCore/QBuffer>
#include <QtCore/QFile>

QT_BEGIN_NAMESPACE

class QPsdWriter::Private : public QSharedData
{
public:
    QPsdFileHeader fileHeader;
    QPsdColorModeData colorModeData;
    QPsdImageResources imageResources;
    QPsdLayerAndMaskInformation layerAndMaskInformation;
    QPsdImageData imageData;
    mutable QString errorString;
};

QPsdWriter::QPsdWriter()
    : d(new Private)
{}

QPsdWriter::QPsdWriter(const QPsdWriter &other)
    : d(other.d)
{}

QPsdWriter &QPsdWriter::operator=(const QPsdWriter &other)
{
    if (this != &other)
        d.operator=(other.d);
    return *this;
}

QPsdWriter::~QPsdWriter() = default;

QPsdFileHeader QPsdWriter::fileHeader() const
{
    return d->fileHeader;
}

void QPsdWriter::setFileHeader(const QPsdFileHeader &fileHeader)
{
    d->fileHeader = fileHeader;
}

QPsdColorModeData QPsdWriter::colorModeData() const
{
    return d->colorModeData;
}

void QPsdWriter::setColorModeData(const QPsdColorModeData &colorModeData)
{
    d->colorModeData = colorModeData;
}

QPsdImageResources QPsdWriter::imageResources() const
{
    return d->imageResources;
}

void QPsdWriter::setImageResources(const QPsdImageResources &imageResources)
{
    d->imageResources = imageResources;
}

QPsdLayerAndMaskInformation QPsdWriter::layerAndMaskInformation() const
{
    return d->layerAndMaskInformation;
}

void QPsdWriter::setLayerAndMaskInformation(const QPsdLayerAndMaskInformation &info)
{
    d->layerAndMaskInformation = info;
}

QPsdImageData QPsdWriter::imageData() const
{
    return d->imageData;
}

void QPsdWriter::setImageData(const QPsdImageData &imageData)
{
    d->imageData = imageData;
}

QString QPsdWriter::errorString() const
{
    return d->errorString;
}

// Helper to write layer mask adjustment layer data
static void writeLayerMaskData(QIODevice *dest, const QPsdLayerMaskAdjustmentLayerData &maskData)
{
    if (maskData.isEmpty()) {
        // Empty mask data: just write length 0
        QPsdSection::writeU32(dest, 0);
    } else {
        // Write the mask rect + default color + flags = 20 bytes
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QPsdSection::writeRectangle(&buf, maskData.rect());
        QPsdSection::writeU8(&buf, maskData.defaultColor());
        quint8 flags = 0;
        if (maskData.isPositionRelativeToLayer()) flags |= 0x01;
        if (maskData.isLayerMaskDisabled()) flags |= 0x02;
        if (maskData.isLayerMaskFromRenderingOtherData()) flags |= 0x08;
        if (maskData.isLayerMaskFromVectorData()) flags |= 0x10;
        QPsdSection::writeU8(&buf, flags);
        // Padding to get to 20 bytes (rect=16 + defaultColor=1 + flags=1 + padding=2)
        buf.write(QByteArray(2, '\0'));
        buf.close();

        QPsdSection::writeU32(dest, buf.data().size());
        dest->write(buf.data());
    }
}

// Helper to write layer blending ranges data
static void writeBlendingRangesData(QIODevice *dest, const QPsdLayerBlendingRangesData &rangesData)
{
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);

    // Gray blend source and destination ranges (4 bytes)
    auto srcRange = rangesData.grayBlendSourceRange();
    auto dstRange = rangesData.grayBlendDestinationRange();
    QPsdSection::writeU8(&buf, srcRange.first);
    QPsdSection::writeU8(&buf, srcRange.second);
    QPsdSection::writeU8(&buf, dstRange.first);
    QPsdSection::writeU8(&buf, dstRange.second);

    // Channel ranges
    const auto &channelSrcRanges = rangesData.channelSourceRanges();
    const auto &channelDstRanges = rangesData.channelDestinationRanges();
    for (int i = 0; i < channelSrcRanges.size() && i < channelDstRanges.size(); ++i) {
        QPsdSection::writeU8(&buf, channelSrcRanges.at(i).first);
        QPsdSection::writeU8(&buf, channelSrcRanges.at(i).second);
        QPsdSection::writeU8(&buf, channelDstRanges.at(i).first);
        QPsdSection::writeU8(&buf, channelDstRanges.at(i).second);
    }
    buf.close();

    QPsdSection::writeU32(dest, buf.data().size());
    dest->write(buf.data());
}

bool QPsdWriter::write(QIODevice *device) const
{
    d->errorString.clear();

    if (!device || !device->isWritable()) {
        d->errorString = u"Device is not writable"_s;
        return false;
    }

    // === Section 1: File Header (26 bytes fixed) ===
    // Signature
    device->write("8BPS", 4);
    // Version
    QPsdSection::writeU16(device, 1);
    // Reserved (6 bytes of zero)
    device->write(QByteArray(6, '\0'));
    // Channels
    QPsdSection::writeU16(device, d->fileHeader.channels());
    // Height
    QPsdSection::writeU32(device, d->fileHeader.height());
    // Width
    QPsdSection::writeU32(device, d->fileHeader.width());
    // Depth
    QPsdSection::writeU16(device, d->fileHeader.depth());
    // Color Mode
    QPsdSection::writeU16(device, static_cast<quint16>(d->fileHeader.colorMode()));

    // === Section 2: Color Mode Data ===
    const QByteArray &colorData = d->colorModeData.colorData();
    QPsdSection::writeU32(device, colorData.size());
    if (!colorData.isEmpty())
        device->write(colorData);

    // === Section 3: Image Resources ===
    {
        QBuffer irBuf;
        irBuf.open(QIODevice::WriteOnly);
        const auto &blocks = d->imageResources.imageResourceBlocks();
        for (const auto &block : blocks) {
            // Signature
            irBuf.write("8BIM", 4);
            // Resource ID
            QPsdSection::writeU16(&irBuf, block.id());
            // Pascal string (padded to even)
            QPsdSection::writePascalString(&irBuf, block.name(), 2);
            // Data length + data + even padding
            const QByteArray &data = block.data();
            QPsdSection::writeU32(&irBuf, data.size());
            irBuf.write(data);
            if (data.size() % 2 != 0)
                irBuf.write(QByteArray(1, '\0'));
        }
        irBuf.close();
        QPsdSection::writeU32(device, irBuf.data().size());
        device->write(irBuf.data());
    }

    // === Section 4: Layer and Mask Information ===
    {
        QBuffer lmiBuf;
        lmiBuf.open(QIODevice::WriteOnly);

        const auto &layerInfo = d->layerAndMaskInformation.layerInfo();
        const auto &records = layerInfo.records();
        const auto &channelImageDataList = layerInfo.channelImageData();

        // --- Layer Info sub-section ---
        {
            QBuffer liBuf;
            liBuf.open(QIODevice::WriteOnly);

            // Layer count (signed, negative means first alpha is merged result)
            QPsdSection::writeS16(&liBuf, static_cast<qint16>(records.size()));

            // --- Layer records ---
            // We need to compute and set channel lengths before writing records.
            // Build a list of per-record channel data sizes.
            QList<QList<quint32>> allChannelSizes;
            for (int ri = 0; ri < records.size(); ++ri) {
                const auto &record = records.at(ri);
                QList<quint32> sizes;
                for (const auto &ci : record.channelInfo()) {
                    quint32 channelDataSize = 0;
                    if (ri < channelImageDataList.size()) {
                        // Attempt to get size from the channel image data
                        // For raw data: 2 (compression) + raw bytes
                        const auto &cid = channelImageDataList.at(ri);
                        Q_UNUSED(cid);
                    }
                    // Use the length as stored in channelInfo (already set, or computed)
                    channelDataSize = ci.length();
                    sizes.append(channelDataSize);
                }
                allChannelSizes.append(sizes);
            }

            for (int ri = 0; ri < records.size(); ++ri) {
                const auto &record = records.at(ri);

                // Rectangle
                QPsdSection::writeRectangle(&liBuf, record.rect());

                // Number of channels
                const auto &channelInfoList = record.channelInfo();
                QPsdSection::writeU16(&liBuf, static_cast<quint16>(channelInfoList.size()));

                // Channel info entries
                for (const auto &ci : channelInfoList) {
                    QPsdSection::writeU16(&liBuf, static_cast<quint16>(static_cast<qint16>(ci.id())));
                    QPsdSection::writeU32(&liBuf, ci.length());
                }

                // Blend mode signature
                liBuf.write("8BIM", 4);

                // Blend mode key
                QByteArray blendKey = QPsdBlend::toKey(record.blendMode());
                liBuf.write(blendKey.leftJustified(4, ' ', true));

                // Opacity
                QPsdSection::writeU8(&liBuf, record.opacity());

                // Clipping
                QPsdSection::writeU8(&liBuf, static_cast<quint8>(record.clipping()));

                // Flags
                quint8 flags = 0;
                if (record.isTransparencyProtected()) flags |= 0x01;
                if (record.isVisible()) flags |= 0x02;
                if (record.hasPixelDataIrrelevantToAppearanceDocument()) flags |= 0x08;
                if (record.isPixelDataIrrelevantToAppearanceDocument()) flags |= 0x10;
                QPsdSection::writeU8(&liBuf, flags);

                // Filler
                QPsdSection::writeU8(&liBuf, 0);

                // Extra data (mask data + blending ranges + name + additional layer info)
                QBuffer extraBuf;
                extraBuf.open(QIODevice::WriteOnly);

                // Layer mask data
                writeLayerMaskData(&extraBuf, record.layerMaskAdjustmentLayerData());

                // Layer blending ranges
                writeBlendingRangesData(&extraBuf, record.layerBlendingRangesData());

                // Layer name (Pascal string, padded to multiple of 4)
                QPsdSection::writePascalString(&extraBuf, record.name(), 4);

                // Additional layer information
                const auto &ali = record.additionalLayerInformation();
                for (auto it = ali.constBegin(); it != ali.constEnd(); ++it) {
                    QByteArray payload;
                    if (it.value().typeId() == QMetaType::QByteArray) {
                        payload = it.value().toByteArray();
                    } else {
                        auto plugin = QPsdAdditionalLayerInformationPlugin::plugin(it.key());
                        if (plugin)
                            payload = plugin->serialize(it.value());
                    }
                    if (!payload.isEmpty()) {
                        extraBuf.write("8BIM", 4);
                        extraBuf.write(it.key().leftJustified(4, '\0', true));
                        // Per-layer ALI: length rounded up to even byte count
                        // (parser uses EnsureSeek with padding=0, so length must include padding)
                        quint32 paddedSize = payload.size();
                        if (paddedSize % 2 != 0) paddedSize++;
                        QPsdSection::writeU32(&extraBuf, paddedSize);
                        extraBuf.write(payload);
                        if (payload.size() % 2 != 0)
                            extraBuf.write(QByteArray(1, '\0'));
                    }
                }
                extraBuf.close();

                // Extra data length
                QPsdSection::writeU32(&liBuf, extraBuf.data().size());
                liBuf.write(extraBuf.data());
            }

            // --- Channel image data ---
            for (int ri = 0; ri < records.size(); ++ri) {
                const auto &record = records.at(ri);
                for (const auto &ci : record.channelInfo()) {
                    // Compression type: 0 = Raw
                    QPsdSection::writeU16(&liBuf, 0);

                    // Raw channel data
                    if (ri < channelImageDataList.size()) {
                        const auto &cid = channelImageDataList.at(ri);
                        // Get raw data based on channel ID
                        QByteArray channelData;
                        switch (ci.id()) {
                        case QPsdChannelInfo::TransparencyMask:
                            channelData = cid.transparencyMaskData();
                            break;
                        case QPsdChannelInfo::UserSuppliedLayerMask:
                            channelData = cid.userSuppliedLayerMask();
                            break;
                        default:
                            // For R/G/B/A channels, use the image data from the channel
                            // The channel image data stores raw per-channel data keyed by ID
                            channelData = cid.imageData();
                            break;
                        }
                        // Write the raw channel bytes
                        // Note: For a proper write, we should write per-channel data
                        // but the existing API returns combined image data.
                        // For round-trip fidelity, we rely on the channel length in channelInfo.
                        if (ci.length() > 2) {
                            // length includes the 2-byte compression field
                            quint32 dataSize = ci.length() - 2;
                            if (static_cast<quint32>(channelData.size()) >= dataSize) {
                                liBuf.write(channelData.constData(), dataSize);
                            } else {
                                // Write what we have, pad with zeros
                                liBuf.write(channelData);
                                if (dataSize > static_cast<quint32>(channelData.size())) {
                                    liBuf.write(QByteArray(dataSize - channelData.size(), '\0'));
                                }
                            }
                        }
                    }
                }
            }

            liBuf.close();

            // Write layer info length (rounded up to multiple of 2)
            // The length field itself must be the even-rounded value,
            // as the parser uses EnsureSeek(length, 0) and relies on length being even.
            quint32 liSize = liBuf.data().size();
            quint32 liPaddedSize = liSize;
            if (liPaddedSize % 2 != 0) liPaddedSize++;
            QPsdSection::writeU32(&lmiBuf, liPaddedSize);
            lmiBuf.write(liBuf.data());
            if (liSize % 2 != 0)
                lmiBuf.write(QByteArray(1, '\0'));
        }

        // --- Global layer mask info ---
        const auto &glmi = d->layerAndMaskInformation.globalLayerMaskInfo();
        if (glmi.length() == 0) {
            QPsdSection::writeU32(&lmiBuf, 0);
        } else {
            QBuffer glmiBuf;
            glmiBuf.open(QIODevice::WriteOnly);
            // Overlay color space + 4 color components (10 bytes)
            QPsdSection::writeU16(&glmiBuf, glmi.overlayColorSpace());
            // Color: 4 * 2 bytes (we write zeros since color is stored as string)
            glmiBuf.write(QByteArray(8, '\0'));
            // Opacity
            QPsdSection::writeU16(&glmiBuf, glmi.opacity());
            // Kind
            QPsdSection::writeU8(&glmiBuf, static_cast<quint8>(glmi.kind()));
            // Pad to fill remaining
            glmiBuf.close();

            QPsdSection::writeU32(&lmiBuf, glmiBuf.data().size());
            lmiBuf.write(glmiBuf.data());
        }

        // --- Additional layer information (top-level) ---
        const auto &topAli = d->layerAndMaskInformation.additionalLayerInformation();
        for (auto it = topAli.constBegin(); it != topAli.constEnd(); ++it) {
            QByteArray payload;
            if (it.value().typeId() == QMetaType::QByteArray) {
                payload = it.value().toByteArray();
            } else {
                auto plugin = QPsdAdditionalLayerInformationPlugin::plugin(it.key());
                if (plugin)
                    payload = plugin->serialize(it.value());
            }
            if (!payload.isEmpty()) {
                lmiBuf.write("8BIM", 4);
                lmiBuf.write(it.key().leftJustified(4, '\0', true));
                // Top-level ALI: length is actual payload size,
                // padded to 4-byte boundary (parser uses EnsureSeek with padding=4)
                QPsdSection::writeU32(&lmiBuf, payload.size());
                lmiBuf.write(payload);
                int remainder = payload.size() % 4;
                if (remainder != 0)
                    lmiBuf.write(QByteArray(4 - remainder, '\0'));
            }
        }

        lmiBuf.close();

        // Write entire layer and mask info section length
        QPsdSection::writeU32(device, lmiBuf.data().size());
        device->write(lmiBuf.data());
    }

    // === Section 5: Image Data ===
    {
        const QByteArray &imgData = d->imageData.imageData();
        // Compression: 0 = Raw
        QPsdSection::writeU16(device, 0);
        if (!imgData.isEmpty())
            device->write(imgData);
    }

    return true;
}

bool QPsdWriter::write(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QFile::WriteOnly)) {
        d->errorString = file.errorString();
        return false;
    }

    bool ok = write(&file);
    file.close();
    return ok;
}

QT_END_NAMESPACE
