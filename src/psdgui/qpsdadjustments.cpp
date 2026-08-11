// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdadjustments.h"
#include "qpsdadjustmentlayeritem.h"
#include "qpsdimagelayeritem.h"
#include "qpsdshapelayeritem.h"

#include <QtPsdCore/qpsddescriptor.h>

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

#include <QtCore/QtMath>
#include <QtGui/QColor>

#include <algorithm>
#include <array>
#include <functional>

QT_BEGIN_NAMESPACE

using namespace Qt::Literals::StringLiterals;

namespace {

struct Rgb {
    qreal r, g, b;
};

qreal lum(const Rgb &c)
{
    return 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
}

struct Hsl {
    qreal h, s, l;
};

Hsl rgb2hsl(const Rgb &c)
{
    const qreal mx = qMax(c.r, qMax(c.g, c.b));
    const qreal mn = qMin(c.r, qMin(c.g, c.b));
    const qreal l = (mx + mn) * 0.5;
    if (qFuzzyCompare(mx, mn))
        return {0.0, 0.0, l};
    const qreal d = mx - mn;
    const qreal s = l > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);
    qreal h;
    if (mx == c.r)
        h = (c.g - c.b) / d + (c.g < c.b ? 6.0 : 0.0);
    else if (mx == c.g)
        h = (c.b - c.r) / d + 2.0;
    else
        h = (c.r - c.g) / d + 4.0;
    return {h / 6.0, s, l};
}

qreal hue2rgb(qreal p, qreal q, qreal t)
{
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0 / 2.0) return q;
    if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
    return p;
}

Rgb hsl2rgb(const Hsl &hsl)
{
    if (hsl.s <= 0.0)
        return {hsl.l, hsl.l, hsl.l};
    const qreal q = hsl.l < 0.5 ? hsl.l * (1.0 + hsl.s) : hsl.l + hsl.s - hsl.l * hsl.s;
    const qreal p = 2.0 * hsl.l - q;
    return {hue2rgb(p, q, hsl.h + 1.0 / 3.0), hue2rgb(p, q, hsl.h), hue2rgb(p, q, hsl.h - 1.0 / 3.0)};
}

qreal srgbToLinear(qreal c)
{
    return c <= 0.04045 ? c / 12.92 : qPow((c + 0.055) / 1.055, 2.4);
}

qreal linearToSrgb(qreal c)
{
    c = qBound(0.0, c, 1.0);
    return c <= 0.0031308 ? c * 12.92 : 1.055 * qPow(c, 1.0 / 2.4) - 0.055;
}

struct Oklab {
    qreal L, a, b;
};

Oklab srgbToOklab(const Rgb &c)
{
    const qreal r = srgbToLinear(c.r), g = srgbToLinear(c.g), b = srgbToLinear(c.b);
    const qreal l = qPow(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b, 1.0 / 3.0);
    const qreal m = qPow(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b, 1.0 / 3.0);
    const qreal s = qPow(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b, 1.0 / 3.0);
    return { 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
             1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
             0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s };
}

Rgb oklabToSrgb(const Oklab &c)
{
    const qreal l_ = c.L + 0.3963377774 * c.a + 0.2158037573 * c.b;
    const qreal m_ = c.L - 0.1055613458 * c.a - 0.0638541728 * c.b;
    const qreal s_ = c.L - 0.0894841775 * c.a - 1.2914855480 * c.b;
    const qreal l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    return { linearToSrgb(4.0767416621 * l - 3.3077115913 * m + 0.2307590544 * s),
             linearToSrgb(-1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s),
             linearToSrgb(-0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s) };
}

qreal applyLevels(qreal v, qreal sIn, qreal hIn, qreal sOut, qreal hOut, qreal mid)
{
    qreal range = hIn - sIn;
    if (range <= 0.0)
        range = 1.0 / 255.0;
    v = qBound(0.0, (v - sIn) / range, 1.0);
    if (mid > 0.0 && !qFuzzyCompare(mid, 1.0))
        v = qPow(v, 1.0 / mid);
    return sOut + (hOut - sOut) * v;
}

} // namespace

