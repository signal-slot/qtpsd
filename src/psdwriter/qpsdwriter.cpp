// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdwriter.h"
#include <QtPsdCore/qpsdabstractimage.h>
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
    const QByteArray raw = maskData.rawData();
    if (!raw.isEmpty()) {
        QPsdSection::writeU32(dest, raw.size());
        dest->write(raw);
    } else if (maskData.isEmpty()) {
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
    const QByteArray raw = rangesData.rawData();
    if (!raw.isEmpty()) {
        QPsdSection::writeU32(dest, raw.size());
        dest->write(raw);
        return;
    }

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

            if (!records.isEmpty()) {
            // Layer count (signed, negative means first alpha is merged result)
            qint16 layerCount = static_cast<qint16>(records.size());
            if (layerInfo.hasMergedAlpha())
                layerCount = -layerCount;
            QPsdSection::writeS16(&liBuf, layerCount);

            // --- Pre-compute channel encoded data ---
            // For each record, for each channel: encoded bytes (including compression u16)
            QList<QList<QByteArray>> allEncodedChannels;
            for (int ri = 0; ri < records.size(); ++ri) {
                const auto &record = records.at(ri);
                QList<QByteArray> encodedChannels;
                for (const auto &ci : record.channelInfo()) {
                    QByteArray encoded;
                    if (ri < channelImageDataList.size()) {
                        const auto &cid = channelImageDataList.at(ri);
                        const QByteArray channelData = cid.channelData(ci.id());
                        const auto compression = cid.channelCompression(ci.id());
                        if (!channelData.isEmpty()) {
                            int height = record.rect().height();
                            if (ci.id() == QPsdChannelInfo::UserSuppliedLayerMask) {
                                height = record.layerMaskAdjustmentLayerData().rect().height();
                            } else if (ci.id() == QPsdChannelInfo::RealUserSuppliedLayerMask) {
                                height = record.layerMaskAdjustmentLayerData().realUserMaskRect().height();
                            }
                            QBuffer comprBuf;
                            comprBuf.open(QIODevice::WriteOnly);
                            switch (compression) {
                            case QPsdAbstractImage::RawData:
                                QPsdSection::writeU16(&comprBuf, 0);
                                comprBuf.write(channelData);
                                break;
                            case QPsdAbstractImage::RLE:
                                if (height > 0) {
                                    int rowWidth = channelData.size() / height;
                                    QByteArray rleData = QPsdSection::encodePackBits(channelData, rowWidth, height);
                                    QPsdSection::writeU16(&comprBuf, 1);
                                    comprBuf.write(rleData);
                                } else {
                                    QPsdSection::writeU16(&comprBuf, 1);
                                }
                                break;
                            case QPsdAbstractImage::ZipWithoutPrediction:
                            case QPsdAbstractImage::ZipWithPrediction:
                                // TODO: implement zlib compression when encountered
                                QPsdSection::writeU16(&comprBuf, static_cast<quint16>(compression));
                                qWarning("ZIP compression re-encoding not yet implemented");
                                comprBuf.write(channelData);
                                break;
                            }
                            comprBuf.close();
                            encoded = comprBuf.data();
                        } else {
                            // No data: just compression u16
                            QBuffer comprBuf;
                            comprBuf.open(QIODevice::WriteOnly);
                            QPsdSection::writeU16(&comprBuf, 0);
                            comprBuf.close();
                            encoded = comprBuf.data();
                        }
                    } else {
                        // No channel image data for this record
                        QBuffer comprBuf;
                        comprBuf.open(QIODevice::WriteOnly);
                        QPsdSection::writeU16(&comprBuf, 0);
                        comprBuf.close();
                        encoded = comprBuf.data();
                    }
                    encodedChannels.append(encoded);
                }
                allEncodedChannels.append(encodedChannels);
            }

            // --- Layer records ---
            for (int ri = 0; ri < records.size(); ++ri) {
                const auto &record = records.at(ri);

                // Rectangle
                QPsdSection::writeRectangle(&liBuf, record.rect());

                // Number of channels
                const auto &channelInfoList = record.channelInfo();
                QPsdSection::writeU16(&liBuf, static_cast<quint16>(channelInfoList.size()));

                // Channel info entries (use pre-computed lengths)
                for (int ci = 0; ci < channelInfoList.size(); ++ci) {
                    QPsdSection::writeU16(&liBuf, static_cast<quint16>(static_cast<qint16>(channelInfoList.at(ci).id())));
                    QPsdSection::writeU32(&liBuf, static_cast<quint32>(allEncodedChannels.at(ri).at(ci).size()));
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

                // Flags (use raw flags byte directly)
                QPsdSection::writeU8(&liBuf, record.flags());

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

                // Additional layer information (preserve original order)
                const auto &ali = record.additionalLayerInformation();
                const auto &keyOrder = record.aliKeyOrder();
                // Use original key order if available, otherwise fall back to hash iteration
                const auto keys = keyOrder.isEmpty() ? ali.keys() : keyOrder;
                for (const auto &key : keys) {
                    if (!ali.contains(key))
                        continue;
                    const QVariant &value = ali.value(key);
                    QByteArray payload;
                    if (value.typeId() == QMetaType::QByteArray) {
                        payload = value.toByteArray();
                    } else {
                        auto plugin = QPsdAdditionalLayerInformationPlugin::plugin(key);
                        if (plugin)
                            payload = plugin->serialize(value);
                    }
                    if (!payload.isEmpty()) {
                        extraBuf.write("8BIM", 4);
                        extraBuf.write(key.leftJustified(4, '\0', true));
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
                for (int ci = 0; ci < allEncodedChannels.at(ri).size(); ++ci) {
                    liBuf.write(allEncodedChannels.at(ri).at(ci));
                }
            }

            } // end if (!records.isEmpty())

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
            QPsdSection::writeColorSpace(&glmiBuf, glmi.colorSpace());
            // Opacity
            QPsdSection::writeU16(&glmiBuf, glmi.opacity());
            // Kind
            QPsdSection::writeU8(&glmiBuf, static_cast<quint8>(glmi.kind()));
            // Filler/padding bytes
            const QByteArray &glmiRaw = glmi.rawData();
            if (!glmiRaw.isEmpty())
                glmiBuf.write(glmiRaw);
            glmiBuf.close();

            QPsdSection::writeU32(&lmiBuf, glmiBuf.data().size());
            lmiBuf.write(glmiBuf.data());
        }

        // --- Additional layer information (top-level, preserve original order) ---
        const auto &topAli = d->layerAndMaskInformation.additionalLayerInformation();
        const auto &topKeyOrder = d->layerAndMaskInformation.aliKeyOrder();
        const auto topKeys = topKeyOrder.isEmpty() ? topAli.keys() : topKeyOrder;
        for (const auto &key : topKeys) {
            if (!topAli.contains(key))
                continue;
            const QVariant &value = topAli.value(key);
            QByteArray payload;
            if (value.typeId() == QMetaType::QByteArray) {
                payload = value.toByteArray();
            } else {
                auto plugin = QPsdAdditionalLayerInformationPlugin::plugin(key);
                if (plugin)
                    payload = plugin->serialize(value);
            }
            if (!payload.isEmpty()) {
                lmiBuf.write("8BIM", 4);
                lmiBuf.write(key.leftJustified(4, '\0', true));
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
        const quint16 compression = d->imageData.compression();
        switch (compression) {
        case QPsdAbstractImage::RawData:
            QPsdSection::writeU16(device, 0);
            if (!imgData.isEmpty())
                device->write(imgData);
            break;
        case QPsdAbstractImage::RLE: {
            QPsdSection::writeU16(device, 1);
            if (!imgData.isEmpty()) {
                int channels = d->fileHeader.channels();
                int height = d->fileHeader.height();
                int channelSize = d->fileHeader.width() * d->fileHeader.depth() / 8;
                // RLE encode each scan line (height * channels total rows)
                QByteArray rleData = QPsdSection::encodePackBits(imgData, channelSize, height * channels);
                device->write(rleData);
            }
            break;
        }
        case QPsdAbstractImage::ZipWithoutPrediction:
        case QPsdAbstractImage::ZipWithPrediction:
            // TODO: implement zlib compression when encountered
            QPsdSection::writeU16(device, compression);
            qWarning("ZIP compression re-encoding for image data not yet implemented");
            if (!imgData.isEmpty())
                device->write(imgData);
            break;
        default:
            QPsdSection::writeU16(device, 0);
            if (!imgData.isEmpty())
                device->write(imgData);
            break;
        }
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
