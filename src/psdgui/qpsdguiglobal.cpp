#include "qpsdguiglobal.h"

#include <QtCore/QtMath>

QT_BEGIN_NAMESPACE

namespace QtPsdGui {

// --- HSL helper functions per W3C Compositing and Blending spec §13.3 ---

static inline qreal luminance(qreal r, qreal g, qreal b)
{
    return 0.299 * r + 0.587 * g + 0.114 * b;
}

static inline void clipColor(qreal &r, qreal &g, qreal &b)
{
    const qreal l = luminance(r, g, b);
    const qreal n = qMin(r, qMin(g, b));
    const qreal x = qMax(r, qMax(g, b));

    if (n < 0.0) {
        const qreal denom = l - n;
        if (denom > 1e-10) {
            r = l + (r - l) * l / denom;
            g = l + (g - l) * l / denom;
            b = l + (b - l) * l / denom;
        }
    }
    if (x > 1.0) {
        const qreal denom = x - l;
        if (denom > 1e-10) {
            r = l + (r - l) * (1.0 - l) / denom;
            g = l + (g - l) * (1.0 - l) / denom;
            b = l + (b - l) * (1.0 - l) / denom;
        }
    }
}

static inline void setLum(qreal &r, qreal &g, qreal &b, qreal l)
{
    const qreal d = l - luminance(r, g, b);
    r += d;
    g += d;
    b += d;
    clipColor(r, g, b);
}

static inline qreal saturation(qreal r, qreal g, qreal b)
{
    return qMax(r, qMax(g, b)) - qMin(r, qMin(g, b));
}

static inline void setSat(qreal &r, qreal &g, qreal &b, qreal s)
{
    // Sort channels into min, mid, max by pointer
    qreal *cmin = &r, *cmid = &g, *cmax = &b;
    if (*cmin > *cmid) std::swap(cmin, cmid);
    if (*cmid > *cmax) std::swap(cmid, cmax);
    if (*cmin > *cmid) std::swap(cmin, cmid);

    if (*cmax > *cmin) {
        *cmid = ((*cmid - *cmin) * s) / (*cmax - *cmin);
        *cmax = s;
    } else {
        *cmid = 0.0;
        *cmax = 0.0;
    }
    *cmin = 0.0;
}

// --- Blend formula helpers ---

static inline qreal colorBurnFormula(qreal dst, qreal src)
{
    if (dst >= 1.0) return 1.0;
    if (src <= 0.0) return 0.0;
    return 1.0 - qMin(1.0, (1.0 - dst) / src);
}

static inline qreal colorDodgeFormula(qreal dst, qreal src)
{
    if (dst <= 0.0) return 0.0;
    if (src >= 1.0) return 1.0;
    return qMin(1.0, dst / (1.0 - src));
}
QImage imageDataToImage(const QPsdAbstractImage &imageData, const QPsdFileHeader &fileHeader, const QPsdColorModeData &colorModeData, const QByteArray &iccProfile)
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
                    // Copy row by row to handle QImage's bytesPerLine alignment
                    const auto srcBytesPerLine = w * 3;
                    const uchar* src = reinterpret_cast<const uchar*>(data.constData());
                    for (quint32 y = 0; y < h; ++y) {
                        memcpy(image.scanLine(y), src + y * srcBytesPerLine, srcBytesPerLine);
                    }
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

                // Apply ICC color profile if available
                if (!iccProfile.isEmpty()) {
                    QColorSpace cmykColorSpace = QColorSpace::fromIccProfile(iccProfile);
                    if (cmykColorSpace.isValid()) {
                        image.setColorSpace(cmykColorSpace);
                        image = image.convertedToColorSpace(QColorSpace::SRgb);
                    }
                }
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
            qWarning() << "QPainter doesn't support QPsdBlend::Mode" << psdBlendMode;
            return QPainter::CompositionMode_SourceOver;