namespace QtPsdGui {

bool applyAdjustmentToImage(QImage &image, const QPsdAdjustmentLayerItem *layer,
                            const QImage &weightMask)
{
    if (!layer || image.isNull())
        return false;

    const QByteArray key = layer->adjustmentKey();
    const auto ali = layer->record().additionalLayerInformation();
    // ALI data may be a QVariantMap (custom plugins) or QPsdDescriptor (v16descriptor plugin)
    QVariantMap data = ali.value(key).toMap();
    if (data.isEmpty() && ali.value(key).canConvert<QPsdDescriptor>()) {
        const auto desc = ali.value(key).value<QPsdDescriptor>();
        const auto hash = desc.data();
        for (auto it = hash.cbegin(); it != hash.cend(); ++it)
            data.insert(QString::fromLatin1(it.key()), it.value());
    }

    std::function<Rgb(const Rgb &)> transform;

    if (key == "brit") {
        // Legacy brightness adds the raw value; modern (non-legacy) parameters
        // live in the CgEd descriptor and apply contrast before brightness
        qreal brightness = data.value(u"brightness"_s).toDouble() / 255.0;
        qreal contrast = data.value(u"contrast"_s).toDouble() / 100.0;
        qreal pivot = 0.5;
        bool modern = false;
        const QVariant cged = ali.value("CgEd");
        if (cged.canConvert<QPsdDescriptor>()) {
            const auto hash = cged.value<QPsdDescriptor>().data();
            if (hash.contains("Brgh") && !hash.value("useLegacy").toBool()) {
                modern = true;
                brightness = hash.value("Brgh").toDouble() / 255.0;
                contrast = hash.value("Cntr").toDouble() / 100.0;
                pivot = hash.value("means", 127.5).toDouble() / 255.0;
            }
        }
        transform = [=](const Rgb &in) -> Rgb {
            auto applyContrast = [&](qreal v) {
                if (contrast > 0.0)
                    return qBound(0.0, (v - pivot) / (1.0 - qMin(contrast, 0.999)) + pivot, 1.0);
                if (contrast < 0.0)
                    return qBound(0.0, (v - pivot) * (1.0 + contrast) + pivot, 1.0);
                return v;
            };
            auto applyBrightness = [&](qreal v) {
                return qBound(0.0, v + brightness, 1.0);
            };
            auto apply = [&](qreal v) {
                return modern ? applyBrightness(applyContrast(v))
                              : applyContrast(applyBrightness(v));
            };
            return {apply(in.r), apply(in.g), apply(in.b)};
        };
    } else if (key == "levl") {
        struct Ch { qreal sIn, hIn, sOut, hOut, mid; };
        auto channel = [&](const QString &name) -> Ch {
            const auto ch = data.value(name).toMap();
            return { ch.value(u"shadowInput"_s).toDouble() / 255.0,
                     ch.value(u"highlightInput"_s).toDouble() / 255.0,
                     ch.value(u"shadowOutput"_s).toDouble() / 255.0,
                     ch.value(u"highlightOutput"_s).toDouble() / 255.0,
                     ch.value(u"midtoneInput"_s, 100).toDouble() / 100.0 };
        };
        const Ch master = channel(u"rgb"_s);
        const Ch red = channel(u"red"_s);
        const Ch green = channel(u"green"_s);
        const Ch blue = channel(u"blue"_s);
        transform = [=](const Rgb &in) -> Rgb {
            Rgb c { applyLevels(in.r, red.sIn, red.hIn, red.sOut, red.hOut, red.mid),
                    applyLevels(in.g, green.sIn, green.hIn, green.sOut, green.hOut, green.mid),
                    applyLevels(in.b, blue.sIn, blue.hIn, blue.sOut, blue.hOut, blue.mid) };
            return { applyLevels(c.r, master.sIn, master.hIn, master.sOut, master.hOut, master.mid),
                     applyLevels(c.g, master.sIn, master.hIn, master.sOut, master.hOut, master.mid),
                     applyLevels(c.b, master.sIn, master.hIn, master.sOut, master.hOut, master.mid) };
        };
    } else if (key == "curv") {
        const auto rgbCurve = curveLut(data.value(u"rgb"_s).toList());
        const auto redCurve = curveLut(data.value(u"red"_s).toList());
        const auto greenCurve = curveLut(data.value(u"green"_s).toList());
        const auto blueCurve = curveLut(data.value(u"blue"_s).toList());
        transform = [=](const Rgb &in) -> Rgb {
            auto idx = [](qreal v) { return qBound(0, qRound(v * 255.0), 255); };
            return { redCurve[rgbCurve[idx(in.r)]] / 255.0,
                     greenCurve[rgbCurve[idx(in.g)]] / 255.0,
                     blueCurve[rgbCurve[idx(in.b)]] / 255.0 };
        };
    } else if (key == "expA") {
        // Exposure operates in linear light, not in the sRGB-encoded values
        const qreal exposure = data.value(u"exposure"_s).toDouble();
        const qreal offset = data.value(u"offset"_s).toDouble();
        const qreal gamma = data.value(u"gamma"_s, 1.0).toDouble();
        transform = [=](const Rgb &in) -> Rgb {
            const qreal mul = qPow(2.0, exposure);
            auto srgb2lin = [](qreal c) {
                return c <= 0.04045 ? c / 12.92 : qPow((c + 0.055) / 1.055, 2.4);
            };
            auto lin2srgb = [](qreal c) {
                return c <= 0.0031308 ? c * 12.92 : 1.055 * qPow(c, 1.0 / 2.4) - 0.055;
            };
            auto apply = [&](qreal v) {
                qreal lin = srgb2lin(v) * mul + offset;
                lin = qPow(qMax(0.0, lin), 1.0 / (gamma > 0.0 ? gamma : 1.0));
                return qBound(0.0, lin2srgb(lin), 1.0);
            };
            return {apply(in.r), apply(in.g), apply(in.b)};
        };
    } else if (key == "hue2") {
        const auto master = data.value(u"master"_s).toMap();
        const qreal hueShift = master.value(u"hue"_s).toDouble();
        const qreal satShift = master.value(u"saturation"_s).toDouble();
        const qreal lightShift = master.value(u"lightness"_s).toDouble();
        transform = [=](const Rgb &in) -> Rgb {
            Hsl hsl = rgb2hsl(in);
            hsl.h = hsl.h + hueShift / 360.0;
            hsl.h -= qFloor(hsl.h);
            hsl.s = qBound(0.0, hsl.s + satShift / 100.0, 1.0);
            hsl.l = qBound(0.0, hsl.l + lightShift / 100.0, 1.0);
            return hsl2rgb(hsl);
        };
    } else if (key == "blnc") {
        const auto shadows = data.value(u"shadows"_s).toMap();
        const auto midtones = data.value(u"midtones"_s).toMap();
        const auto highlights = data.value(u"highlights"_s).toMap();
        auto triple = [](const QVariantMap &m) -> Rgb {
            return { m.value(u"cyanRed"_s).toDouble() / 100.0,
                     m.value(u"magentaGreen"_s).toDouble() / 100.0,
                     m.value(u"yellowBlue"_s).toDouble() / 100.0 };
        };
        const Rgb sh = triple(shadows);
        const Rgb mid = triple(midtones);
        const Rgb hi = triple(highlights);
        const bool preserveLum = data.value(u"preserveLuminosity"_s).toBool();
        transform = [=](const Rgb &in) -> Rgb {
            const qreal l = lum(in);
            const qreal shadowW = qBound(0.0, 1.0 - l / 0.5, 1.0);
            const qreal highW = qBound(0.0, (l - 0.5) / 0.5, 1.0);
            const qreal midW = 1.0 - shadowW - highW;
            Rgb c { qBound(0.0, in.r + shadowW * sh.r + midW * mid.r + highW * hi.r, 1.0),
                    qBound(0.0, in.g + shadowW * sh.g + midW * mid.g + highW * hi.g, 1.0),
                    qBound(0.0, in.b + shadowW * sh.b + midW * mid.b + highW * hi.b, 1.0) };
            if (preserveLum) {
                const qreal newL = lum(c);
                if (newL > 0.0) {
                    const qreal f = l / newL;
                    c = { qBound(0.0, c.r * f, 1.0), qBound(0.0, c.g * f, 1.0), qBound(0.0, c.b * f, 1.0) };
                }
            }
            return c;
        };
    } else if (key == "phfl") {
        QColor filterColor = data.value(u"color"_s).value<QColor>();
        if (!filterColor.isValid())
            filterColor = QColor(236, 138, 0); // Warming Filter (85)
        const qreal density = data.value(u"density"_s).toDouble() / 100.0;
        const bool preserveLum = data.value(u"preserveLuminosity"_s).toBool();
        auto srgb2lin = [](qreal c) {
            return c <= 0.04045 ? c / 12.92 : qPow((c + 0.055) / 1.055, 2.4);
        };
        auto lin2srgb = [](qreal c) {
            c = qBound(0.0, c, 1.0);
            return c <= 0.0031308 ? c * 12.92 : 1.055 * qPow(c, 1.0 / 2.4) - 0.055;
        };
        const Rgb fl { srgb2lin(filterColor.redF()), srgb2lin(filterColor.greenF()),
                       srgb2lin(filterColor.blueF()) };
        // Multiply with the filter color in linear light, mix at density,
        // then restore the original luminosity (calibrated against
        // Photoshop output)
        transform = [=](const Rgb &in) -> Rgb {
            auto apply = [&](qreal v, qreal f) {
                const qreal lin = srgb2lin(v);
                return lin2srgb(lin + (lin * f - lin) * density);
            };
            Rgb c { apply(in.r, fl.r), apply(in.g, fl.g), apply(in.b, fl.b) };
            if (preserveLum) {
                const qreal newLum = lum(c);
                if (newLum > 0.0) {
                    const qreal f = lum(in) / newLum;
                    c = {c.r * f, c.g * f, c.b * f};
                }
            }
            return { qBound(0.0, c.r, 1.0), qBound(0.0, c.g, 1.0), qBound(0.0, c.b, 1.0) };
        };
    } else if (key == "nvrt") {
        transform = [](const Rgb &in) -> Rgb {
            return {1.0 - in.r, 1.0 - in.g, 1.0 - in.b};
        };
    } else if (key == "post") {
        const int postLevels = ali.value(key).toInt();
        const qreal levels = qMax(2.0, qreal(postLevels > 1 ? postLevels : 4));
        transform = [=](const Rgb &in) -> Rgb {
            auto apply = [&](qreal v) {
                return qBound(0.0, qFloor(v * levels) / (levels - 1.0), 1.0);
            };
            return {apply(in.r), apply(in.g), apply(in.b)};
        };
    } else if (key == "thrs") {
        const int level = ali.value(key).toInt();
        const qreal threshold = (level > 0 ? level : 128) / 255.0;
        transform = [=](const Rgb &in) -> Rgb {
            const qreal v = lum(in) >= threshold - 0.5 / 255.0 ? 1.0 : 0.0;
            return {v, v, v};
        };
    } else if (key == "vibA") {
        const qreal vibrance = data.value(u"vibrance"_s).toDouble() / 100.0;
        const qreal vibranceSat = data.value(u"Strt"_s).toDouble() / 100.0;
        // Calibrated against Photoshop output: push each channel away from
        // the max channel by the squared distance (protects already-saturated
        // colors less aggressively than a linear model), then apply the
        // saturation slider as a gentle spread around the channel average
        transform = [=](const Rgb &in) -> Rgb {
            const qreal mx = qMax(in.r, qMax(in.g, in.b));
            auto push = [&](qreal v) {
                const qreal d = mx - v;
                return v - d * d * 1.5 * vibrance;
            };
            const Rgb c { push(in.r), push(in.g), push(in.b) };
            const qreal avg = (c.r + c.g + c.b) / 3.0;
            auto spread = [&](qreal v) {
                return qBound(0.0, avg + (v - avg) * (1.0 + 0.5 * vibranceSat), 1.0);
            };
            return { spread(c.r), spread(c.g), spread(c.b) };
        };
    } else if (key == "mixr") {
        const bool mono = data.value(u"monochrome"_s).toBool();
        const auto red = data.value(u"red"_s).toMap();
        const auto green = data.value(u"green"_s).toMap();
        const auto blue = data.value(u"blue"_s).toMap();
        const auto src = mono ? data.value(u"gray"_s).toMap() : red;
        auto coeffs = [](const QVariantMap &m) -> std::array<qreal, 4> {
            return { m.value(u"red"_s).toDouble() / 100.0,
                     m.value(u"green"_s).toDouble() / 100.0,
                     m.value(u"blue"_s).toDouble() / 100.0,
                     m.value(u"constant"_s).toDouble() / 100.0 };
        };
        const auto cr = coeffs(src);
        const auto cg = coeffs(green);
        const auto cb = coeffs(blue);
        transform = [=](const Rgb &in) -> Rgb {
            auto mix = [&](const std::array<qreal, 4> &c) {
                return qBound(0.0, c[0] * in.r + c[1] * in.g + c[2] * in.b + c[3], 1.0);
            };
            if (mono) {
                const qreal g = mix(cr);
                return {g, g, g};
            }
            return {mix(cr), mix(cg), mix(cb)};
        };
    } else if (key == "blwh") {
        const qreal wr = data.value(u"Rd  "_s).toDouble();
        const qreal wy = data.value(u"Yllw"_s).toDouble();
        const qreal wg = data.value(u"Grn "_s).toDouble();
        const qreal wc = data.value(u"Cyn "_s).toDouble();
        const qreal wb = data.value(u"Bl  "_s).toDouble();
        const qreal wm = data.value(u"Mgnt"_s).toDouble();
        transform = [=](const Rgb &in) -> Rgb {
            const Hsl hsl = rgb2hsl(in);
            const qreal h = hsl.h * 360.0;
            const qreal rW = qMax(0.0, 1.0 - qMin(qAbs(h), qAbs(h - 360.0)) / 60.0);
            const qreal yW = qMax(0.0, 1.0 - qAbs(h - 60.0) / 60.0);
            const qreal gW = qMax(0.0, 1.0 - qAbs(h - 120.0) / 60.0);
            const qreal cW = qMax(0.0, 1.0 - qAbs(h - 180.0) / 60.0);
            const qreal bW = qMax(0.0, 1.0 - qAbs(h - 240.0) / 60.0);
            const qreal mW = qMax(0.0, 1.0 - qAbs(h - 300.0) / 60.0);
            const qreal totalW = rW + yW + gW + cW + bW + mW;
            // The neutral point is the channel average, not Rec.601 luma
            // (calibrated against Photoshop output)
            const qreal avg = (in.r + in.g + in.b) / 3.0;
            qreal gray = avg;
            if (totalW > 0.0) {
                const qreal mixFactor = (rW * wr + yW * wy + gW * wg + cW * wc + bW * wb + mW * wm)
                    / (totalW * 100.0);
                gray = qBound(0.0, avg + (mixFactor - 0.5) * hsl.s, 1.0);
            }
            return {gray, gray, gray};
        };
    } else if (key == "grdm") {
        const QList<QRgb> rgbLut = gradientMapLut(data);
        QVector<Rgb> lut(256);
        for (int i = 0; i < 256; ++i)
            lut[i] = { qRed(rgbLut[i]) / 255.0, qGreen(rgbLut[i]) / 255.0, qBlue(rgbLut[i]) / 255.0 };
        transform = [lut](const Rgb &in) -> Rgb {
            return lut[qBound(0, qRound(lum(in) * 255.0), 255)];
        };
    }

    if (!transform)
        return false;

    image = image.convertToFormat(QImage::Format_ARGB32);
    const qreal layerOpacity = layer->opacity();
    const bool hasMask = !weightMask.isNull();

    for (int y = 0; y < image.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        const uchar *maskLine = hasMask && y < weightMask.height() ? weightMask.constScanLine(y) : nullptr;
        for (int x = 0; x < image.width(); ++x) {
            qreal w = layerOpacity;
            if (maskLine && x < weightMask.width())
                w *= maskLine[x] / 255.0;
            if (w <= 0.0)
                continue;
            const QRgb p = line[x];
            const Rgb in { qRed(p) / 255.0, qGreen(p) / 255.0, qBlue(p) / 255.0 };
            const Rgb out = transform(in);
            const Rgb mixed { in.r + (out.r - in.r) * w,
                              in.g + (out.g - in.g) * w,
                              in.b + (out.b - in.b) * w };
            line[x] = qRgba(qBound(0, qRound(mixed.r * 255.0), 255),
                            qBound(0, qRound(mixed.g * 255.0), 255),
                            qBound(0, qRound(mixed.b * 255.0), 255),
                            qAlpha(p));
        }
    }
    return true;
}

namespace {

QImage buildDocumentWeightMask(const QPsdAdjustmentLayerItem *layer, const QSize &canvasSize)
{
    const QImage mask = layer->layerMask();
    const QRect maskRect = layer->layerMaskRect();
    const quint8 defaultColor = layer->layerMaskDefaultColor();
    const qreal density = layer->layerMaskDensity() / 255.0;
    const bool hasMaskImage = !mask.isNull() && maskRect.isValid() && !maskRect.isEmpty();

    if (!hasMaskImage && defaultColor == 255)
        return QImage(); // full weight everywhere

    QImage doc(canvasSize, QImage::Format_Grayscale8);
    doc.fill(defaultColor);
    if (hasMaskImage) {
        QPainter p(&doc);
        p.drawImage(maskRect.topLeft(), mask.convertToFormat(QImage::Format_Grayscale8));
    }
    if (density < 1.0) {
        // Reduced mask density fades the mask toward "no mask" (full weight)
        for (int y = 0; y < doc.height(); ++y) {
            uchar *line = doc.scanLine(y);
            for (int x = 0; x < doc.width(); ++x)
                line[x] = 255 - qRound(density * (255 - line[x]));
        }
    }
    return doc;
}

// Coverage of the clipping-mask base layer (the layer the adjustment is
// clipped to): the adjustment only affects pixels the base covers
QImage clipBaseCoverage(const QPsdAbstractLayerItem *base, const QSize &canvasSize)
{
    QImage doc(canvasSize, QImage::Format_Grayscale8);
    doc.fill(0);
    QPainter p(&doc);
    const QRect rect = base->rect();

    QPsdAbstractLayerItem::PathInfo pathInfo;
    if (base->type() == QPsdAbstractLayerItem::Shape)
        pathInfo = reinterpret_cast<const QPsdShapeLayerItem *>(base)->pathInfo();
    else
        pathInfo = base->vectorMask();

    if (pathInfo.type != QPsdAbstractLayerItem::PathInfo::None) {
        QPainterPath path;
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::Rectangle:
            path.addRect(pathInfo.rect);
            break;
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
            path.addRoundedRect(pathInfo.rect, pathInfo.radius, pathInfo.radius);
            break;
        default:
            path = pathInfo.path;
            break;
        }
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(rect.topLeft());
        p.fillPath(path, Qt::white);
        return doc;
    }

