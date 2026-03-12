// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdshapelayeritem.h"
#include "qpsdvectorstrokecontentsetting.h"

#include <QtGui/QColor>
#include <QtGui/QConicalGradient>
#include <QtGui/QLinearGradient>
#include <QtGui/QPen>
#include <QtGui/QRadialGradient>

#include <QtPsdCore/QPsdDescriptor>
#include <QtPsdCore/QPsdEnum>
#include <QtPsdCore/QPsdUnitFloat>
#include <QtPsdCore/QPsdVectorStrokeData>

QT_BEGIN_NAMESPACE

namespace {
QColor colorFromDescriptor(const QPsdDescriptor &clrDescriptor)
{
    const auto clr_ = clrDescriptor.data();
    if (clrDescriptor.classID() == "CMYC") {
        const double c = clr_.value("Cyn ").toDouble() / 100.0;
        const double m = clr_.value("Mgnt").toDouble() / 100.0;
        const double y = clr_.value("Ylw ").toDouble() / 100.0;
        const double k = clr_.value("Blck").toDouble() / 100.0;
        return QColor::fromCmykF(c, m, y, k);
    } else {
        const int r = clr_.value("Rd  ").toDouble();
        const int g = clr_.value("Grn ").toDouble();
        const int b = clr_.value("Bl  ").toDouble();
        return QColor(r, g, b);
    }
}
Qt::PenCapStyle strokeStyleLineCapTypeToQt(const QPsdEnum &data)
{
    const auto type = data.type();
    Q_ASSERT(type == "strokeStyleLineCapType");
    const auto value = data.value();
    if (value == "strokeStyleButtCap") {
        return Qt::FlatCap;
    } else if (value == "strokeStyleRoundCap") {
        return Qt::RoundCap;
    } else if (value == "strokeStyleSquareCap") {
        return Qt::SquareCap;
    }
    qFatal() << value << "not implemented";
    return Qt::FlatCap;
}
double interpolateOpacity(const QList<QPair<double, double>> &transparencies, double position)
{
    if (transparencies.isEmpty())
        return 100.0;
    if (position <= transparencies.first().first)
        return transparencies.first().second;
    if (position >= transparencies.last().first)
        return transparencies.last().second;
    for (int i = 0; i < transparencies.size() - 1; ++i) {
        const auto &a = transparencies.at(i);
        const auto &b = transparencies.at(i + 1);
        if (position >= a.first && position <= b.first) {
            const double range = b.first - a.first;
            if (range <= 0)
                return a.second;
            const double t = (position - a.first) / range;
            return a.second + t * (b.second - a.second);
        }
    }
    return transparencies.last().second;
}

QBrush brushFromGdFl(const QVariant &gdflVariant, const QRectF &rect)
{
    const auto gdfl = gdflVariant.value<QPsdDescriptor>().data();
    const auto grad = gdfl.value("Grad").value<QPsdDescriptor>().data();
    const auto intr = grad.contains("Intr") ? grad.value("Intr").toDouble() : 4096.0;

    // Parse transparency stops
    QList<QPair<double, double>> transparencies;
    const auto trns = grad.value("Trns").toList();
    for (const auto &tln : trns) {
        const auto trnS = tln.value<QPsdDescriptor>().data();
        const auto lctn = trnS.value("Lctn").toDouble();
        const auto opct = trnS.value("Opct").value<QPsdUnitFloat>();
        transparencies.append({lctn, opct.value()});
    }

    // Parse color stops
    QGradientStops stops;
    const auto clrs = grad.value("Clrs").toList();
    for (const auto &c : clrs) {
        const auto clr = c.value<QPsdDescriptor>().data();
        const auto lctn = clr.value("Lctn").toDouble();
        const auto clrDescriptor = clr.value("Clr ").value<QPsdDescriptor>();
        QColor color = colorFromDescriptor(clrDescriptor);
        // Interpolate opacity at this color stop position and apply as alpha
        const double opacityPercent = interpolateOpacity(transparencies, lctn);
        color.setAlphaF(opacityPercent / 100.0);
        stops.append({lctn / intr, color});
    }

    // Handle reverse flag
    const auto rvrs = gdfl.value("Rvrs").toBool();
    if (rvrs) {
        QGradientStops reversedStops;
        for (int i = stops.size() - 1; i >= 0; --i)
            reversedStops.append({1.0 - stops[i].first, stops[i].second});
        stops = reversedStops;
    }

    // Determine gradient type
    const auto type = gdfl.value("Type").value<QPsdEnum>();
    const auto typeValue = type.value();

    // Read angle
    const auto angl = gdfl.value("Angl").value<QPsdUnitFloat>();
    const auto angle = angl.value() * M_PI / 180.0;

    if (typeValue == "Lnr ") {
        QLinearGradient gradient;
        const QPointF center(rect.width() / 2, rect.height() / 2);
        // PSD angle is in screen coordinates (0°=right, 90°=down).
        // Position 0 is at the start (opposite end of gradient direction).
        gradient.setStart(center.x() - std::cos(angle) * rect.width() / 2,
                          center.y() + std::sin(angle) * rect.height() / 2);
        gradient.setFinalStop(center.x() + std::cos(angle) * rect.width() / 2,
                              center.y() - std::sin(angle) * rect.height() / 2);
        gradient.setStops(stops);
        return QBrush(gradient);
    } else if (typeValue == "Rdl ") {
        QRadialGradient gradient;
        const QPointF center(rect.width() / 2, rect.height() / 2);
        gradient.setCenter(center);
        gradient.setFocalPoint(center);
        gradient.setRadius(qMax(rect.width(), rect.height()) / 2);
        gradient.setStops(stops);
        return QBrush(gradient);
    } else if (typeValue == "Angl") {
        QConicalGradient gradient;
        const QPointF center(rect.width() / 2, rect.height() / 2);
        gradient.setCenter(center);
        gradient.setAngle(angl.value());
        // PSD sweeps clockwise, Qt sweeps counter-clockwise — reverse stops
        QGradientStops reversedStops;
        for (const auto &stop : stops)
            reversedStops.prepend({1.0 - stop.first, stop.second});
        gradient.setStops(reversedStops);
        return QBrush(gradient);
    } else if (typeValue == "Rflc") {
        // Reflected: linear gradient mirrored at center.
        // PSD stops define center→edge pattern. Build symmetric full-span stops:
        // First half (0→0.5): reversed stops (edge→center)
        // Second half (0.5→1.0): normal stops (center→edge)
        QGradientStops reflectedStops;
        for (int i = stops.size() - 1; i >= 0; --i)
            reflectedStops.append({(1.0 - stops[i].first) * 0.5, stops[i].second});
        for (int i = 1; i < stops.size(); ++i)
            reflectedStops.append({0.5 + stops[i].first * 0.5, stops[i].second});
        QLinearGradient gradient;
        const QPointF center(rect.width() / 2, rect.height() / 2);
        gradient.setStart(center.x() - std::cos(angle) * rect.width() / 2,
                          center.y() + std::sin(angle) * rect.height() / 2);
        gradient.setFinalStop(center.x() + std::cos(angle) * rect.width() / 2,
                              center.y() - std::sin(angle) * rect.height() / 2);
        gradient.setStops(reflectedStops);
        return QBrush(gradient);
    } else if (typeValue == "Dmnd") {
        // Diamond: approximate as radial gradient (no direct Qt equivalent)
        QRadialGradient gradient;
        const QPointF center(rect.width() / 2, rect.height() / 2);
        gradient.setCenter(center);
        gradient.setFocalPoint(center);
        gradient.setRadius(qMax(rect.width(), rect.height()) / 2);
        gradient.setStops(stops);
        return QBrush(gradient);
    } else {
        qWarning() << "GdFl gradient type" << typeValue << "not supported";
        return Qt::NoBrush;
    }
}
}

