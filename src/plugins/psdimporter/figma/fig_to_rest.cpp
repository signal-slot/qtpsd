// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "fig_to_rest.h"

#include <QtCore/QCborArray>
#include <QtCore/QCborMap>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRectF>
#include <QtGui/QTransform>

#include <cmath>
#include <cstring>

using namespace Qt::StringLiterals;

namespace FigToRest {

namespace {

// ---------------- helpers ----------------

static float readF32LE(const uchar *p)
{
    quint32 b = quint32(p[0]) | (quint32(p[1]) << 8)
              | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
    float f;
    std::memcpy(&f, &b, sizeof(f));
    return f;
}

static QString guidToStr(const QCborValue &v)
{
    const auto m = v.toMap();
    return QStringLiteral("%1:%2")
        .arg(m.value(QStringLiteral("sessionID")).toInteger())
        .arg(m.value(QStringLiteral("localID")).toInteger());
}

static QJsonValue toJson(const QCborValue &v);

static QJsonObject toJsonObj(const QCborMap &m)
{
    QJsonObject out;
    for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
        out.insert(it.key().toString(), toJson(it.value()));
    }
    return out;
}

static QJsonArray toJsonArr(const QCborArray &a)
{
    QJsonArray out;
    for (const auto &v : a)
        out.append(toJson(v));
    return out;
}

static QJsonValue toJson(const QCborValue &v)
{
    if (v.isBool()) return QJsonValue(v.toBool());
    if (v.isInteger()) return QJsonValue(double(v.toInteger()));
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (!std::isfinite(d)) return QJsonValue::Null;
        return QJsonValue(d);
    }
    if (v.isString()) return QJsonValue(v.toString());
    if (v.isByteArray()) return QJsonValue(QString::fromLatin1(v.toByteArray().toBase64()));
    if (v.isMap()) return toJsonObj(v.toMap());
    if (v.isArray()) return toJsonArr(v.toArray());
    return QJsonValue::Null;
}

// Sanitize a double: NaN/Inf → 0 (suitable for QJsonValue which forbids NaN).
static double finite(double d, double fallback = 0.0)
{
    return std::isfinite(d) ? d : fallback;
}

// Shorthand for common patterns — safely pull a finite double from a CBOR value.
static double fdouble(const QCborValue &v, double fallback = 0.0)
{
    return finite(v.toDouble(fallback), fallback);
}

// ---------------- commandsBlob parser ----------------

static QString svgFormat(qreal v)
{
    // Short, stable representation — keep 2 decimals, strip trailing zeros.
    QString s = QString::number(v, 'f', 2);
    while (s.endsWith(u'0')) s.chop(1);
    if (s.endsWith(u'.')) s.chop(1);
    if (s == "-0"_L1) s = "0"_L1;
    return s;
}

} // anon

QString commandsBlobToSvgPath(const QByteArray &bytes)
{
    QString out;
    const uchar *p = reinterpret_cast<const uchar *>(bytes.constData());
    const qsizetype n = bytes.size();
    qsizetype off = 0;
    while (off < n) {
        const uchar op = p[off++];
        switch (op) {
        case 0: // Z
            out.append(u'Z');
            break;
        case 1: // M
            if (off + 8 > n) return {};
            out += u'M' + svgFormat(readF32LE(p + off)) + u' ' + svgFormat(readF32LE(p + off + 4));
            off += 8;
            break;
        case 2: // L
            if (off + 8 > n) return {};
            out += u'L' + svgFormat(readF32LE(p + off)) + u' ' + svgFormat(readF32LE(p + off + 4));
            off += 8;
            break;
        case 3: // Q
            if (off + 16 > n) return {};
            out += u'Q' + svgFormat(readF32LE(p + off)) + u' ' + svgFormat(readF32LE(p + off + 4))
                 + u' ' + svgFormat(readF32LE(p + off + 8)) + u' ' + svgFormat(readF32LE(p + off + 12));
            off += 16;
            break;
        case 4: // C
            if (off + 24 > n) return {};
            out += u'C' + svgFormat(readF32LE(p + off)) + u' ' + svgFormat(readF32LE(p + off + 4))
                 + u' ' + svgFormat(readF32LE(p + off + 8)) + u' ' + svgFormat(readF32LE(p + off + 12))
                 + u' ' + svgFormat(readF32LE(p + off + 16)) + u' ' + svgFormat(readF32LE(p + off + 20));
            off += 24;
            break;
        default:
            return {}; // unknown opcode
        }
    }
    return out;
}