    const QImage transparency = base->transparencyMask();
    if (!transparency.isNull()) {
        p.drawImage(rect.topLeft(), transparency);
        return doc;
    }

    if (base->type() == QPsdAbstractLayerItem::Image) {
        const QImage image = reinterpret_cast<const QPsdImageLayerItem *>(base)->image();
        if (!image.isNull() && image.hasAlphaChannel()) {
            const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
            QImage alpha(argb.size(), QImage::Format_Grayscale8);
            for (int y = 0; y < argb.height(); ++y) {
                const QRgb *src = reinterpret_cast<const QRgb *>(argb.constScanLine(y));
                uchar *dst = alpha.scanLine(y);
                for (int x = 0; x < argb.width(); ++x)
                    dst[x] = qAlpha(src[x]);
            }
            p.drawImage(rect.topLeft(), alpha);
            return doc;
        }
    }

    p.fillRect(rect, Qt::white);
    return doc;
}

} // namespace

QList<quint8> curveLut(const QVariantList &points)
{
    QList<quint8> table(256);
    if (points.isEmpty()) {
        for (int i = 0; i < 256; ++i)
            table[i] = i;
        return table;
    }
    QVector<QPair<int, int>> pts;
    for (const auto &p : points) {
        const auto m = p.toMap();
        pts.append({m.value(u"input"_s).toInt(), m.value(u"output"_s).toInt()});
    }
    std::sort(pts.begin(), pts.end());
    if (pts.first().first != 0)
        pts.prepend({0, pts.first().second});
    if (pts.last().first != 255)
        pts.append({255, pts.last().second});

    const int n = pts.size();
    // Natural cubic spline through the points (matches Photoshop's Curves)
    QVector<double> m(n, 0.0); // second derivatives; natural ends stay 0
    if (n > 2) {
        QVector<double> h(n - 1), alpha(n), l(n), mu(n), z(n);
        for (int i = 0; i < n - 1; ++i)
            h[i] = qMax(1, pts[i + 1].first - pts[i].first);
        for (int i = 1; i < n - 1; ++i)
            alpha[i] = 6.0 * ((pts[i + 1].second - pts[i].second) / h[i]
                              - (pts[i].second - pts[i - 1].second) / h[i - 1]);
        l[0] = 1.0; mu[0] = 0.0; z[0] = 0.0;
        for (int i = 1; i < n - 1; ++i) {
            l[i] = 2.0 * (pts[i + 1].first - pts[i - 1].first) - h[i - 1] * mu[i - 1];
            mu[i] = h[i] / l[i];
            z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
        }
        for (int i = n - 2; i >= 1; --i)
            m[i] = z[i] - mu[i] * m[i + 1];
    }

    int seg = 0;
    for (int i = 0; i < 256; ++i) {
        while (seg < n - 2 && i > pts[seg + 1].first)
            ++seg;
        const double x0 = pts[seg].first, y0 = pts[seg].second;
        const double x1 = pts[seg + 1].first, y1 = pts[seg + 1].second;
        const double h = qMax(1.0, x1 - x0);
        const double a = x1 - i, b = i - x0;
        const double v = m[seg] / (6.0 * h) * a * a * a
            + m[seg + 1] / (6.0 * h) * b * b * b
            + (y0 / h - m[seg] * h / 6.0) * a
            + (y1 / h - m[seg + 1] * h / 6.0) * b;
        table[i] = qBound(0, qRound(v), 255);
    }
    return table;
}