class QPsdShapeLayerItem::Private
{
public:
    QPen pen = Qt::NoPen;
    QBrush brush = Qt::NoBrush;
    QPsdShapeLayerItem::StrokeAlignment strokeAlignment = QPsdShapeLayerItem::StrokeCenter;
    QPsdAbstractLayerItem::PathInfo path;
    QString vscgPatternId;
    qreal vscgPatternScale = 1.0;
    qreal vscgPatternAngle = 0;
};

QPsdShapeLayerItem::QPsdShapeLayerItem(const QPsdLayerRecord &record)
    : QPsdAbstractLayerItem(record)
    , d(new Private)
{
    const auto additionalLayerInformation = record.additionalLayerInformation();

    if (additionalLayerInformation.contains("vsms")) {
        d->path = parseShape(additionalLayerInformation.value("vsms").value<QPsdVectorMaskSetting>());
    } else if (additionalLayerInformation.contains("vmsk")) {
        d->path = parseShape(additionalLayerInformation.value("vmsk").value<QPsdVectorMaskSetting>());
    }

    if (additionalLayerInformation.contains("vstk")) {
        const auto vstk = additionalLayerInformation.value("vstk").value<QPsdVectorStrokeData>();
        if (vstk.strokeEnabled()) {
            QColor color(vstk.strokeStyleContent());
            color.setAlpha(vstk.strokeStyleOpacity().value() * 255 / 100);
            d->pen = QPen(color);

            d->pen.setCapStyle(strokeStyleLineCapTypeToQt(vstk.strokeStyleLineCapType()));
            if (vstk.strokeStyleLineDashOffset().unit() == QPsdUnitFloat::Points) {
                d->pen.setDashOffset(vstk.strokeStyleLineDashOffset().value());
            } else if (vstk.strokeStyleLineDashOffset().unit() == QPsdUnitFloat::Pixels) {
                // Pixels are the same as points in Qt
                d->pen.setDashOffset(vstk.strokeStyleLineDashOffset().value());
            } else if (vstk.strokeStyleLineDashOffset().unit() == QPsdUnitFloat::Percent) {
                // For percent, convert to points based on stroke width
                const auto percent = vstk.strokeStyleLineDashOffset().value();
                const auto strokeWidth = d->pen.widthF();
                d->pen.setDashOffset(strokeWidth * percent / 100.0);
            } else {
                qFatal() << vstk.strokeStyleLineDashOffset().unit() << "not implemented";
            }
            const auto strokeStyleLineDashSet = vstk.strokeStyleLineDashSet();
            if (!strokeStyleLineDashSet.isEmpty()) {
                d->pen.setDashPattern(strokeStyleLineDashSet);
            }
            const auto strokeStyleLineJoinType = vstk.strokeStyleLineJoinType();
            Q_ASSERT(strokeStyleLineJoinType.type() == "strokeStyleLineJoinType");
            const auto strokeStyleLineJoinTypeValue = strokeStyleLineJoinType.value();
            if (strokeStyleLineJoinTypeValue == "strokeStyleMiterJoin") {
                d->pen.setJoinStyle(Qt::MiterJoin);
            } else if (strokeStyleLineJoinTypeValue == "strokeStyleRoundJoin") {
                d->pen.setJoinStyle(Qt::RoundJoin);
            } else if (strokeStyleLineJoinTypeValue == "strokeStyleBevelJoin") {
                d->pen.setJoinStyle(Qt::BevelJoin);
            } else {
                qFatal() << strokeStyleLineJoinTypeValue << "not implemented";
            }
            d->pen.setWidthF(vstk.strokeStyleLineWidth().value());
            d->pen.setMiterLimit(vstk.strokeStyleMiterLimit());

            const auto lineAlignment = vstk.strokeStyleLineAlignment().value();
            if (lineAlignment == "strokeStyleAlignInside") {
                d->strokeAlignment = StrokeInside;
            } else if (lineAlignment == "strokeStyleAlignOutside") {
                d->strokeAlignment = StrokeOutside;
            } else {
                d->strokeAlignment = StrokeCenter;
            }
        }

        if (vstk.fillEnabled()) {
            if (additionalLayerInformation.contains("vscg")) {
                const auto vscg = additionalLayerInformation.value("vscg").value<QPsdVectorStrokeContentSetting>();
                switch (vscg.type()) {
                case QPsdVectorStrokeContentSetting::SolidColor:
                    d->brush = QBrush(QColor(vscg.solidColor()));
                    break;
                case QPsdVectorStrokeContentSetting::GradientFill: {
                    switch (vscg.gradientType()) {
                    case QPsdVectorStrokeContentSetting::Linear: {
                        const auto colors = vscg.colors();
                        const auto opacities = vscg.opacities();
                        const auto angle = vscg.angle() * M_PI / 180.0;
                        QLinearGradient gradient;
                        const QPointF center(rect().width() / 2, rect().height() / 2);
                        gradient.setStart(center.x() + std::cos(angle) * rect().width() / 2,
                                          center.y() + std::sin(angle) * rect().height() / 2);
                        gradient.setFinalStop(center.x() - std::cos(angle) * rect().width() / 2,
                                              center.y() - std::sin(angle) * rect().height() / 2);
                        for (int i = 0; i < colors.size(); ++i) {
                            const auto color = colors.at(i);
                            gradient.setColorAt(color.first, QColor(color.second));
                        }
                        d->brush = QBrush(gradient);
                        break; }
                    case QPsdVectorStrokeContentSetting::Radial: {
                        const auto colors = vscg.colors();
                        const auto opacities = vscg.opacities();
                        QRadialGradient gradient;
                        const QPointF center(rect().width() / 2, rect().height() / 2);
                        gradient.setCenter(center);
                        gradient.setFocalPoint(center);
                        gradient.setRadius(qMax(rect().width(), rect().height()) / 2);
                        for (int i = 0; i < colors.size(); ++i) {
                            const auto color = colors.at(i);
                            gradient.setColorAt(color.first, QColor(color.second));
                        }
                        d->brush = QBrush(gradient);
                        break; }
                    default:
                        qWarning() << "vscg gradient type" << vscg.gradientType() << "not implemented";
                    }
                    break; }
                case QPsdVectorStrokeContentSetting::PatternFill:
                    d->vscgPatternId = vscg.patternId();
                    d->vscgPatternScale = vscg.patternScale();
                    d->vscgPatternAngle = vscg.angle();
                    d->brush = Qt::NoBrush;
                    break;
                }
            } else if (additionalLayerInformation.contains("SoCo")) {
                const auto soco = additionalLayerInformation.value("SoCo").value<QPsdDescriptor>().data();
                d->brush = QBrush(colorFromDescriptor(soco.value("Clr ").value<QPsdDescriptor>()));
            } else if (additionalLayerInformation.contains("PtFl")) {
                const auto ptfl = additionalLayerInformation.value("PtFl").value<QPsdDescriptor>().data();
                const auto ptrn = ptfl.value("Ptrn").value<QPsdDescriptor>().data();
                d->vscgPatternId = ptrn.value("Idnt").toString();
                const auto scl = ptfl.value("Scl ").value<QPsdUnitFloat>();
                if (scl.unit() == QPsdUnitFloat::Percent) {
                    d->vscgPatternScale = scl.value() / 100;
                }
                const auto angl = ptfl.value("Angl").value<QPsdUnitFloat>();
                if (angl.unit() == QPsdUnitFloat::Angle) {
                    d->vscgPatternAngle = angl.value();
                }
                d->brush = Qt::NoBrush;
            } else if (additionalLayerInformation.contains("GdFl")) {
                d->brush = brushFromGdFl(additionalLayerInformation.value("GdFl"), rect());
            }
        }
    } else {
        if (additionalLayerInformation.contains("SoCo")) {
            const auto soco = additionalLayerInformation.value("SoCo").value<QPsdDescriptor>().data();
            d->brush = QBrush(colorFromDescriptor(soco.value("Clr ").value<QPsdDescriptor>()));
        } else if (additionalLayerInformation.contains("PtFl")) {
            const auto ptfl = additionalLayerInformation.value("PtFl").value<QPsdDescriptor>().data();
            const auto ptrn = ptfl.value("Ptrn").value<QPsdDescriptor>().data();
            d->vscgPatternId = ptrn.value("Idnt").toString();
            const auto scl = ptfl.value("Scl ").value<QPsdUnitFloat>();
            if (scl.unit() == QPsdUnitFloat::Percent) {
                d->vscgPatternScale = scl.value() / 100;
            }
            const auto angl = ptfl.value("Angl").value<QPsdUnitFloat>();
            if (angl.unit() == QPsdUnitFloat::Angle) {
                d->vscgPatternAngle = angl.value();
            }
            d->brush = Qt::NoBrush;
        } else if (additionalLayerInformation.contains("GdFl")) {
            d->brush = brushFromGdFl(additionalLayerInformation.value("GdFl"), rect());
        }
    }
}

