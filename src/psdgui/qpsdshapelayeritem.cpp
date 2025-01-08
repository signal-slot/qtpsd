// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdshapelayeritem.h"
#include "qpsdvectorstrokecontentsetting.h"

#include <QtGui/QPen>

#include <QtPsdCore/QPsdVectorStrokeData>

QT_BEGIN_NAMESPACE

namespace {
Qt::PenCapStyle strokeStyleLineCapTypeToQt(const QPsdEnum &data)
{
    const auto type = data.type();
    Q_ASSERT(type == "strokeStyleLineCapType");
    const auto value = data.value();
    if (value == "strokeStyleButtCap") {
        return Qt::FlatCap;
    }
    qFatal() << type << "not implemented";
    return Qt::FlatCap;
}
}

class QPsdShapeLayerItem::Private
{
public:
    QPen pen = Qt::NoPen;
    QBrush brush = Qt::NoBrush;
    QPsdAbstractLayerItem::PathInfo path;
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

    const auto vstk = additionalLayerInformation.value("vstk").value<QPsdVectorStrokeData>();
    if (vstk.strokeEnabled()) {
        QColor color(vstk.strokeStyleContent());
        color.setAlpha(vstk.strokeStyleOpacity().value() * 255 / 100);
        d->pen = QPen(color);

        d->pen.setCapStyle(strokeStyleLineCapTypeToQt(vstk.strokeStyleLineCapType()));
        if (vstk.strokeStyleLineDashOffset().unit() == QPsdUnitFloat::Points) {
            d->pen.setDashOffset(vstk.strokeStyleLineDashOffset().value());
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
        } else {
            qFatal() << strokeStyleLineJoinTypeValue << "not implemented";
        }
        d->pen.setWidthF(vstk.strokeStyleLineWidth().value());
        d->pen.setMiterLimit(vstk.strokeStyleMiterLimit());
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
                case QPsdVectorStrokeContentSetting::Liner: {
                    const auto colors = vscg.colors();
                    const auto opacities = vscg.opacities();
                    const auto angle = vscg.angle() * M_PI / 180.0;
                    QLinearGradient gradient;
                    const auto center = rect().center();
                    gradient.setStart(center.x() - std::cos(angle) * rect().width() / 2,
                                      center.y() - std::sin(angle) * rect().height() / 2);
                    gradient.setFinalStop(center.x() + std::cos(angle) * rect().width() / 2,
                                          center.y() + std::sin(angle) * rect().height() / 2);
                    for (int i = 0; i < colors.size(); ++i) {
                        const auto color = colors.at(i);
                        gradient.setColorAt(color.first, QColor(color.second));
                    }
                    d->brush = QBrush(gradient);
                    break; }
                default:
                    qFatal() << vscg.gradientType() << "not implemented";
                }
                break; }
            }
        } else if (additionalLayerInformation.contains("SoCo")) {
            const auto soco = additionalLayerInformation.value("SoCo").value<QPsdDescriptor>().data();
            const auto clr_ = soco.value("Clr ").value<QPsdDescriptor>().data();
            const int rd__ = clr_.value("Rd  ").toDouble();
            const int grn_ = clr_.value("Grn ").toDouble();
            const int bl__ = clr_.value("Bl  ").toDouble();
            d->brush = QBrush(QColor(rd__, grn_, bl__));
        }
    }
}

QPsdShapeLayerItem::QPsdShapeLayerItem()
    : QPsdAbstractLayerItem()
    , d(new Private)
{}

QPsdShapeLayerItem::~QPsdShapeLayerItem() = default;

QPen QPsdShapeLayerItem::pen() const
{
    return d->pen;
}

QBrush QPsdShapeLayerItem::brush() const
{
    return d->brush;
}

QPsdAbstractLayerItem::PathInfo QPsdShapeLayerItem::pathInfo() const
{
    return d->path;
}

QT_END_NAMESPACE