namespace {

// ---------------- fill conversion ----------------

static QJsonObject convertPaint(const QCborMap &src)
{
    QJsonObject dst;
    const QString type = src.value(QStringLiteral("type")).toString();
    dst.insert("type"_L1, type);
    dst.insert("visible"_L1, src.value(QStringLiteral("visible")).toBool(true));
    dst.insert("opacity"_L1, fdouble(src.value(QStringLiteral("opacity")), 1.0));
    const auto blendMode = src.value(QStringLiteral("blendMode")).toString();
    if (!blendMode.isEmpty())
        dst.insert("blendMode"_L1, blendMode);

    if (type == "SOLID"_L1) {
        const auto c = src.value(QStringLiteral("color")).toMap();
        QJsonObject color;
        color.insert("r"_L1, fdouble(c.value(QStringLiteral("r"))));
        color.insert("g"_L1, fdouble(c.value(QStringLiteral("g"))));
        color.insert("b"_L1, fdouble(c.value(QStringLiteral("b"))));
        color.insert("a"_L1, fdouble(c.value(QStringLiteral("a")), 1.0));
        dst.insert("color"_L1, color);
    } else if (type.startsWith("GRADIENT_"_L1)) {
        const auto handles = src.value(QStringLiteral("gradientHandles")).toArray();
        QJsonArray h;
        for (const auto &pt : handles) {
            const auto pm = pt.toMap();
            QJsonObject o;
            o.insert("x"_L1, fdouble(pm.value(QStringLiteral("x"))));
            o.insert("y"_L1, fdouble(pm.value(QStringLiteral("y"))));
            h.append(o);
        }
        dst.insert("gradientHandlePositions"_L1, h);
        const auto stops = src.value(QStringLiteral("stops")).toArray();
        QJsonArray s;
        for (const auto &st : stops) {
            const auto stm = st.toMap();
            QJsonObject o;
            o.insert("position"_L1, fdouble(stm.value(QStringLiteral("position"))));
            const auto cc = stm.value(QStringLiteral("color")).toMap();
            QJsonObject color;
            color.insert("r"_L1, fdouble(cc.value(QStringLiteral("r"))));
            color.insert("g"_L1, fdouble(cc.value(QStringLiteral("g"))));
            color.insert("b"_L1, fdouble(cc.value(QStringLiteral("b"))));
            color.insert("a"_L1, fdouble(cc.value(QStringLiteral("a")), 1.0));
            o.insert("color"_L1, color);
            s.append(o);
        }
        dst.insert("gradientStops"_L1, s);
    } else if (type == "IMAGE"_L1) {
        const auto image = src.value(QStringLiteral("image")).toMap();
        const auto ref = src.value(QStringLiteral("imageRef")).toString();
        if (!ref.isEmpty()) {
            dst.insert("imageRef"_L1, ref);
        } else {
            // kiwi may store image hash as bytes in paint.image.hash
            const auto hashBytes = image.value(QStringLiteral("hash")).toByteArray();
            if (!hashBytes.isEmpty())
                dst.insert("imageRef"_L1, QString::fromLatin1(hashBytes.toHex()));
        }
        const auto scaleMode = src.value(QStringLiteral("imageScaleMode")).toString();
        if (!scaleMode.isEmpty())
            dst.insert("scaleMode"_L1, scaleMode);
    }
    return dst;
}

static QJsonArray convertPaints(const QCborArray &src)
{
    QJsonArray out;
    for (const auto &v : src) {
        if (v.isMap())
            out.append(convertPaint(v.toMap()));
    }
    return out;
}

// ---------------- geometry ----------------

struct Geometry {
    QJsonArray fillArr;
    QJsonArray strokeArr;
};

static QJsonArray buildGeometryArray(const QCborArray &geoms, const QCborArray &blobs)
{
    QJsonArray out;
    for (const auto &gv : geoms) {
        if (!gv.isMap()) continue;
        const auto gm = gv.toMap();
        const auto blobIdx = gm.value(QStringLiteral("commandsBlob")).toInteger(-1);
        if (blobIdx < 0 || blobIdx >= blobs.size()) continue;
        const auto blobEntry = blobs.at(blobIdx).toMap();
        const QByteArray bytes = blobEntry.value(QStringLiteral("bytes")).toByteArray();
        const QString path = commandsBlobToSvgPath(bytes);
        if (path.isEmpty()) continue;
        QJsonObject entry;
        entry.insert("path"_L1, path);
        const auto windingRule = gm.value(QStringLiteral("windingRule")).toString();
        if (!windingRule.isEmpty())
            entry.insert("windingRule"_L1, windingRule);
        out.append(entry);
    }
    return out;
}

// ---------------- text ----------------

static QJsonValue textDataToCharacters(const QCborValue &textData)
{
    if (!textData.isMap()) return QJsonValue();
    return textData.toMap().value(QStringLiteral("characters")).toString();
}

static QJsonObject textStyle(const QCborMap &node)
{
    QJsonObject style;
    const auto fn = node.value(QStringLiteral("fontName")).toMap();
    const QString family = fn.value(QStringLiteral("family")).toString();
    const QString styleName = fn.value(QStringLiteral("style")).toString();
    if (!family.isEmpty()) style.insert("fontFamily"_L1, family);
    if (!styleName.isEmpty()) style.insert("fontPostScriptName"_L1, fn.value(QStringLiteral("postscript")).toString());
    // Infer weight / italic from style name loosely (matches REST style hints).
    const QString up = styleName.toUpper();
    int weight = 400;
    if (up.contains("THIN"_L1)) weight = 100;
    else if (up.contains("EXTRALIGHT"_L1) || up.contains("ULTRALIGHT"_L1)) weight = 200;
    else if (up.contains("LIGHT"_L1)) weight = 300;
    else if (up.contains("MEDIUM"_L1)) weight = 500;
    else if (up.contains("SEMIBOLD"_L1) || up.contains("DEMIBOLD"_L1)) weight = 600;
    else if (up.contains("EXTRABOLD"_L1) || up.contains("ULTRABOLD"_L1)) weight = 800;
    else if (up.contains("BLACK"_L1) || up.contains("HEAVY"_L1)) weight = 900;
    else if (up.contains("BOLD"_L1)) weight = 700;
    style.insert("fontWeight"_L1, weight);
    style.insert("italic"_L1, up.contains("ITALIC"_L1) || up.contains("OBLIQUE"_L1));

    style.insert("fontSize"_L1, node.value(QStringLiteral("fontSize")).toDouble(12.0));

    const auto lineHeight = node.value(QStringLiteral("lineHeight")).toMap();
    if (!lineHeight.isEmpty()) {
        const auto units = lineHeight.value(QStringLiteral("units")).toString();
        const double value = lineHeight.value(QStringLiteral("value")).toDouble();
        if (units == "PIXELS"_L1) {
            style.insert("lineHeightPx"_L1, value);
            style.insert("lineHeightUnit"_L1, QStringLiteral("PIXELS"));
        } else if (units == "PERCENT"_L1) {
            style.insert("lineHeightPercentFontSize"_L1, value);
            style.insert("lineHeightUnit"_L1, QStringLiteral("FONT_SIZE_%"));
        } else if (units == "RAW"_L1) {
            style.insert("lineHeightUnit"_L1, QStringLiteral("INTRINSIC_%"));
        }
    }

    const auto letterSpacing = node.value(QStringLiteral("letterSpacing")).toMap();
    if (!letterSpacing.isEmpty()) {
        const auto units = letterSpacing.value(QStringLiteral("units")).toString();
        const double value = letterSpacing.value(QStringLiteral("value")).toDouble();
        if (units == "PIXELS"_L1)
            style.insert("letterSpacing"_L1, value);
        else if (units == "PERCENT"_L1)
            style.insert("letterSpacing"_L1, value * node.value(QStringLiteral("fontSize")).toDouble() / 100.0);
    }

    const auto halign = node.value(QStringLiteral("textAlignHorizontal")).toString();
    if (!halign.isEmpty()) style.insert("textAlignHorizontal"_L1, halign);
    const auto valign = node.value(QStringLiteral("textAlignVertical")).toString();
    if (!valign.isEmpty()) style.insert("textAlignVertical"_L1, valign);
    const auto autoResize = node.value(QStringLiteral("textAutoResize")).toString();
    if (!autoResize.isEmpty()) style.insert("textAutoResize"_L1, autoResize);

    const auto textCase = node.value(QStringLiteral("textCase")).toString();
    if (!textCase.isEmpty()) style.insert("textCase"_L1, textCase);
    const auto textDecoration = node.value(QStringLiteral("textDecoration")).toString();
    if (!textDecoration.isEmpty()) style.insert("textDecoration"_L1, textDecoration);

    return style;
}

// ---------------- conversion driver ----------------

struct NodeRef {
    int index = -1; // position in nodeChanges
    QString guidStr;
    QString parentGuidStr;
    QString positionKey; // sort key within parent
};

struct BuildCtx
{
    QCborArray nodeChanges;
    QCborArray blobs;
    QHash<QString, int> nodeByGuidStr;
    QHash<QString, QVector<int>> childrenByParent; // parentGuidStr → ordered indices
};

// Per-node rendering context carried down the conversion recursion: the effective
// absolute transform of the current node's parent, and a unique guid that may be
// synthetic (instance-scoped) so repeated instances of the same symbol do not
// collide in the downstream flat id space.
struct NodeCtx
{
    QTransform parentAbs;
    QString displayIdPrefix; // empty for normal, "instGuid|" for cloned subtrees
};

static QJsonObject convertNode(const BuildCtx &ctx, int idx, const NodeCtx &nctx);

static QJsonArray buildChildren(const BuildCtx &ctx, const QString &parentGuidStr,
                                const NodeCtx &nctx)
{
    QJsonArray out;
    const auto it = ctx.childrenByParent.constFind(parentGuidStr);
    if (it == ctx.childrenByParent.cend()) return out;
    for (int childIdx : *it)
        out.append(convertNode(ctx, childIdx, nctx));
    return out;
}

static QTransform localTransformOf(const QCborMap &node)
{
    const auto t = node.value(QStringLiteral("transform")).toMap();
    const double m00 = finite(t.value(QStringLiteral("m00")).toDouble(1.0), 1.0);
    const double m01 = finite(t.value(QStringLiteral("m01")).toDouble(), 0.0);
    const double m02 = finite(t.value(QStringLiteral("m02")).toDouble(), 0.0);
    const double m10 = finite(t.value(QStringLiteral("m10")).toDouble(), 0.0);
    const double m11 = finite(t.value(QStringLiteral("m11")).toDouble(1.0), 1.0);
    const double m12 = finite(t.value(QStringLiteral("m12")).toDouble(), 0.0);
    return QTransform(m00, m10, m01, m11, m02, m12);
}

static QJsonObject convertNode(const BuildCtx &ctx, int idx, const NodeCtx &nctx)
{
    const auto node = ctx.nodeChanges.at(idx).toMap();
    const QString type = node.value(QStringLiteral("type")).toString();
    const QString rawGuid = guidToStr(node.value(QStringLiteral("guid")));
    const QString guidStr = nctx.displayIdPrefix.isEmpty()
        ? rawGuid
        : nctx.displayIdPrefix + rawGuid;

    const QTransform local = localTransformOf(node);
    const QTransform absX = local * nctx.parentAbs;

    QJsonObject out;
    out.insert("id"_L1, guidStr);
    out.insert("type"_L1, type);
    out.insert("name"_L1, node.value(QStringLiteral("name")).toString());
    out.insert("visible"_L1, node.value(QStringLiteral("visible")).toBool(true));
    out.insert("opacity"_L1, fdouble(node.value(QStringLiteral("opacity")), 1.0));
    const auto blendMode = node.value(QStringLiteral("blendMode")).toString();
    if (!blendMode.isEmpty()) out.insert("blendMode"_L1, blendMode);
    out.insert("isMask"_L1, node.value(QStringLiteral("mask")).toBool(false));

    // Rect from transform + size (absolute, axis-aligned bbox of 4 transformed corners).
    const auto size = node.value(QStringLiteral("size")).toMap();
    const double w = fdouble(size.value(QStringLiteral("x")));
    const double h = fdouble(size.value(QStringLiteral("y")));
    {
        const QPointF c1 = absX.map(QPointF(0, 0));
        const QPointF c2 = absX.map(QPointF(w, 0));
        const QPointF c3 = absX.map(QPointF(0, h));
        const QPointF c4 = absX.map(QPointF(w, h));
        const double minX = std::min({c1.x(), c2.x(), c3.x(), c4.x()});
        const double minY = std::min({c1.y(), c2.y(), c3.y(), c4.y()});
        const double maxX = std::max({c1.x(), c2.x(), c3.x(), c4.x()});
        const double maxY = std::max({c1.y(), c2.y(), c3.y(), c4.y()});
        QJsonObject bbox;
        bbox.insert("x"_L1, finite(minX));
        bbox.insert("y"_L1, finite(minY));
        bbox.insert("width"_L1, finite(maxX - minX, w));
        bbox.insert("height"_L1, finite(maxY - minY, h));
        out.insert("absoluteBoundingBox"_L1, bbox);
    }
    {
        const auto t = node.value(QStringLiteral("transform")).toMap();
        QJsonArray row0;
        row0.append(fdouble(t.value(QStringLiteral("m00")), 1.0));
        row0.append(fdouble(t.value(QStringLiteral("m01"))));
        row0.append(fdouble(t.value(QStringLiteral("m02"))));
        QJsonArray row1;
        row1.append(fdouble(t.value(QStringLiteral("m10"))));
        row1.append(fdouble(t.value(QStringLiteral("m11")), 1.0));
        row1.append(fdouble(t.value(QStringLiteral("m12"))));
        QJsonArray rt; rt.append(row0); rt.append(row1);
        out.insert("relativeTransform"_L1, rt);
    }

    // Fills / strokes
    const auto fillPaints = node.value(QStringLiteral("fillPaints")).toArray();
    if (!fillPaints.isEmpty())
        out.insert("fills"_L1, convertPaints(fillPaints));
    const auto strokePaints = node.value(QStringLiteral("strokePaints")).toArray();
    if (!strokePaints.isEmpty())
        out.insert("strokes"_L1, convertPaints(strokePaints));
    const auto strokeWeight = fdouble(node.value(QStringLiteral("strokeWeight")), 0.0);
    if (strokeWeight > 0) out.insert("strokeWeight"_L1, strokeWeight);
    const auto strokeAlign = node.value(QStringLiteral("strokeAlign")).toString();
    if (!strokeAlign.isEmpty()) out.insert("strokeAlign"_L1, strokeAlign);
    const auto strokeCap = node.value(QStringLiteral("strokeCap")).toString();
    if (!strokeCap.isEmpty()) out.insert("strokeCap"_L1, strokeCap);
    const auto strokeJoin = node.value(QStringLiteral("strokeJoin")).toString();
    if (!strokeJoin.isEmpty()) out.insert("strokeJoin"_L1, strokeJoin);
    const auto dashPattern = node.value(QStringLiteral("dashPattern")).toArray();
    if (!dashPattern.isEmpty()) {
        QJsonArray arr;
        for (const auto &d : dashPattern) arr.append(fdouble(d));
        out.insert("strokeDashes"_L1, arr);
    }

    // CANVAS background color (reused by buildFromFigmaJson via page["backgroundColor"]).
    if (type == "CANVAS"_L1) {
        const auto bg = node.value(QStringLiteral("backgroundColor")).toMap();
        if (!bg.isEmpty()) {
            QJsonObject color;
            color.insert("r"_L1, fdouble(bg.value(QStringLiteral("r"))));
            color.insert("g"_L1, fdouble(bg.value(QStringLiteral("g"))));
            color.insert("b"_L1, fdouble(bg.value(QStringLiteral("b"))));
            color.insert("a"_L1, fdouble(bg.value(QStringLiteral("a")), 1.0));
            out.insert("backgroundColor"_L1, color);
        }
    }

    // Rectangle corner radii
    auto insertRadii = [&]() {
        const auto tl = node.value(QStringLiteral("rectangleTopLeftCornerRadius"));
        const auto tr = node.value(QStringLiteral("rectangleTopRightCornerRadius"));
        const auto bl = node.value(QStringLiteral("rectangleBottomLeftCornerRadius"));
        const auto br = node.value(QStringLiteral("rectangleBottomRightCornerRadius"));
        if (!(tl.isUndefined() && tr.isUndefined() && bl.isUndefined() && br.isUndefined())) {
            QJsonArray radii;
            radii.append(fdouble(tl)); radii.append(fdouble(tr));
            radii.append(fdouble(br)); radii.append(fdouble(bl));
            out.insert("rectangleCornerRadii"_L1, radii);
        }
        const auto cornerRadius = node.value(QStringLiteral("cornerRadius"));
        if (!cornerRadius.isUndefined())
            out.insert("cornerRadius"_L1, fdouble(cornerRadius));
    };
    insertRadii();

    // Frame clip
    const bool frameMaskDisabled = node.value(QStringLiteral("frameMaskDisabled")).toBool(false);
    out.insert("clipsContent"_L1, !frameMaskDisabled && (type == "FRAME"_L1 || type == "INSTANCE"_L1 || type == "SYMBOL"_L1));

    // Geometry → SVG paths (fillGeometry array)
    const auto fillGeom = node.value(QStringLiteral("fillGeometry")).toArray();
    if (!fillGeom.isEmpty())
        out.insert("fillGeometry"_L1, buildGeometryArray(fillGeom, ctx.blobs));
    const auto strokeGeom = node.value(QStringLiteral("strokeGeometry")).toArray();
    if (!strokeGeom.isEmpty())
        out.insert("strokeGeometry"_L1, buildGeometryArray(strokeGeom, ctx.blobs));

    // TEXT
    if (type == "TEXT"_L1) {
        out.insert("characters"_L1, textDataToCharacters(node.value(QStringLiteral("textData"))));
        out.insert("style"_L1, textStyle(node));
    }

    // Effects (DROP_SHADOW/INNER_SHADOW/LAYER_BLUR).
    const auto effects = node.value(QStringLiteral("effects")).toArray();
    if (!effects.isEmpty()) {
        QJsonArray arr;
        for (const auto &ev : effects) {
            if (!ev.isMap()) continue;
            const auto em = ev.toMap();
            QJsonObject eo;
            eo.insert("type"_L1, em.value(QStringLiteral("type")).toString());
            eo.insert("visible"_L1, em.value(QStringLiteral("visible")).toBool(true));
            eo.insert("radius"_L1, fdouble(em.value(QStringLiteral("radius"))));
            eo.insert("spread"_L1, fdouble(em.value(QStringLiteral("spread"))));
            const auto cc = em.value(QStringLiteral("color")).toMap();
            QJsonObject color;
            color.insert("r"_L1, fdouble(cc.value(QStringLiteral("r"))));
            color.insert("g"_L1, fdouble(cc.value(QStringLiteral("g"))));
            color.insert("b"_L1, fdouble(cc.value(QStringLiteral("b"))));
            color.insert("a"_L1, fdouble(cc.value(QStringLiteral("a")), 1.0));
            eo.insert("color"_L1, color);
            const auto off = em.value(QStringLiteral("offset")).toMap();
            QJsonObject offset;
            offset.insert("x"_L1, fdouble(off.value(QStringLiteral("x"))));
            offset.insert("y"_L1, fdouble(off.value(QStringLiteral("y"))));
            eo.insert("offset"_L1, offset);
            arr.append(eo);
        }
        out.insert("effects"_L1, arr);
    }

    // Children. INSTANCE expands its referenced SYMBOL's children, re-positioned
    // relative to the INSTANCE. Each instance gets a unique displayId prefix so
    // downstream id lookups don't collide between multiple instances of the
    // same symbol.
    if (type == "INSTANCE"_L1) {
        const auto symbolData = node.value(QStringLiteral("symbolData")).toMap();
        const auto symID = symbolData.value(QStringLiteral("symbolID"));
        if (!symID.isUndefined()) {
            const QString symbolGuid = guidToStr(symID);
            const int symIdx = ctx.nodeByGuidStr.value(symbolGuid, -1);
            if (symIdx >= 0) {
                NodeCtx childCtx { absX, guidStr + u'|' };
                const auto it = ctx.childrenByParent.constFind(symbolGuid);
                if (it != ctx.childrenByParent.cend()) {
                    QJsonArray children;
                    for (int ci : *it)
                        children.append(convertNode(ctx, ci, childCtx));
                    if (!children.isEmpty())
                        out.insert("children"_L1, children);
                }
            }
        }
    } else if (type == "DOCUMENT"_L1 || type == "CANVAS"_L1
               || type == "FRAME"_L1 || type == "GROUP"_L1
               || type == "SYMBOL"_L1 || type == "SECTION"_L1
               || type == "BOOLEAN_OPERATION"_L1) {
        NodeCtx childCtx { absX, nctx.displayIdPrefix };
        const QJsonArray children = buildChildren(ctx, rawGuid, childCtx);
        if (!children.isEmpty())
            out.insert("children"_L1, children);
    }

    return out;
}

// Build guid→index map and parent→ordered children map. Absolute positioning
// is computed lazily during convertNode recursion so INSTANCE expansion can
// reposition cloned SYMBOL subtrees correctly.
static void buildIndexes(BuildCtx &ctx)
{
    for (int i = 0; i < ctx.nodeChanges.size(); ++i) {
        const auto m = ctx.nodeChanges.at(i).toMap();
        const QString gs = guidToStr(m.value(QStringLiteral("guid")));
        ctx.nodeByGuidStr.insert(gs, i);
    }
    QHash<QString, QMap<QString, int>> orderedByParent;
    for (int i = 0; i < ctx.nodeChanges.size(); ++i) {
        const auto m = ctx.nodeChanges.at(i).toMap();
        const auto pi = m.value(QStringLiteral("parentIndex")).toMap();
        if (pi.isEmpty()) continue;
        const QString parentGuid = guidToStr(pi.value(QStringLiteral("guid")));
        const QString pos = pi.value(QStringLiteral("position")).toString();
        orderedByParent[parentGuid].insert(pos, i);
    }
    for (auto it = orderedByParent.cbegin(); it != orderedByParent.cend(); ++it) {
        QVector<int> indices;
        for (auto mit = it.value().cbegin(); mit != it.value().cend(); ++mit)
            indices.append(mit.value());
        ctx.childrenByParent.insert(it.key(), indices);
    }
}

} // anon