        // Custom blend modes handled by customBlend() — return SourceOver as fallback
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

bool isCustomBlendMode(QPsdBlend::Mode mode)
{
    switch (mode) {
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
        return true;
    default:
        return false;
    }
}

// Apply blend formula to a single pixel (all values in 0.0-1.0 straight alpha space)
static inline void blendPixel(qreal sr, qreal sg, qreal sb,
                               qreal dr, qreal dg, qreal db,
                               QPsdBlend::Mode mode,
                               qreal &outR, qreal &outG, qreal &outB)
{
    switch (mode) {
    case QPsdBlend::Mode::LinearDodge: // Add
        outR = qMin(sr + dr, 1.0);
        outG = qMin(sg + dg, 1.0);
        outB = qMin(sb + db, 1.0);
        break;
    case QPsdBlend::Mode::LinearBurn:
        outR = qMax(sr + dr - 1.0, 0.0);
        outG = qMax(sg + dg - 1.0, 0.0);
        outB = qMax(sb + db - 1.0, 0.0);
        break;
    case QPsdBlend::Mode::VividLight:
        // src<=0.5: ColorBurn(dst, 2*src); else ColorDodge(dst, 2*(src-0.5))
        outR = (sr <= 0.5) ? colorBurnFormula(dr, 2.0 * sr) : colorDodgeFormula(dr, 2.0 * (sr - 0.5));
        outG = (sg <= 0.5) ? colorBurnFormula(dg, 2.0 * sg) : colorDodgeFormula(dg, 2.0 * (sg - 0.5));
        outB = (sb <= 0.5) ? colorBurnFormula(db, 2.0 * sb) : colorDodgeFormula(db, 2.0 * (sb - 0.5));
        break;
    case QPsdBlend::Mode::LinearLight:
        // src<=0.5: LinearBurn(dst, 2*src); else LinearDodge(dst, 2*(src-0.5))
        outR = (sr <= 0.5) ? qMax(dr + 2.0 * sr - 1.0, 0.0) : qMin(dr + 2.0 * (sr - 0.5), 1.0);
        outG = (sg <= 0.5) ? qMax(dg + 2.0 * sg - 1.0, 0.0) : qMin(dg + 2.0 * (sg - 0.5), 1.0);
        outB = (sb <= 0.5) ? qMax(db + 2.0 * sb - 1.0, 0.0) : qMin(db + 2.0 * (sb - 0.5), 1.0);
        break;
    case QPsdBlend::Mode::PinLight:
        // src<=0.5: min(dst, 2*src); else max(dst, 2*(src-0.5))
        outR = (sr <= 0.5) ? qMin(dr, 2.0 * sr) : qMax(dr, 2.0 * (sr - 0.5));
        outG = (sg <= 0.5) ? qMin(dg, 2.0 * sg) : qMax(dg, 2.0 * (sg - 0.5));
        outB = (sb <= 0.5) ? qMin(db, 2.0 * sb) : qMax(db, 2.0 * (sb - 0.5));
        break;
    case QPsdBlend::Mode::HardMix:
        outR = (sr + dr >= 1.0) ? 1.0 : 0.0;
        outG = (sg + dg >= 1.0) ? 1.0 : 0.0;
        outB = (sb + db >= 1.0) ? 1.0 : 0.0;
        break;
    case QPsdBlend::Mode::Subtract:
        outR = qMax(dr - sr, 0.0);
        outG = qMax(dg - sg, 0.0);
        outB = qMax(db - sb, 0.0);
        break;
    case QPsdBlend::Mode::Divide:
        outR = (sr > 1.0 / 256.0) ? qMin(dr / sr, 1.0) : 1.0;
        outG = (sg > 1.0 / 256.0) ? qMin(dg / sg, 1.0) : 1.0;
        outB = (sb > 1.0 / 256.0) ? qMin(db / sb, 1.0) : 1.0;
        break;
    case QPsdBlend::Mode::DarkerColor: {
        const qreal sl = luminance(sr, sg, sb);
        const qreal dl = luminance(dr, dg, db);
        if (sl < dl) { outR = sr; outG = sg; outB = sb; }
        else         { outR = dr; outG = dg; outB = db; }
        break;
    }
    case QPsdBlend::Mode::LighterColor: {
        const qreal sl = luminance(sr, sg, sb);
        const qreal dl = luminance(dr, dg, db);
        if (sl > dl) { outR = sr; outG = sg; outB = sb; }
        else         { outR = dr; outG = dg; outB = db; }
        break;
    }
    case QPsdBlend::Mode::Hue: {
        // SetLum(SetSat(src, Sat(dst)), Lum(dst))
        outR = sr; outG = sg; outB = sb;
        setSat(outR, outG, outB, saturation(dr, dg, db));
        setLum(outR, outG, outB, luminance(dr, dg, db));
        break;
    }
    case QPsdBlend::Mode::Saturation: {
        // SetLum(SetSat(dst, Sat(src)), Lum(dst))
        outR = dr; outG = dg; outB = db;
        setSat(outR, outG, outB, saturation(sr, sg, sb));
        setLum(outR, outG, outB, luminance(dr, dg, db));
        break;
    }
    case QPsdBlend::Mode::Color: {
        // SetLum(src, Lum(dst))
        outR = sr; outG = sg; outB = sb;
        setLum(outR, outG, outB, luminance(dr, dg, db));
        break;
    }
    case QPsdBlend::Mode::Luminosity: {
        // SetLum(dst, Lum(src))
        outR = dr; outG = dg; outB = db;
        setLum(outR, outG, outB, luminance(sr, sg, sb));
        break;
    }
    default:
        // Dissolve handled separately; fallback = normal
        outR = sr; outG = sg; outB = sb;
        break;
    }
}

void customBlend(QImage &dest, const QImage &src,
                  QPsdBlend::Mode mode, qreal opacity)
{
    const int w = qMin(dest.width(), src.width());
    const int h = qMin(dest.height(), src.height());
    if (w <= 0 || h <= 0)
        return;

    const bool isDissolve = (mode == QPsdBlend::Mode::Dissolve);

    for (int y = 0; y < h; ++y) {
        QRgb *dstLine = reinterpret_cast<QRgb *>(dest.scanLine(y));
        const QRgb *srcLine = reinterpret_cast<const QRgb *>(src.constScanLine(y));

        for (int x = 0; x < w; ++x) {
            const QRgb sp = srcLine[x];
            const QRgb dp = dstLine[x];

            // Unpremultiply source
            int sa = qAlpha(sp);
            if (sa == 0)
                continue;

            qreal sr, sg, sb;
            if (sa == 255) {
                sr = qRed(sp) / 255.0;
                sg = qGreen(sp) / 255.0;
                sb = qBlue(sp) / 255.0;
            } else {
                sr = qRed(sp) / (qreal)sa;
                sg = qGreen(sp) / (qreal)sa;
                sb = qBlue(sp) / (qreal)sa;
            }

            // Effective source alpha
            const qreal srcAlpha = (sa / 255.0) * opacity;

            if (isDissolve) {
                // Deterministic dither based on pixel position
                const quint32 hash = (static_cast<quint32>(x) * 2654435761u)
                                   ^ (static_cast<quint32>(y) * 2246822519u);
                const qreal threshold = (hash & 0xFFFF) / 65535.0;
                if (threshold < srcAlpha) {
                    // Source pixel fully replaces destination
                    dstLine[x] = qRgba(qBound(0, qRound(sr * 255.0), 255),
                                        qBound(0, qRound(sg * 255.0), 255),
                                        qBound(0, qRound(sb * 255.0), 255),
                                        255);
                }
                // else: keep destination as-is
                continue;
            }

            // Unpremultiply destination
            const int da = qAlpha(dp);
            qreal dr, dg, db;
            if (da == 0) {
                dr = dg = db = 0.0;
            } else if (da == 255) {
                dr = qRed(dp) / 255.0;
                dg = qGreen(dp) / 255.0;
                db = qBlue(dp) / 255.0;
            } else {
                dr = qRed(dp) / (qreal)da;
                dg = qGreen(dp) / (qreal)da;
                db = qBlue(dp) / (qreal)da;
            }

            // Apply blend formula
            qreal outR, outG, outB;
            blendPixel(sr, sg, sb, dr, dg, db, mode, outR, outG, outB);

            // Composite: result = blend(src,dst) * srcAlpha + dst * (1 - srcAlpha)
            const qreal invSrcAlpha = 1.0 - srcAlpha;
            const qreal rr = outR * srcAlpha + dr * invSrcAlpha;
            const qreal rg = outG * srcAlpha + dg * invSrcAlpha;
            const qreal rb = outB * srcAlpha + db * invSrcAlpha;

            // Result alpha: standard Porter-Duff over
            const qreal dstAlpha = da / 255.0;
            const qreal ra = srcAlpha + dstAlpha * invSrcAlpha;

            if (ra < 1e-10) {
                dstLine[x] = 0;
                continue;
            }

            // Premultiply result
            const int finalR = qBound(0, qRound(rr * ra * 255.0), 255);
            const int finalG = qBound(0, qRound(rg * ra * 255.0), 255);
            const int finalB = qBound(0, qRound(rb * ra * 255.0), 255);
            const int finalA = qBound(0, qRound(ra * 255.0), 255);
            dstLine[x] = qRgba(finalR, finalG, finalB, finalA);
        }
    }
}

}

QT_END_NAMESPACE
