// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdadditionallayerinformationplugin.h>

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
                return {};
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
            if (imageMode == 2) {
                auto indexColorTable = readByteArray(source, 256 * 3, &length);
                Q_UNUSED(indexColorTable); // TODO
            }

            // Pattern data as Virtual Memory Array List
            {
                // Virtual Memory Array List
                // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#VirtualMemoryArrayList

                // Version ( =3)
                auto version = readU32(source, &length);
                if (version != 3) {
                    qWarning("patt: unsupported Virtual Memory Array List version %u", version);
                    return {};
                }

                // Length
                auto size = readU32(source, &length);
                qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "size" << size;
                EnsureSeek es(source, size);

                // Rectangle: top, left, bottom, right
                const auto rect = readRectangle(source, &length);
                qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "rect" << rect;

                // Number of channels
                auto channels = readU32(source, &length);

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
                    const auto rect = readRectangle(source, &length);
                    qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "rect" << rect;

                    // Pixel depth: 1, 8, 16 or 32
                    depth = readU16(source, &length);
                    qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "depth" << depth;
                    Q_ASSERT(depth == 1 || depth == 8 || depth == 16 || depth == 32);

                    // Compression mode of data to follow. 1 is zip.
                    auto compression = readU8(source, &length);
                    qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << "compression" << compression;

                    // Actual data based on parameters and compression
                    qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << (size - 4 - 16 - 2 - 1);;
                    auto data = readByteArray(source, size - 4 - 16 - 2 - 1, &length);
                    Q_UNUSED(data); // TODO
                }
            }
            qCDebug(lcQPsdAdditionalLayerInformationPattPlugin) << initialLength << patternLength << initialLength - patternLength; // << es.bytesAvailable();
            skip(source, es.paddingSize(), &length);
        }
        return true;
    }
};

QT_END_NAMESPACE

#include "patt.moc"