QList<QRgb> gradientMapLut(const QVariantMap &grdmData)
{
    QList<QRgb> lut(256, qRgb(0, 0, 0));
    struct Stop { double pos; QColor color; };
    QVector<Stop> gradStops;
    const auto stops = grdmData.value(u"colorStops"_s).toList();
    for (const auto &s : stops) {
        const auto m = s.toMap();
        const double loc = m.value(u"location"_s).toDouble() / 4096.0;
        const QColor c = m.value(u"color"_s).value<QColor>();
        if (c.isValid())
            gradStops.append({loc, c});
    }
    if (gradStops.isEmpty())
        return lut;

    const QString method = grdmData.value(u"method"_s).toString();
    const bool perceptual = method == "Smoo"_L1 || method == "Perc"_L1;
    const bool linear = method == "Lnr "_L1;

    for (int i = 0; i < 256; ++i) {
        const double t = i / 255.0;
        int idx = 0;
        while (idx < gradStops.size() - 1 && gradStops[idx + 1].pos < t)
            ++idx;
        Rgb out;
        if (idx >= gradStops.size() - 1) {
            const QColor &c = gradStops.last().color;
            out = {c.redF(), c.greenF(), c.blueF()};
        } else {
            const double t0 = gradStops[idx].pos, t1 = gradStops[idx + 1].pos;
            const double f = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0;
            const QColor &q0 = gradStops[idx].color;
            const QColor &q1 = gradStops[idx + 1].color;
            const Rgb c0 {q0.redF(), q0.greenF(), q0.blueF()};
            const Rgb c1 {q1.redF(), q1.greenF(), q1.blueF()};
            if (perceptual) {
                const Oklab l0 = srgbToOklab(c0);
                const Oklab l1 = srgbToOklab(c1);
                out = oklabToSrgb({ l0.L + f * (l1.L - l0.L),
                                    l0.a + f * (l1.a - l0.a),
                                    l0.b + f * (l1.b - l0.b) });
            } else if (linear) {
                auto lerpLin = [&](qreal a, qreal b) {
                    const qreal la = srgbToLinear(a), lb = srgbToLinear(b);
                    return linearToSrgb(la + f * (lb - la));
                };
                out = { lerpLin(c0.r, c1.r), lerpLin(c0.g, c1.g), lerpLin(c0.b, c1.b) };
            } else {
                out = { c0.r + f * (c1.r - c0.r),
                        c0.g + f * (c1.g - c0.g),
                        c0.b + f * (c1.b - c0.b) };
            }
        }
        lut[i] = qRgb(qBound(0, qRound(out.r * 255.0), 255),
                      qBound(0, qRound(out.g * 255.0), 255),
                      qBound(0, qRound(out.b * 255.0), 255));
    }
    return lut;
}

QImage adjustmentWeightMask(const QPsdAdjustmentLayerItem *layer,
                            const QPsdAbstractLayerItem *clipBase,
                            const QSize &canvasSize)
{
    QImage weight = buildDocumentWeightMask(layer, canvasSize);
    if (!clipBase)
        return weight;
    const QImage coverage = clipBaseCoverage(clipBase, canvasSize);
    if (weight.isNull())
        return coverage;
    for (int y = 0; y < weight.height(); ++y) {
        uchar *w = weight.scanLine(y);
        const uchar *c = coverage.constScanLine(y);
        for (int x = 0; x < weight.width(); ++x)
            w[x] = (w[x] * c[x]) / 255;
    }
    return weight;
}

} // namespace QtPsdGui

QT_END_NAMESPACE
