// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdabstractlayeritem.h"
#include "qpsdborder.h"
#include "qpsdpatternfill.h"
#include "qpsdguiglobal.h"

#include <QtCore/QCborArray>
#include <QtCore/QCborMap>

#include <QtGui/QLinearGradient>

#include <QtPsdCore/QPsdEffectsLayer>
#include <QtPsdCore/QPsdEnum>
#include <QtPsdCore/QPsdUnitFloat>

QT_BEGIN_NAMESPACE

class QPsdAbstractLayerItem::Private
{
public:
    QPsdLayerRecord record;

    quint32 id = 0;
    QString name;
    QColor color = Qt::transparent;
    bool visible = true;
    qreal opacity = 1.0;
    qreal fillOpacity = 1.0;
    QRect rect;
    QSize documentSize;
    QScopedPointer<QGradient> gradient;
    qreal gradientOpacity = 1.0;
    QCborMap dropShadow;
    QScopedPointer<QPsdBorder> border;
    QScopedPointer<QPsdPatternFill> patternFill;
    PathInfo vectorMask;
    QImage image;
    QImage transparencyMask;
    QImage layerMask;
    QRect layerMaskRect;
    quint8 layerMaskDefaultColor = 255;  // Default: outside mask is opaque
    QPsdLinkedLayer::LinkedFile linkedFile;
    QVariantList effects;
};

QPsdAbstractLayerItem::QPsdAbstractLayerItem(int width, int height)
    : QPsdAbstractLayerItem()
{
    d->rect = QRect(0, 0, width, height);
}

