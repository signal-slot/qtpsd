// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdadjustments.h"
#include "qpsdadjustmentlayeritem.h"

#include <QtPsdCore/qpsddescriptor.h>

#include <QtCore/QtMath>
#include <QtGui/QColor>

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

QVector<quint8> buildCurveTable(const QVariantList &points)
{
    QVector<quint8> table(256);
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
    if (pts.first().first != 0)
        pts.prepend({0, pts.first().second});
    if (pts.last().first != 255)
        pts.append({255, pts.last().second});
    int seg = 0;
    for (int i = 0; i < 256; ++i) {
        while (seg < pts.size() - 2 && i > pts[seg + 1].first)
            ++seg;
        const int x0 = pts[seg].first, y0 = pts[seg].second;
        const int x1 = pts[seg + 1].first, y1 = pts[seg + 1].second;
        const double t = (x1 != x0) ? double(i - x0) / (x1 - x0) : 0.0;
        table[i] = qBound(0, qRound(y0 + t * (y1 - y0)), 255);
    }
    return table;
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
        const auto rgbCurve = buildCurveTable(data.value(u"rgb"_s).toList());
        const auto redCurve = buildCurveTable(data.value(u"red"_s).toList());
        const auto greenCurve = buildCurveTable(data.value(u"green"_s).toList());
        const auto blueCurve = buildCurveTable(data.value(u"blue"_s).toList());
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
            filterColor = QColor(255, 147, 0);
        const Rgb fc { filterColor.redF(), filterColor.greenF(), filterColor.blueF() };
        const qreal density = data.value(u"density"_s).toDouble() / 100.0;
        const bool preserveLum = data.value(u"preserveLuminosity"_s).toBool();
        transform = [=](const Rgb &in) -> Rgb {
            Rgb c { in.r + (in.r * fc.r - in.r) * density,
                    in.g + (in.g * fc.g - in.g) * density,
                    in.b + (in.b * fc.b - in.b) * density };
            if (preserveLum) {
                const qreal origLum = lum(in);
                const qreal newLum = lum(c);
                if (newLum > 0.0) {
                    const qreal f = origLum / newLum;
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
        const qreal vibranceSat = data.value(u"Strt"_s).toDouble();
        transform = [=](const Rgb &in) -> Rgb {
            Hsl hsl = rgb2hsl(in);
            hsl.s = qBound(0.0, hsl.s + vibrance * (1.0 - hsl.s), 1.0);
            hsl.s = qBound(0.0, hsl.s + vibranceSat / 100.0, 1.0);
            return hsl2rgb(hsl);
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
            qreal gray;
            if (totalW > 0.0) {
                const qreal mixFactor = (rW * wr + yW * wy + gW * wg + cW * wc + bW * wb + mW * wm)
                    / (totalW * 100.0);
                gray = qBound(0.0, lum(in) + (mixFactor - 0.5) * hsl.s, 1.0);
            } else {
                gray = lum(in);
            }
            return {gray, gray, gray};
        };
    } else if (key == "grdm") {
        QVector<Rgb> lut(256, {0.0, 0.0, 0.0});
        const auto stops = data.value(u"colorStops"_s).toList();
        struct Stop { double pos; QColor color; };
        QVector<Stop> gradStops;
        for (const auto &s : stops) {
            const auto m = s.toMap();
            const double loc = m.value(u"location"_s).toDouble() / 4096.0;
            const QColor c = m.value(u"color"_s).value<QColor>();
            if (c.isValid())
                gradStops.append({loc, c});
        }
        if (!gradStops.isEmpty()) {
            for (int i = 0; i < 256; ++i) {
                const double t = i / 255.0;
                int idx = 0;
                while (idx < gradStops.size() - 1 && gradStops[idx + 1].pos < t)
                    ++idx;
                QColor c;
                if (idx >= gradStops.size() - 1) {
                    c = gradStops.last().color;
                } else {
                    const double t0 = gradStops[idx].pos, t1 = gradStops[idx + 1].pos;
                    const double f = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0;
                    const auto &c0 = gradStops[idx].color;
                    const auto &c1 = gradStops[idx + 1].color;
                    c = QColor::fromRgbF(c0.redF() + f * (c1.redF() - c0.redF()),
                                         c0.greenF() + f * (c1.greenF() - c0.greenF()),
                                         c0.blueF() + f * (c1.blueF() - c0.blueF()));
                }
                lut[i] = {c.redF(), c.greenF(), c.blueF()};
            }
        }
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

} // namespace QtPsdGui

QT_END_NAMESPACE