QPsdShapeLayerItem::QPsdShapeLayerItem()
    : QPsdAbstractLayerItem()
    , d(new Private)
{}

QPsdShapeLayerItem::~QPsdShapeLayerItem() = default;

void QPsdShapeLayerItem::setPen(const QPen &pen)
{
    d->pen = pen;
}

void QPsdShapeLayerItem::setBrush(const QBrush &brush)
{
    d->brush = brush;
}

void QPsdShapeLayerItem::setStrokeAlignment(StrokeAlignment alignment)
{
    d->strokeAlignment = alignment;
}

void QPsdShapeLayerItem::setPathInfo(const PathInfo &info)
{
    d->path = info;
}

QPen QPsdShapeLayerItem::pen() const
{
    return d->pen;
}

QBrush QPsdShapeLayerItem::brush() const
{
    return d->brush;
}

QPsdShapeLayerItem::StrokeAlignment QPsdShapeLayerItem::strokeAlignment() const
{
    return d->strokeAlignment;
}

QPsdAbstractLayerItem::PathInfo QPsdShapeLayerItem::pathInfo() const
{
    return d->path;
}

QString QPsdShapeLayerItem::vscgPatternId() const
{
    return d->vscgPatternId;
}

qreal QPsdShapeLayerItem::vscgPatternScale() const
{
    return d->vscgPatternScale;
}

qreal QPsdShapeLayerItem::vscgPatternAngle() const
{
    return d->vscgPatternAngle;
}

QT_END_NAMESPACE