QPsdAbstractLayerItem::QPsdAbstractLayerItem(const QPsdLayerRecord &record)
    : QPsdAbstractLayerItem()
{
    d->record = record;
    const auto additionalLayerInformation = record.additionalLayerInformation();

    // Layer ID
    const auto lyid = additionalLayerInformation.value("lyid").value<quint32>();
    d->id = lyid;

    // Layer name
    d->name = QString::fromUtf8(record.name());
    if (additionalLayerInformation.contains("luni")) {
        d->name = additionalLayerInformation.value("luni").toString();
    }

    // Sheet Color setting
    if (additionalLayerInformation.contains("lclr")) {
        d->color = QColor(additionalLayerInformation.value("lclr").toString());
    }

    // Layer visibility
    d->visible = !record.isVisible();

    // Layer opacity
    d->opacity = record.opacity() / 255.0;

    // Fill opacity (iOpa)
    if (additionalLayerInformation.contains("iOpa")) {
        d->fillOpacity = additionalLayerInformation.value("iOpa").value<quint8>() / 255.0;
    }

    // Layer rectangle
    d->rect = record.rect();

    if (additionalLayerInformation.contains("lrFX")) {
        const auto effectsLayer = additionalLayerInformation.value("lrFX").value<QPsdEffectsLayer>();
        d->effects = effectsLayer.effects();
    }

    // Effects
    if (additionalLayerInformation.contains("lfx2")) {
        const auto lfx2 = additionalLayerInformation.value("lfx2").value<QPsdDescriptor>();
        std::function<bool(const QPsdDescriptor &, int indent)> debugDescriptor = [&](const QPsdDescriptor &descriptor, int indent) {
            const auto data = descriptor.data();
            if (data.contains("enab") && !data.value("enab").toBool()) {
                return false;
            }
            qDebug() << QByteArray(indent* 2, ' ').constData() << descriptor.classID() << ": {{";
            indent++;
            const auto keys = data.keys();
            for (const auto &key : keys) {
                const auto value = data.value(key);
                if (value.canConvert<QPsdDescriptor>()) {
                    qDebug() << QByteArray(indent* 2, ' ').constData() << key << ":";
                    debugDescriptor(value.value<QPsdDescriptor>(), indent + 1);
                } else if (value.canConvert<QPsdEnum>()) {
                    const auto psdEnum = value.value<QPsdEnum>();
                    qDebug() << QByteArray(indent* 2, ' ').constData() << key << ":" << psdEnum.value() << "(" << psdEnum.type() << ")";
                } else if (value.canConvert<QPsdUnitFloat>()) {
                    const auto psdUnitFloat = value.value<QPsdUnitFloat>();
                    qDebug() << QByteArray(indent* 2, ' ').constData() << key << ":" << psdUnitFloat.value() << "(" << psdUnitFloat.unit() << ")";
                } else if (value.canConvert<QVariantList>()) {
                    qDebug() << QByteArray(indent* 2, ' ').constData() << key << ": [";
                    indent++;
                    for (const auto &v : value.toList()) {
                        if (v.canConvert<QPsdDescriptor>()) {
                            debugDescriptor(v.value<QPsdDescriptor>(), indent);
                        } else if (v.canConvert<QPsdEnum>()) {
                            const auto psdEnum = v.value<QPsdEnum>();
                            qDebug() << QByteArray(indent* 2, ' ').constData() << key << ":" << psdEnum.value() << "(" << psdEnum.type() << ")";
                        } else if (value.canConvert<QPsdUnitFloat>()) {
                            const auto psdUnitFloat = value.value<QPsdUnitFloat>();
                            qDebug() << QByteArray(indent* 2, ' ').constData() << key << ":" << psdUnitFloat.value() << "(" << psdUnitFloat.unit() << ")";
                        } else {
                            qDebug() << QByteArray(indent* 2, ' ').constData() << v;
                        }
                    }
                    indent--;
                    qDebug() << QByteArray(indent* 2, ' ').constData() << "]";
                } else {
                    qDebug() << QByteArray(indent* 2, ' ').constData() << key << ":" << value;
                }
            }
            indent--;
            qDebug() << QByteArray(indent* 2, ' ').constData() << "}}";
            return true;
        };

        // debugDescriptor(lfx2, 0);

        auto fx = lfx2.data();

        // Gradient Fill
        if (fx.contains("GrFl")) {
            const auto grfl = fx.take("GrFl").value<QPsdDescriptor>().data();
            if (grfl.value("enab", false).toBool()) {
                const auto md__ = grfl.value("Md  ").value<QPsdEnum>();
                Q_ASSERT(md__.type() == "BlnM");
                const auto dthr = grfl.value("Dthr").toBool();
                Q_UNUSED(dthr);
                const auto ofst = grfl.value("Ofst").value<QPsdDescriptor>().data();
                const auto hrzn = ofst.value("Hrzn").value<QPsdUnitFloat>();
                Q_ASSERT(hrzn.unit() == QPsdUnitFloat::Percent);
                const auto vrtc = ofst.value("Vrtc").value<QPsdUnitFloat>();
                Q_ASSERT(vrtc.unit() == QPsdUnitFloat::Percent);
                \
                    const auto opct = grfl.value("Opct").value<QPsdUnitFloat>();
                Q_ASSERT(opct.unit() == QPsdUnitFloat::Percent);
                d->gradientOpacity = opct.value() / 100.0;

                const auto type = grfl.value("Type").value<QPsdEnum>();
                Q_ASSERT(type.type() == "GrdT");
                const auto grad = grfl.value("Grad").value<QPsdDescriptor>().data();
                const auto intr = grad.value("Intr").toDouble();
                const auto trns = grad.value("Trns").toList();
                const auto clrs = grad.value("Clrs").toList();
                const auto nm = grad.value("Nm  ").toString();
                Q_UNUSED(nm); // TODO
                const auto grdf = grad.value("GrdF").value<QPsdEnum>();
                Q_ASSERT(grdf.type() == "GrdF");
                QList<QPair<double, double>> transparencies;
                for (const auto &tln : trns) {
                    const auto trnS = tln.value<QPsdDescriptor>().data();
                    const auto lctn = trnS.value("Lctn").toInt();
                    const auto opct = trnS.value("Opct").value<QPsdUnitFloat>();
                    Q_ASSERT(opct.unit() == QPsdUnitFloat::Percent);
                    const auto mdpn = trnS.value("Mdpn").toInt();
                    Q_UNUSED(mdpn);
                    transparencies.append({ lctn, opct.value() });
                }
                QList<QPair<double, QColor>> colors;
                for (const auto &clr : clrs) {
                    const auto clrt = clr.value<QPsdDescriptor>().data();
                    const auto type = clrt.value("Type").value<QPsdEnum>();
                    Q_ASSERT(type.type() == "Clry");
                    const auto lctn = clrt.value("Lctn").toInt();
                    const auto clr_ = clrt.value("Clr ").value<QPsdDescriptor>().data();
                    const auto rd__ = clr_.value("Rd  ").toInt();
                    const auto grn_ = clr_.value("Grn ").toInt();
                    const auto bl__ = clr_.value("Bl  ").toInt();
                    const auto mdpn = clrt.value("Mdpn").toInt();
                    Q_UNUSED(mdpn);
                    colors.append({ lctn, QColor::fromRgb(rd__, grn_, bl__) });
                }

                const auto rvrs = grfl.value("Rvrs").toBool();
                Q_UNUSED(rvrs);

                const auto angl = grfl.value("Angl").value<QPsdUnitFloat>();
                Q_ASSERT(angl.unit() == QPsdUnitFloat::Angle);

                const auto scl_ = grfl.value("Scl ").value<QPsdUnitFloat>();
                Q_ASSERT(scl_.unit() == QPsdUnitFloat::Percent);

                const auto algn = grfl.value("Algn").toBool();
                Q_UNUSED(algn);

                if (type.value() == "Lnr ") {
                    auto gradient = new QLinearGradient;
                    const auto angle = angl.value() * M_PI / 180.0;
                    QSize size = d->rect.size() / 2;
                    gradient->setStart(size.width() + std::cos(angle) * d->rect.width() / 2, size.height() + std::sin(angle) * d->rect.height() / 2);
                    gradient->setFinalStop(size.width() - std::cos(angle) * d->rect.width() / 2, size.height() - std::sin(angle) * d->rect.height() / 2);
                    d->gradient.reset(gradient);
                    // qDebug() << gradient->start() << gradient->finalStop() << d->rect << angl.value();
                } else {
                    qWarning() << name() << type.value() << "not supported";
                }

                if (d->gradient) {
                    for (int i = 0; i < colors.count(); i++) {
                        const auto color = colors.at(i);
                        // const auto transparent = transparencies.at(i);
                        const auto pos = color.first;
                        // Q_ASSERT(pos == transparent.first);
                        QColor c = color.second;
                        // c.setAlpha(transparent.second * 255 / 100);
                        d->gradient->setColorAt(pos / intr, c);
                    }
                }
            }
        }

        // Drop Shadow
        if (fx.contains("DrSh")) {
            QCborMap dropShadow;
            const auto drsh = fx.take("DrSh").value<QPsdDescriptor>().data();

            const auto md__ = drsh.value("Md  ").value<QPsdEnum>();
            Q_ASSERT(md__.type() == "BlnM");
            dropShadow.insert("mode"_L1, QString::fromUtf8(md__.value()));

            const auto opct = drsh.value("Opct").value<QPsdUnitFloat>();
            Q_ASSERT(opct.unit() == QPsdUnitFloat::Percent);
            dropShadow.insert("opacity"_L1, opct.value() / 100.0);

            const auto lagl = drsh.value("lagl").value<QPsdUnitFloat>();
            Q_ASSERT(lagl.unit() == QPsdUnitFloat::Angle);
            dropShadow.insert("angle"_L1, lagl.value());
            const auto uglg = drsh.value("uglg").toBool();
            dropShadow.insert("useGlobalLight"_L1, uglg);

            const auto dstn = drsh.value("Dstn").value<QPsdUnitFloat>();
            Q_ASSERT(dstn.unit() == QPsdUnitFloat::Pixels);
            dropShadow.insert("distance"_L1, dstn.value());

            const auto cmkt = drsh.value("Ckmt").value<QPsdUnitFloat>();
            Q_ASSERT(cmkt.unit() == QPsdUnitFloat::Pixels);
            dropShadow.insert("spread"_L1, cmkt.value() / 100.0);

            const auto blur = drsh.value("blur").value<QPsdUnitFloat>();
            Q_ASSERT(blur.unit() == QPsdUnitFloat::Pixels);
            dropShadow.insert("size"_L1, blur.value());

            const auto trns = drsh.value("Trns").value<QPsdDescriptor>().data();
            const auto nm__ = trns.value("Nm  ").toString();
            QCborMap transferMap;
            transferMap.insert("name"_L1, nm__);
            QCborArray transfer;
            const auto crv_ = trns.value("Crv ").toList();
            for (const auto &crv : crv_) {
                const auto crpt = crv.value<QPsdDescriptor>().data();
                const auto hrzn = crpt.value("Hrzn").toDouble();
                const auto vrtc = crpt.value("Vrtc").toDouble();
                QCborMap point;
                point.insert("horizontal"_L1, hrzn);
                point.insert("vertical"_L1, vrtc);
                transfer.append(point);
            }
            transferMap.insert("points"_L1, transfer);
            dropShadow.insert("transfer"_L1, transferMap);

            const auto clr_ = drsh.value("Clr ").value<QPsdDescriptor>().data();
            const auto rd__ = clr_.value("Rd  ").toDouble();
            const auto grn_ = clr_.value("Grn ").toDouble();
            const auto bl__ = clr_.value("Bl  ").toDouble();
            dropShadow.insert("color"_L1, QColor::fromRgbF(rd__ / 255.0, grn_ / 255.0, bl__ / 255.0).name());

            const auto anta = drsh.value("AntA").toBool();
            dropShadow.insert("antialias"_L1, anta);

            const auto nose = drsh.value("Nose").value<QPsdUnitFloat>();
            Q_ASSERT(nose.unit() == QPsdUnitFloat::Percent);
            dropShadow.insert("noise"_L1, nose.value() / 100.0);

            const auto layerConceals = drsh.value("layerConceals").toBool();
            dropShadow.insert("layerConceals"_L1, layerConceals);
            const auto present = drsh.value("present").toBool();
            dropShadow.insert("present"_L1, present);
            d->dropShadow = dropShadow;
        }

        if (fx.contains("patternFill")) {
            const auto patternFill = fx.take("patternFill").value<QPsdDescriptor>();
            if (patternFill.data().value("enab", false).toBool()) {
                d->patternFill.reset(new QPsdPatternFill(patternFill));
            }
        }

        // Border
        if (fx.contains("FrFX")) {
            const auto frFX = fx.take("FrFX").value<QPsdDescriptor>();
            if (frFX.data().value("enab", false).toBool()) {
                d->border.reset(new QPsdBorder(frFX));
            }
        }
        const auto keys = fx.keys();
        for (const auto &key : keys) {
            const auto value = fx.value(key);
            if (!value.canConvert<QPsdDescriptor>())
                continue;
            const auto descriptor = value.value<QPsdDescriptor>().data();
            if (!descriptor.contains("enab") || !descriptor.value("enab").toBool())
                continue;
            qDebug() << key << descriptor;
        }
    }

    // Layer image
    const auto imageData = record.imageData();
    const auto header = imageData.header();

    // Use imageDataToImage function to create a QImage that owns its data
    d->image = QtPsdGui::imageDataToImage(imageData, header);

    // Transparency mask
    const auto transparencyMaskData = imageData.transparencyMaskData();
    if (!transparencyMaskData.isEmpty()) {
        const auto w = imageData.width();
        const auto h = imageData.height();
        // Create QImage that owns its data
        QImage image(w, h, QImage::Format_Grayscale8);
        if (!image.isNull() && static_cast<size_t>(transparencyMaskData.size()) >= static_cast<size_t>(w) * h) {
            memcpy(image.bits(), transparencyMaskData.constData(), w * h);
            d->transparencyMask = image;
        }
    }

    // User-supplied layer mask (raster mask)
    const auto layerMaskData = imageData.userSuppliedLayerMask();
    const auto maskInfo = record.layerMaskAdjustmentLayerData();
    // Skip if:
    // - No layer mask data
    // - Mask info is empty
    // - Mask is disabled
    // - Mask came from rendering other data (e.g. rasterized vector mask)
    // - Mask is from vector data (handled separately via vectorMask())
    if (!layerMaskData.isEmpty() && !maskInfo.isEmpty() && !maskInfo.isLayerMaskDisabled()
        && !maskInfo.isLayerMaskFromRenderingOtherData() && !maskInfo.isLayerMaskFromVectorData()) {
        const auto maskRect = maskInfo.rect();
        const auto w = maskRect.width();
        const auto h = maskRect.height();
        if (w > 0 && h > 0) {
            QImage maskImage(w, h, QImage::Format_Grayscale8);
            if (!maskImage.isNull() && static_cast<size_t>(layerMaskData.size()) >= static_cast<size_t>(w) * h) {
                // Row-by-row copy to handle QImage row alignment (bytesPerLine may differ from width)
                for (int y = 0; y < h; ++y) {
                    memcpy(maskImage.scanLine(y), layerMaskData.constData() + y * w, w);
                }
                d->layerMask = maskImage;
                d->layerMaskRect = maskRect;
                d->layerMaskDefaultColor = maskInfo.defaultColor();
            }
        }
    }

    // Document size
    d->documentSize = QSize(header.width(), header.height());

    // Vector mask
    if (additionalLayerInformation.contains("vmsk")) {
        d->vectorMask = parseShape(additionalLayerInformation.value("vmsk").value<QPsdVectorMaskSetting>());
    }
}

