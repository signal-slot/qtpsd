// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

#include <QtCore/QBuffer>
#include <QtGui/QImage>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdAdditionalLayerInformationPattPlugin, "qt.psdcore.plugin.patt")

class QPsdAdditionalLayerInformationPattPlugin : public QPsdAdditionalLayerInformationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdAdditionalLayerInformationFactoryInterface" FILE "patt.json")
public:
    // Patterns (Photoshop 6.0 and CS (8.0))
    QVariant parse(QIODevice *source , quint32 length) const override {
        auto cleanup = qScopeGuard([&] {
            if (length > 3)
                qWarning("patt: %u bytes remaining after parse", length);
        });

        // Save raw bytes for lossless round-trip (must be done before any parsing)
        const qint64 startPos = source->pos();
        const quint32 totalLength = length;
        const QByteArray rawBytes = source->read(totalLength);
        source->seek(startPos);

        QVariantHash result;
        result.insert(u"__rawData__"_s, rawBytes);

        // The following is repeated for each pattern.
        while (length > 20) {
            qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << (void *)source->pos() << source->pos() % 4;
            qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "length" << length;
            auto initialLength = length;

            // Length of this pattern
            auto patternLength = readU32(source, &length);
            EnsureSeek es(source, patternLength, 4);
            qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "patternLength" << patternLength << es.bytesAvailable();

            // Version ( =1)
            auto version = readU32(source, &length);
            if (version != 1) {
                qWarning("patt: unsupported pattern version %u", version);
                return QVariant::fromValue(result);
            }

            // The image mode of the file. Supported values are: Bitmap = 0; Grayscale = 1; Indexed = 2; RGB = 3; CMYK = 4; Multichannel = 7; Duotone = 8; Lab = 9.
            auto imageMode = readU32(source, &length);
            qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "imageMode" << imageMode;

            // Point: vertical, 2 bytes and horizontal, 2 bytes
            const auto pointX = readU16(source, &length);
            const auto pointY = readU16(source, &length);
            QPoint point(pointX, pointY);
            qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "point" << point;

            // Name: Unicode string
            auto name = readString(source, &length);
            qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "name" << name;

            // Unique ID for this pattern: Pascal string
            auto uniqueID = readPascalString(source, 1, &length);
            qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "uniqueID" << uniqueID;

            // Index color table (256 * 3 RGB values): only present when image mode is indexed color
            QByteArray indexColorTable;
            if (imageMode == 2) {
                indexColorTable = readByteArray(source, 256 * 3, &length);
            }

            // Pattern data as Virtual Memory Array List
            {
                // Virtual Memory Array List
                // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#VirtualMemoryArrayList

                // Version ( =3)
                auto version = readU32(source, &length);
                if (version != 3) {
                    qWarning("patt: unsupported Virtual Memory Array List version %u", version);
                    return QVariant::fromValue(result);
                }

                // Length
                auto size = readU32(source, &length);
                qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "size" << size;
                EnsureSeek es(source, size);

                // Rectangle: top, left, bottom, right
                const auto patternRect = readRectangle(source, &length);
                qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "rect" << patternRect;

                // Number of channels
                auto channels = readU32(source, &length);

                // Collect decompressed channel data
                QList<QByteArray> channelDataList;

                // The following is a virtual memory array, repeated
                // for the number of channels
                // + one for a user mask
                channels += 1;
                // + one for a sheet mask.
                channels += 1;
                for (quint32 i = 0; i < channels; i++) {
                    qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << i << channels << length;
                    // Boolean indicating whether array is written, skip following data if 0.
                    auto isWritten = readU32(source, &length);
                    qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "isWritten" << isWritten;
                    if (!isWritten) {
                        continue;
                    }

                    // Length, skip following data if 0.
                    auto size = readU32(source, &length);
                    qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "size" << size;
                    if (!size) {
                        continue;
                    }

                    // Pixel depth: 1, 8, 16 or 32
                    auto depth = readU32(source, &length);
                    qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "depth" << depth;
                    Q_ASSERT(depth == 1 || depth == 8 || depth == 16 || depth == 32);

                    // Rectangle: top, left, bottom, right
                    const auto channelRect = readRectangle(source, &length);
                    qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "rect" << channelRect;

                    // Pixel depth: 1, 8, 16 or 32
                    depth = readU16(source, &length);
                    qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "depth" << depth;
                    Q_ASSERT(depth == 1 || depth == 8 || depth == 16 || depth == 32);

                    // Compression mode of data to follow. 0 = raw, 1 = RLE (PackBits).
                    auto compression = readU8(source, &length);
                    qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "compression" << compression;

                    // Actual data based on parameters and compression
                    auto dataSize = size - 4 - 16 - 2 - 1;
                    qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << dataSize;
                    auto compressedData = readByteArray(source, dataSize, &length);

                    QByteArray channelData;
                    if (compression == 1) {
                        // RLE (PackBits) decompression
                        // Format: height row byte counts (2 bytes each) + packed data
                        QBuffer buffer(&compressedData);
                        buffer.open(QIODevice::ReadOnly);
                        const int h = channelRect.height();
                        quint32 bufLen = compressedData.size();
                        QList<qint16> byteCounts;
                        for (int y = 0; y < h; y++) {
                            byteCounts.append(readS16(&buffer, &bufLen));
                        }
                        for (qint16 byteCount : std::as_const(byteCounts)) {
                            EnsureSeek es(&buffer, byteCount);
                            while (es.bytesAvailable() > 0) {
                                auto sz = readS8(&buffer, &bufLen);
                                if (sz == -128) {
                                    // ignore
                                } else if (sz < 0) {
                                    channelData.append(-sz + 1, readByteArray(&buffer, 1, &bufLen).at(0));
                                } else {
                                    channelData.append(readByteArray(&buffer, sz + 1, &bufLen));
                                }
                            }
                        }
                    } else if (compression == 0) {
                        // Raw uncompressed data
                        channelData = compressedData;
                    }

                    if (!channelData.isEmpty()) {
                        channelDataList.append(channelData);
                    }
                }

                // Assemble channel data into QImage
                if (imageMode == 3 && !channelDataList.isEmpty()) {
                    // RGB mode
                    const int w = patternRect.width();
                    const int h = patternRect.height();
                    if (w > 0 && h > 0) {
                        QImage image(w, h, QImage::Format_ARGB32);
                        image.fill(Qt::transparent);

                        const int pixelCount = w * h;
                        for (int y = 0; y < h; ++y) {
                            auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
                            for (int x = 0; x < w; ++x) {
                                const int idx = y * w + x;
                                const int r = (channelDataList.size() > 0 && idx < channelDataList[0].size())
                                    ? static_cast<quint8>(channelDataList[0].at(idx)) : 0;
                                const int g = (channelDataList.size() > 1 && idx < channelDataList[1].size())
                                    ? static_cast<quint8>(channelDataList[1].at(idx)) : 0;
                                const int b = (channelDataList.size() > 2 && idx < channelDataList[2].size())
                                    ? static_cast<quint8>(channelDataList[2].at(idx)) : 0;
                                const int a = 255;
                                line[x] = qRgba(r, g, b, a);
                            }
                        }
                        Q_UNUSED(pixelCount);

                        result.insert(uniqueID, QVariant::fromValue(image));
                        qCDebug(lcQPsdAdditionalLayerInformationPattPlugin)
                            << "Pattern" << uniqueID << "assembled as" << w << "x" << h << "RGB image";
                    }
                } else if (imageMode == 1 && !channelDataList.isEmpty()) {
                    // Grayscale mode
                    const int w = patternRect.width();
                    const int h = patternRect.height();
                    if (w > 0 && h > 0) {
                        QImage image(w, h, QImage::Format_ARGB32);
                        image.fill(Qt::transparent);

                        for (int y = 0; y < h; ++y) {
                            auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
                            for (int x = 0; x < w; ++x) {
                                const int idx = y * w + x;
                                const int gray = (idx < channelDataList[0].size())
                                    ? static_cast<quint8>(channelDataList[0].at(idx)) : 0;
                                line[x] = qRgba(gray, gray, gray, 255);
                            }
                        }

                        result.insert(uniqueID, QVariant::fromValue(image));
                        qCDebug(lcQPsdAdditionalLayerInformationPattPlugin)
                            << "Pattern" << uniqueID << "assembled as" << w << "x" << h << "Grayscale image";
                    }
                }
            }
            qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << initialLength << patternLength << initialLength - patternLength; // << es.bytesAvailable();
            skip(source, es.paddingSize(), &length);
        }
        return QVariant::fromValue(result);
    }

    QByteArray serialize(const QVariant &data) const override {
        const auto hash = data.value<QVariantHash>();

        // Use raw bytes for lossless round-trip if available
        const auto rawData = hash.value(u"__rawData__"_s).toByteArray();
        if (!rawData.isEmpty())
            return rawData;

        QByteArray result;
        QBuffer output(&result);
        output.open(QIODevice::WriteOnly);

        for (auto it = hash.constBegin(); it != hash.constEnd(); ++it) {
            const QByteArray uniqueID = it.key().toLatin1();
            const QImage image = it.value().value<QImage>();
            if (image.isNull())
                continue;

            const QImage argbImage = image.convertToFormat(QImage::Format_ARGB32);
            const int w = argbImage.width();
            const int h = argbImage.height();
            const int pixelCount = w * h;

            // Determine if grayscale (all R==G==B)
            bool isGrayscale = true;
            for (int y = 0; y < h && isGrayscale; ++y) {
                const auto *line = reinterpret_cast<const QRgb *>(argbImage.constScanLine(y));
                for (int x = 0; x < w; ++x) {
                    if (qRed(line[x]) != qGreen(line[x]) || qRed(line[x]) != qBlue(line[x])) {
                        isGrayscale = false;
                        break;
                    }
                }
            }

            const quint32 imageMode = isGrayscale ? 1 : 3;
            const quint32 channelCount = isGrayscale ? 1 : 3;

            // Extract channel data from image
            QList<QByteArray> channelDataList;
            if (isGrayscale) {
                QByteArray grayData(pixelCount, Qt::Uninitialized);
                for (int y = 0; y < h; ++y) {
                    const auto *line = reinterpret_cast<const QRgb *>(argbImage.constScanLine(y));
                    for (int x = 0; x < w; ++x) {
                        grayData[y * w + x] = static_cast<char>(qRed(line[x]));
                    }
                }
                channelDataList.append(grayData);
            } else {
                QByteArray rData(pixelCount, Qt::Uninitialized);
                QByteArray gData(pixelCount, Qt::Uninitialized);
                QByteArray bData(pixelCount, Qt::Uninitialized);
                for (int y = 0; y < h; ++y) {
                    const auto *line = reinterpret_cast<const QRgb *>(argbImage.constScanLine(y));
                    for (int x = 0; x < w; ++x) {
                        const int idx = y * w + x;
                        rData[idx] = static_cast<char>(qRed(line[x]));
                        gData[idx] = static_cast<char>(qGreen(line[x]));
                        bData[idx] = static_cast<char>(qBlue(line[x]));
                    }
                }
                channelDataList.append(rData);
                channelDataList.append(gData);
                channelDataList.append(bData);
            }

            // Build VMAL data
            QByteArray vmalData;
            {
                QBuffer vmalBuf(&vmalData);
                vmalBuf.open(QIODevice::WriteOnly);

                const QRect patternRect(0, 0, w, h);
                writeRectangle(&vmalBuf, patternRect);
                writeU32(&vmalBuf, channelCount);

                // Actual image channels
                for (quint32 ch = 0; ch < channelCount; ++ch) {
                    writeU32(&vmalBuf, 1); // isWritten
                    const QByteArray &rawData = channelDataList[ch];
                    // size = U32(depth) + Rectangle(16) + U16(depth) + U8(compression) + rawData
                    const quint32 channelSize = 4 + 16 + 2 + 1 + rawData.size();
                    writeU32(&vmalBuf, channelSize);
                    writeU32(&vmalBuf, 8); // pixel depth
                    writeRectangle(&vmalBuf, patternRect);
                    writeU16(&vmalBuf, 8); // pixel depth
                    writeU8(&vmalBuf, 0);  // raw compression
                    writeByteArray(&vmalBuf, rawData);
                }

                // User mask (not written)
                writeU32(&vmalBuf, 0);
                // Sheet mask (not written)
                writeU32(&vmalBuf, 0);
            }

            // Build pattern data
            QByteArray patternData;
            {
                QBuffer patternBuf(&patternData);
                patternBuf.open(QIODevice::WriteOnly);

                writeU32(&patternBuf, 1); // version
                writeU32(&patternBuf, imageMode);
                writeU16(&patternBuf, 0); // point vertical
                writeU16(&patternBuf, 0); // point horizontal
                writeString(&patternBuf, QString::fromLatin1(uniqueID)); // name (use uniqueID)
                writePascalString(&patternBuf, uniqueID, 1);

                // VMAL: version + size + data
                writeU32(&patternBuf, 3); // VMAL version
                writeU32(&patternBuf, vmalData.size());
                writeByteArray(&patternBuf, vmalData);
            }

            // Write pattern length + data + padding
            const quint32 patternLength = patternData.size();
            writeU32(&output, patternLength);
            writeByteArray(&output, patternData);

            // Pad to 4-byte boundary based on patternLength
            if (patternLength % 4 != 0) {
                const int padBytes = 4 - (patternLength % 4);
                output.write(QByteArray(padBytes, '\0'));
            }
        }

        return result;
    }
};

QT_END_NAMESPACE

#include "patt.moc"
