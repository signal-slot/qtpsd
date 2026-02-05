#include "qpsdguiglobal.h"

QT_BEGIN_NAMESPACE

namespace QtPsdGui {
QImage imageDataToImage(const QPsdAbstractImage &imageData, const QPsdFileHeader &fileHeader, const QPsdColorModeData &colorModeData)
{
    QImage image;
    const auto w = imageData.width();
    const auto h = imageData.height();
    if (w * h == 0)
        return image;
    const auto depth = fileHeader.depth();
    const QByteArray data = imageData.toImage(fileHeader.colorMode());

    switch (fileHeader.colorMode()) {
    case QPsdFileHeader::Bitmap:
        // Bitmap mode is 1-bit per pixel
        if (depth == 1) {
            // Calculate expected size for 1-bit data
            const size_t bytesPerRow = (w + 7) / 8; // Round up to next byte
            const size_t expectedSize = static_cast<size_t>(bytesPerRow) * h;

            if (static_cast<size_t>(data.size()) >= expectedSize) {
                // Convert 1-bit to 8-bit grayscale
                image = QImage(w, h, QImage::Format_Grayscale8);
                const uchar* src = reinterpret_cast<const uchar*>(data.constData());
                uchar* dst = image.bits();

                for (quint32 y = 0; y < h; ++y) {
                    for (quint32 x = 0; x < w; ++x) {
                        size_t byteIndex = static_cast<size_t>(y) * bytesPerRow + x / 8;
                        int bitIndex = 7 - (x % 8); // MSB first
                        bool bit = (src[byteIndex] >> bitIndex) & 1;
                        dst[y * w + x] = bit ? 0 : 255;
                    }
                }
            } else {
                qFatal() << Q_FUNC_INFO << __LINE__ << "Expected" << expectedSize << "got" << data.size();
            }
        }
        break;

    case QPsdFileHeader::Grayscale:
        if (depth == 8) {
            // Create QImage that owns its data
            image = QImage(w, h, QImage::Format_Grayscale8);
            if (!image.isNull() && static_cast<size_t>(data.size()) >= static_cast<size_t>(w) * h) {
                memcpy(image.bits(), data.constData(), w * h);
            } else {
                qFatal() << Q_FUNC_INFO << __LINE__;
            }
        } else if (depth == 16) {
            // Create QImage that owns its data
            image = QImage(w, h, QImage::Format_Grayscale16);
            if (!image.isNull() && static_cast<size_t>(data.size()) >= static_cast<size_t>(w) * h * 2) {
                memcpy(image.bits(), data.constData(), w * h * 2);
            } else {
                qFatal() << Q_FUNC_INFO << __LINE__;
            }
        } else if (depth == 32) {
            // Convert 32-bit float grayscale to 16-bit
            image = QImage(w, h, QImage::Format_Grayscale16);
            const auto src = reinterpret_cast<const float *>(data.constData());
            auto dst = reinterpret_cast<quint16 *>(image.bits());
            const auto pixelCount = w * h;
            for (quint32 i = 0; i < pixelCount; ++i) {
                // Convert float (0.0-1.0) to 16-bit (0-65535)
                float value = src[i];
                value = qBound(0.0f, value, 1.0f);
                dst[i] = static_cast<quint16>(value * 65535.0f);
            }
        }
        break;

    case QPsdFileHeader::RGB:
        if (depth == 8) {
            if (imageData.hasAlpha()) {
                image = QImage(w, h, QImage::Format_ARGB32);
                if (!image.isNull() && static_cast<size_t>(data.size()) >= static_cast<size_t>(w) * h * 4) {
                    memcpy(image.bits(), data.constData(), w * h * 4);
                } else {
                    qFatal() << Q_FUNC_INFO << __LINE__;
                }
            } else {
                image = QImage(w, h, QImage::Format_BGR888);
                if (!image.isNull() && static_cast<size_t>(data.size()) >= static_cast<size_t>(w) * h * 3) {
                    memcpy(image.bits(), data.constData(), w * h * 3);
                } else {
                    qFatal() << Q_FUNC_INFO << __LINE__;
                }
            }
        } else if (depth == 16) {
            if (imageData.hasAlpha()) {
                image = QImage(w, h, QImage::Format_RGBA64);
                if (!image.isNull() && static_cast<size_t>(data.size()) >= static_cast<size_t>(w) * h * 8) {
                    memcpy(image.bits(), data.constData(), w * h * 8);
                } else {
                    qFatal() << Q_FUNC_INFO << __LINE__;
                }
            } else {
                image = QImage(w, h, QImage::Format_RGBX64);
                const size_t expectedSize = static_cast<size_t>(w) * h * 6; // 3 channels * 2 bytes
                if (!image.isNull() && static_cast<size_t>(data.size()) >= expectedSize) {
                    // Convert RGB16 to RGBX64 (add padding for X channel)
                    const quint16* src = reinterpret_cast<const quint16*>(data.constData());
                    quint16* dst = reinterpret_cast<quint16*>(image.bits());
                    for (quint32 i = 0; i < w * h; ++i) {
                        *dst++ = src[2]; // B
                        *dst++ = src[1]; // G
                        *dst++ = src[0]; // R
                        *dst++ = 65535;  // X (padding, full opacity)
                        src += 3;
                    }
                } else {
                    qFatal() << Q_FUNC_INFO << __LINE__ << "Expected" << expectedSize << "got" << data.size();
                }
            }
        } else if (depth == 32) {
            // Convert 32-bit float RGB to 16-bit for display using RGBA64 format
            if (imageData.hasAlpha()) {
                image = QImage(w, h, QImage::Format_RGBA64);
                const auto src = reinterpret_cast<const float *>(data.constData());
                auto dst = reinterpret_cast<quint16 *>(image.bits());
                for (quint32 y = 0; y < h; ++y) {
                    for (quint32 x = 0; x < w; ++x) {
                        const int srcIdx = (y * w + x) * 4;
                        const int dstIdx = (y * w + x) * 4;
                        // Convert float (0.0-1.0) to 16-bit (0-65535)
                        // Note: PSD stores in BGR order, but we need RGB
                        dst[dstIdx + 2] = static_cast<quint16>(qBound(0.0f, src[srcIdx], 1.0f) * 65535.0f);     // B -> B
                        dst[dstIdx + 1] = static_cast<quint16>(qBound(0.0f, src[srcIdx + 1], 1.0f) * 65535.0f); // G -> G
                        dst[dstIdx] = static_cast<quint16>(qBound(0.0f, src[srcIdx + 2], 1.0f) * 65535.0f);     // R -> R
                        dst[dstIdx + 3] = static_cast<quint16>(qBound(0.0f, src[srcIdx + 3], 1.0f) * 65535.0f); // A -> A
                    }
                }
            } else {
                image = QImage(w, h, QImage::Format_RGBX64);
                const auto src = reinterpret_cast<const float *>(data.constData());
                auto dst = reinterpret_cast<quint16 *>(image.bits());
                for (quint32 y = 0; y < h; ++y) {
                    for (quint32 x = 0; x < w; ++x) {
                        const int srcIdx = (y * w + x) * 3;
                        const int dstIdx = (y * w + x) * 4;
                        // Convert float (0.0-1.0) to 16-bit (0-65535)
                        // Note: PSD stores in BGR order, but we need RGB
                        dst[dstIdx + 2] = static_cast<quint16>(qBound(0.0f, src[srcIdx], 1.0f) * 65535.0f);     // B -> B
                        dst[dstIdx + 1] = static_cast<quint16>(qBound(0.0f, src[srcIdx + 1], 1.0f) * 65535.0f); // G -> G
                        dst[dstIdx] = static_cast<quint16>(qBound(0.0f, src[srcIdx + 2], 1.0f) * 65535.0f);     // R -> R
                        dst[dstIdx + 3] = 65535; // Alpha = 1.0
                    }
                }
            }
        }
        break;

    case QPsdFileHeader::CMYK:
        if (depth == 8 || depth == 16) {
            // Note: 16-bit CMYK is converted to 8-bit in toImage()
            image = QImage(w, h, QImage::Format_CMYK8888);
            if (!image.isNull() && static_cast<size_t>(data.size()) >= static_cast<size_t>(w) * h * 4) {
                memcpy(image.bits(), data.constData(), w * h * 4);
            } else {
                qFatal() << Q_FUNC_INFO << __LINE__;
            }
        }
        break;

    case QPsdFileHeader::Indexed:
        // Indexed color mode uses palette lookup
        if (depth == 8) {
            const QByteArray palette = colorModeData.colorData();
            if (palette.size() == 768) { // 256 colors * 3 bytes (RGB)
                // Convert indexed to RGB using palette
                image = QImage(w, h, QImage::Format_RGB888);
                if (!image.isNull() && static_cast<size_t>(data.size()) >= static_cast<size_t>(w) * h) {
                    const uchar* src = reinterpret_cast<const uchar*>(data.constData());
                    uchar* dst = image.bits();
                    const uchar* pal = reinterpret_cast<const uchar*>(palette.constData());
                    
                    for (quint32 i = 0; i < w * h; ++i) {
                        const int index = src[i];
                        dst[i * 3 + 0] = pal[index * 3 + 0]; // R
                        dst[i * 3 + 1] = pal[index * 3 + 1]; // G
                        dst[i * 3 + 2] = pal[index * 3 + 2]; // B
                    }
                } else {
                    qFatal() << Q_FUNC_INFO << __LINE__;
                }
            } else {
                // No palette data or invalid size, fall back to grayscale
                image = QImage(w, h, QImage::Format_Grayscale8);
                if (!image.isNull() && static_cast<size_t>(data.size()) >= static_cast<size_t>(w) * h) {
                    memcpy(image.bits(), data.constData(), w * h);
                } else {
                    qFatal() << Q_FUNC_INFO << __LINE__;
                }
            }
        }
        break;

    case QPsdFileHeader::Lab:
        if (depth == 8 || depth == 16) {
            // Lab color is converted to RGB in toImage()
            image = QImage(w, h, QImage::Format_RGB888);
            if (!image.isNull() && static_cast<size_t>(data.size()) >= static_cast<size_t>(w) * h * 3) {
                memcpy(image.bits(), data.constData(), w * h * 3);
            } else {
                qFatal() << Q_FUNC_INFO << __LINE__;
            }
        }
        break;

    case QPsdFileHeader::Multichannel:
        if (depth == 8 || depth == 16) {
            // Multichannel is converted to grayscale in toImage()
            image = QImage(w, h, QImage::Format_Grayscale8);
            if (!image.isNull() && static_cast<size_t>(data.size()) >= static_cast<size_t>(w) * h) {
                memcpy(image.bits(), data.constData(), w * h);
            } else {
                qFatal() << Q_FUNC_INFO << __LINE__;
            }
        }
        break;

    case QPsdFileHeader::Duotone:
        if (depth == 8 || depth == 16) {
            // Duotone is converted to grayscale in toImage()
            image = QImage(w, h, QImage::Format_Grayscale8);
            if (!image.isNull() && static_cast<size_t>(data.size()) >= static_cast<size_t>(w) * h) {
                memcpy(image.bits(), data.constData(), w * h);
            } else {
                qFatal() << Q_FUNC_INFO << __LINE__;
            }
        }
        break;

    default:
        qFatal() << fileHeader.colorMode() << "not supported";
    }

    // The QImage now owns its data, so we can return it directly
    return image;
}

QPainter::CompositionMode compositionMode(QPsdBlend::Mode psdBlendMode)
{
    switch (psdBlendMode) {
        case QPsdBlend::Mode::Invalid:
        case QPsdBlend::Mode::PassThrough:
        case QPsdBlend::Mode::Dissolve:
        case QPsdBlend::Mode::LinearBurn:
        case QPsdBlend::Mode::DarkerColor:
        case QPsdBlend::Mode::LinearDodge:
        case QPsdBlend::Mode::LighterColor:
        case QPsdBlend::Mode::VividLight:
        case QPsdBlend::Mode::LinearLight:
        case QPsdBlend::Mode::PinLight:
        case QPsdBlend::Mode::HardMix:
        case QPsdBlend::Mode::Subtract:
        case QPsdBlend::Mode::Divide:
        case QPsdBlend::Mode::Hue:
        case QPsdBlend::Mode::Saturation:
        case QPsdBlend::Mode::Color:
        case QPsdBlend::Mode::Luminosity:
            qWarning() << "QPainter doesn't support QPsdBlend::Mode" << psdBlendMode;
            return QPainter::CompositionMode_SourceOver;

        case QPsdBlend::Mode::Normal:
            return QPainter::CompositionMode_SourceOver;
        case QPsdBlend::Mode::Darken:
            return QPainter::CompositionMode_Darken;
        case QPsdBlend::Mode::Multiply:
            return QPainter::CompositionMode_Multiply;
        case QPsdBlend::Mode::ColorBurn:
            return QPainter::CompositionMode_ColorBurn;
        case QPsdBlend::Mode::Lighten:
            return QPainter::CompositionMode_Lighten;
        case QPsdBlend::Mode::Screen:
            return QPainter::CompositionMode_Screen;
        case QPsdBlend::Mode::ColorDodge:
            return QPainter::CompositionMode_ColorDodge;
        case QPsdBlend::Mode::Overlay:
            return QPainter::CompositionMode_Overlay;
        case QPsdBlend::Mode::SoftLight:
            return QPainter::CompositionMode_SoftLight;
        case QPsdBlend::Mode::HardLight:
            return QPainter::CompositionMode_HardLight;
        case QPsdBlend::Mode::Difference:
            return QPainter::CompositionMode_Difference;
        case QPsdBlend::Mode::Exclusion:
            return QPainter::CompositionMode_Exclusion;
        }
}

}

QT_END_NAMESPACE