QPsdAbstractLayerItem::QPsdAbstractLayerItem()
    : d(new Private)
{}

QPsdAbstractLayerItem::~QPsdAbstractLayerItem() = default;

QPsdLayerRecord QPsdAbstractLayerItem::record() const
{
    return d->record;
}

quint32 QPsdAbstractLayerItem::id() const
{
    return d->id;
}

QString QPsdAbstractLayerItem::name() const
{
    return d->name;
}

QColor QPsdAbstractLayerItem::color() const
{
    return d->color;
}

bool QPsdAbstractLayerItem::isVisible() const
{
    return d->visible;
}

qreal QPsdAbstractLayerItem::opacity() const
{
    return d->opacity;
}

qreal QPsdAbstractLayerItem::fillOpacity() const
{
    return d->fillOpacity;
}

QRect QPsdAbstractLayerItem::rect() const
{
    return d->rect;
}

QGradient *QPsdAbstractLayerItem::gradient() const
{
    return d->gradient.data();
}

qreal QPsdAbstractLayerItem::gradientOpacity() const
{
    return d->gradientOpacity;
}

QCborMap QPsdAbstractLayerItem::dropShadow() const
{
    return d->dropShadow;
}

QPsdBorder *QPsdAbstractLayerItem::border() const
{
    return d->border.data();
}