bool convertMessageToFileJson(const QCborValue &message,
                              const QString &metaJson,
                              QJsonObject *out,
                              QString *error)
{
    if (!message.isMap()) {
        if (error) *error = QStringLiteral("message is not a map");
        return false;
    }
    const auto mm = message.toMap();
    BuildCtx ctx;
    ctx.nodeChanges = mm.value(QStringLiteral("nodeChanges")).toArray();
    ctx.blobs = mm.value(QStringLiteral("blobs")).toArray();
    if (ctx.nodeChanges.isEmpty()) {
        if (error) *error = QStringLiteral("no nodeChanges in message");
        return false;
    }

    buildIndexes(ctx);

    // Locate DOCUMENT node.
    int docIdx = -1;
    for (int i = 0; i < ctx.nodeChanges.size(); ++i) {
        if (ctx.nodeChanges.at(i).toMap().value(QStringLiteral("type")).toString() == "DOCUMENT"_L1) {
            docIdx = i;
            break;
        }
    }
    if (docIdx < 0) {
        if (error) *error = QStringLiteral("no DOCUMENT node found");
        return false;
    }

    NodeCtx rootCtx { QTransform(), QString() };
    QJsonObject document = convertNode(ctx, docIdx, rootCtx);

    // Filter CANVAS children to exclude the "Internal Only" canvas.
    const QJsonArray allChildren = document.value("children"_L1).toArray();
    QJsonArray pages;
    for (const auto &c : allChildren) {
        const auto co = c.toObject();
        const auto origIdx = ctx.nodeByGuidStr.value(co.value("id"_L1).toString(), -1);
        const bool internalOnly = (origIdx >= 0)
            && ctx.nodeChanges.at(origIdx).toMap().value(QStringLiteral("internalOnly")).toBool(false);
        if (!internalOnly)
            pages.append(co);
    }
    document.insert("children"_L1, pages);

    // Determine file name from meta.json if provided.
    QString fileName;
    if (!metaJson.isEmpty()) {
        QJsonParseError pe;
        const auto meta = QJsonDocument::fromJson(metaJson.toUtf8(), &pe).object();
        fileName = meta.value("file_name"_L1).toString();
    }

    QJsonObject result;
    result.insert("name"_L1, fileName);
    result.insert("document"_L1, document);
    *out = result;
    return true;
}

} // namespace FigToRest