QPsdPatternFill *QPsdAbstractLayerItem::patternFill() const
{
    return d->patternFill.data();
}

QPsdAbstractLayerItem::PathInfo QPsdAbstractLayerItem::vectorMask() const
{
    return d->vectorMask;
}

QImage QPsdAbstractLayerItem::image() const
{
    return d->image;
}

QImage QPsdAbstractLayerItem::transparencyMask() const
{
    return d->transparencyMask;
}

QImage QPsdAbstractLayerItem::layerMask() const
{
    return d->layerMask;
}

QRect QPsdAbstractLayerItem::layerMaskRect() const
{
    return d->layerMaskRect;
}

quint8 QPsdAbstractLayerItem::layerMaskDefaultColor() const
{
    return d->layerMaskDefaultColor;
}

QPsdLinkedLayer::LinkedFile QPsdAbstractLayerItem::linkedFile() const
{
    return d->linkedFile;
}

void QPsdAbstractLayerItem::setLinkedFile(const QPsdLinkedLayer::LinkedFile &linkedFile)
{
    d->linkedFile = linkedFile;
}

QPsdAbstractLayerItem::PathInfo QPsdAbstractLayerItem::parseShape(const QPsdVectorMaskSetting &vms) const
{
    PathInfo ret;
    if (vms.type() == QPsdVectorMaskSetting::Unknown)
        return ret;

    const QPointF topLeft = d->rect.topLeft();
    const QSizeF size = d->documentSize;
    auto transform = [&](const QPointF &point) {
        return QPointF(point.x() * size.width() - topLeft.x(), point.y() * size.height() - topLeft.y());
    };

    // check if the path is rectangle
    const auto subPathList = vms.subPathList();
    for (const auto &pathInfo : subPathList) {
        ret.type = PathInfo::Path;
        // Use per-path fill rule from the subpath length record
        switch (pathInfo.fillRule) {
        case QPsdVectorMaskSetting::EvenOdd:
            ret.path.setFillRule(Qt::OddEvenFill);
            break;
        case QPsdVectorMaskSetting::NonZero:
            ret.path.setFillRule(Qt::WindingFill);
            break;
        default:
            ret.path.setFillRule(Qt::OddEvenFill);
            break;
        }

        QPainterPath currentPath;
        currentPath.setFillRule(ret.path.fillRule());
        QPsdVectorMaskSetting::BezierPath initialPath;
        QPsdVectorMaskSetting::BezierPath lastPath;
        for (const auto &path : pathInfo.subPath) {
            // qDebug() << path.type << path.anchor << path.preceding << path.leaving << (path.anchor == path.preceding) << (path.anchor == path.leaving);
            if (initialPath.type == QPsdVectorMaskSetting::BezierPath::Empty) {
                QPointF anchor = transform(path.anchor);
                currentPath.moveTo(anchor);
                initialPath = path;
            } else {
                QPointF c1 = transform(lastPath.leaving);
                QPointF c2 = transform(path.preceding);
                QPointF anchor = transform(path.anchor);
                currentPath.cubicTo(c1, c2, anchor);
            }
            lastPath = path;
        }
        if (vms.type() == QPsdVectorMaskSetting::Closed) {
            QPointF c1 = transform(lastPath.leaving);
            QPointF c2 = transform(initialPath.preceding);
            QPointF anchor = transform(initialPath.anchor);
            currentPath.cubicTo(c1, c2, anchor);
        }

        if (ret.path.isEmpty()) {
            ret.path = currentPath;
        } else {
            switch (pathInfo.operation) {
            case QPsdVectorMaskSetting::PathInfo::Xor:
                // ret ^= currentPath;
                qWarning() << "Xor operation is not supported";
                break;
            case QPsdVectorMaskSetting::PathInfo::Or:
                ret.path |= currentPath;
                break;
            case QPsdVectorMaskSetting::PathInfo::NotOr:
                ret.path -= currentPath;
                break;
            case QPsdVectorMaskSetting::PathInfo::And:
                ret.path &= currentPath;
                break;
            }
        }
    }

    if (subPathList.count() == 1) {
        const auto pathInfo = subPathList.first();
        switch (pathInfo.type) {
        case QPsdVectorMaskSetting::PathInfo::Rectangle:
            ret.type = PathInfo::Rectangle;
            ret.rect = QRectF(transform(pathInfo.rect.topLeft()), transform(pathInfo.rect.bottomRight())).normalized();
            break;
        case QPsdVectorMaskSetting::PathInfo::Circle:
            ret.type = PathInfo::RoundedRectangle;
            ret.rect = QRectF(transform(pathInfo.rect.topLeft()), transform(pathInfo.rect.bottomRight())).normalized();
            ret.radius = ret.rect.width() / 2;
            break;
        case QPsdVectorMaskSetting::PathInfo::RoundedRectangle:
            ret.type = PathInfo::RoundedRectangle;
            ret.rect = QRectF(transform(pathInfo.rect.topLeft()), transform(pathInfo.rect.bottomRight())).normalized();
            ret.radius = pathInfo.radius * size.width();
            break;
        default:
            break;
        }
    }

    return ret;
}

QVariantList QPsdAbstractLayerItem::effects() const
{
    return d->effects;
}

QT_END_NAMESPACE
