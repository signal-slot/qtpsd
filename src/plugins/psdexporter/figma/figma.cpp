// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/qpsdexporterplugin.h>

#include <QtCore/QBuffer>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QtMath>

#include <QtGui/QImage>

#include <QtPsdGui/QPsdBorder>

QT_BEGIN_NAMESPACE

class QPsdExporterFigmaPlugin : public QPsdExporterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdExporterFactoryInterface" FILE "figma.json")
public:
    int priority() const override { return 900; }
    QIcon icon() const override {
        return QIcon::fromTheme("application-json"_L1);
    }
    QString name() const override {
        return tr("&Figma");
    }
    ExportType exportType() const override { return QPsdExporterPlugin::File; }
    QHash<QString, QString> filters() const override {
        return {{ tr("Figma JSON (*.figma.json)"), ".figma.json" }};
    }
    bool exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const override;

private:
    // Blend mode string conversion
    static QString blendModeToFigma(const QPsdAbstractLayerItem *item);

    // Color helpers
    static QJsonObject colorToJson(const QColor &color);
    static QJsonArray solidFill(const QColor &color, qreal opacity = 1.0);
    static QJsonArray brushToFills(const QBrush &brush, const QRectF &bounds, qreal opacity = 1.0);
    static QJsonArray gradientToFills(const QGradient *gradient, const QRectF &bounds, qreal opacity = 1.0);

    // SVG path from PathCommand list
    static QString commandsToSvgPath(const QList<PathCommand> &commands);

    // Effects
    static QJsonArray buildEffects(const QPsdAbstractLayerItem *item);

    // Bounding box
    static QJsonObject boundingBox(const QRect &rect);

    // Image to base64 PNG
    static QString imageToBase64(const QImage &image);

    // Node builders
    QJsonObject buildTextNode(const QPsdTextLayerItem *text, const QModelIndex &index) const;
    QJsonObject buildShapeNode(const QPsdShapeLayerItem *shape, const QModelIndex &index) const;
    QJsonObject buildImageNode(const QPsdImageLayerItem *image, const QModelIndex &index) const;
    QJsonObject buildFolderNode(const QPsdFolderLayerItem *folder, const QModelIndex &index,
                                const QPsdExporterTreeItemModel *model, QJsonObject &images) const;

    // Recursive tree traversal
    QJsonObject buildNode(const QModelIndex &index, const QPsdExporterTreeItemModel *model,
                          QJsonObject &images) const;
};

// ─── Helpers ──────────────────────────────────────────────────────────────

QString QPsdExporterFigmaPlugin::blendModeToFigma(const QPsdAbstractLayerItem *item)
{
    Q_UNUSED(item)
    return "NORMAL"_L1;
}

QJsonObject QPsdExporterFigmaPlugin::colorToJson(const QColor &color)
{
    return QJsonObject {
        {"r"_L1, color.redF()},
        {"g"_L1, color.greenF()},
        {"b"_L1, color.blueF()},
        {"a"_L1, color.alphaF()},
    };
}

QJsonArray QPsdExporterFigmaPlugin::solidFill(const QColor &color, qreal opacity)
{
    QColor c = color;
    const qreal alpha = c.alphaF();
    c.setAlphaF(1.0);
    return QJsonArray {
        QJsonObject {
            {"type"_L1, "SOLID"_L1},
            {"color"_L1, colorToJson(c)},
            {"opacity"_L1, alpha * opacity},
            {"visible"_L1, true},
        }
    };
}

QJsonArray QPsdExporterFigmaPlugin::brushToFills(const QBrush &brush, const QRectF &bounds, qreal opacity)
{
    if (brush.style() == Qt::NoBrush)
        return {};

    if (brush.gradient())
        return gradientToFills(brush.gradient(), bounds, opacity);

    return solidFill(brush.color(), opacity);
}

QJsonArray QPsdExporterFigmaPlugin::gradientToFills(const QGradient *gradient, const QRectF &bounds, qreal opacity)
{
    if (!gradient)
        return {};

    QJsonObject fill;
    fill["visible"_L1] = true;
    fill["opacity"_L1] = opacity;

    QJsonArray gradientStops;
    for (const auto &stop : gradient->stops()) {
        QJsonObject s;
        s["position"_L1] = stop.first;
        s["color"_L1] = colorToJson(stop.second);
        gradientStops.append(s);
    }
    fill["gradientStops"_L1] = gradientStops;

    const qreal w = bounds.width();
    const qreal h = bounds.height();

    if (gradient->type() == QGradient::LinearGradient) {
        const auto *lg = static_cast<const QLinearGradient *>(gradient);
        fill["type"_L1] = "GRADIENT_LINEAR"_L1;

        QJsonArray handles;
        handles.append(QJsonObject {
            {"x"_L1, w > 0 ? lg->start().x() / w : 0.0},
            {"y"_L1, h > 0 ? lg->start().y() / h : 0.0},
        });
        handles.append(QJsonObject {
            {"x"_L1, w > 0 ? lg->finalStop().x() / w : 1.0},
            {"y"_L1, h > 0 ? lg->finalStop().y() / h : 1.0},
        });
        // Third handle: perpendicular point (for the gradient width)
        const qreal dx = lg->finalStop().x() - lg->start().x();
        const qreal dy = lg->finalStop().y() - lg->start().y();
        handles.append(QJsonObject {
            {"x"_L1, w > 0 ? (lg->start().x() - dy) / w : 0.0},
            {"y"_L1, h > 0 ? (lg->start().y() + dx) / h : 1.0},
        });
        fill["gradientHandlePositions"_L1] = handles;
    } else if (gradient->type() == QGradient::RadialGradient) {
        const auto *rg = static_cast<const QRadialGradient *>(gradient);
        fill["type"_L1] = "GRADIENT_RADIAL"_L1;

        const qreal cx = rg->center().x();
        const qreal cy = rg->center().y();
        const qreal r = rg->radius();
        QJsonArray handles;
        handles.append(QJsonObject {
            {"x"_L1, w > 0 ? cx / w : 0.5},
            {"y"_L1, h > 0 ? cy / h : 0.5},
        });
        handles.append(QJsonObject {
            {"x"_L1, w > 0 ? (cx + r) / w : 1.0},
            {"y"_L1, h > 0 ? cy / h : 0.5},
        });
        handles.append(QJsonObject {
            {"x"_L1, w > 0 ? cx / w : 0.5},
            {"y"_L1, h > 0 ? (cy + r) / h : 1.0},
        });
        fill["gradientHandlePositions"_L1] = handles;
    } else if (gradient->type() == QGradient::ConicalGradient) {
        const auto *cg = static_cast<const QConicalGradient *>(gradient);
        fill["type"_L1] = "GRADIENT_ANGULAR"_L1;

        const qreal cx = cg->center().x();
        const qreal cy = cg->center().y();
        const qreal angle = cg->angle() * M_PI / 180.0;
        const qreal r = qMin(w, h) / 2.0;
        QJsonArray handles;
        handles.append(QJsonObject {
            {"x"_L1, w > 0 ? cx / w : 0.5},
            {"y"_L1, h > 0 ? cy / h : 0.5},
        });
        handles.append(QJsonObject {
            {"x"_L1, w > 0 ? (cx + r * std::cos(angle)) / w : 1.0},
            {"y"_L1, h > 0 ? (cy + r * std::sin(angle)) / h : 0.5},
        });
        fill["gradientHandlePositions"_L1] = handles;

        // Figma sweeps CW, Qt sweeps CCW: reverse stops
        QJsonArray reversedStops;
        for (int i = gradientStops.size() - 1; i >= 0; --i) {
            auto s = gradientStops[i].toObject();
            s["position"_L1] = 1.0 - s["position"_L1].toDouble();
            reversedStops.append(s);
        }
        fill["gradientStops"_L1] = reversedStops;
    }

    return QJsonArray { fill };
}

QString QPsdExporterFigmaPlugin::commandsToSvgPath(const QList<PathCommand> &commands)
{
    QString path;
    for (const auto &cmd : commands) {
        switch (cmd.type) {
        case PathCommand::MoveTo:
            path += u"M %1 %2 "_s.arg(cmd.x).arg(cmd.y);
            break;
        case PathCommand::LineTo:
            path += u"L %1 %2 "_s.arg(cmd.x).arg(cmd.y);
            break;
        case PathCommand::CubicTo:
            path += u"C %1 %2 %3 %4 %5 %6 "_s
                .arg(cmd.c1x).arg(cmd.c1y)
                .arg(cmd.c2x).arg(cmd.c2y)
                .arg(cmd.x).arg(cmd.y);
            break;
        case PathCommand::Close:
            path += u"Z "_s;
            break;
        }
    }
    return path.trimmed();
}

QJsonArray QPsdExporterFigmaPlugin::buildEffects(const QPsdAbstractLayerItem *item)
{
    QJsonArray effects;

    // Drop shadow
    const auto dropShadowInfo = parseDropShadow(item->dropShadow());
    if (dropShadowInfo) {
        const auto offset = dropShadowOffset(*dropShadowInfo);
        QJsonObject effect;
        effect["type"_L1] = "DROP_SHADOW"_L1;
        effect["visible"_L1] = true;
        effect["color"_L1] = colorToJson(dropShadowInfo->color);
        effect["offset"_L1] = QJsonObject {
            {"x"_L1, offset.x()},
            {"y"_L1, offset.y()},
        };
        effect["radius"_L1] = dropShadowInfo->blur;
        effect["spread"_L1] = dropShadowInfo->spread;
        effects.append(effect);
    }

    // Inner shadow
    const auto innerShadowInfo = parseDropShadow(item->innerShadow());
    if (innerShadowInfo) {
        const auto offset = dropShadowOffset(*innerShadowInfo);
        QJsonObject effect;
        effect["type"_L1] = "INNER_SHADOW"_L1;
        effect["visible"_L1] = true;
        effect["color"_L1] = colorToJson(innerShadowInfo->color);
        effect["offset"_L1] = QJsonObject {
            {"x"_L1, offset.x()},
            {"y"_L1, offset.y()},
        };
        effect["radius"_L1] = innerShadowInfo->blur;
        effects.append(effect);
    }

    // Layer blur
    if (item->layerBlur() > 0) {
        QJsonObject effect;
        effect["type"_L1] = "LAYER_BLUR"_L1;
        effect["visible"_L1] = true;
        effect["radius"_L1] = item->layerBlur();
        effects.append(effect);
    }

    return effects;
}

QJsonObject QPsdExporterFigmaPlugin::boundingBox(const QRect &rect)
{
    return QJsonObject {
        {"x"_L1, rect.x()},
        {"y"_L1, rect.y()},
        {"width"_L1, rect.width()},
        {"height"_L1, rect.height()},
    };
}

QString QPsdExporterFigmaPlugin::imageToBase64(const QImage &image)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QString::fromLatin1(data.toBase64());
}

// ─── Node Builders ────────────────────────────────────────────────────────

QJsonObject QPsdExporterFigmaPlugin::buildTextNode(const QPsdTextLayerItem *text, const QModelIndex &index) const
{
    Q_UNUSED(index)
    QJsonObject node;
    node["type"_L1] = "TEXT"_L1;
    node["name"_L1] = text->name();
    node["id"_L1] = QString::number(text->id());
    node["visible"_L1] = text->isVisible();
    node["absoluteBoundingBox"_L1] = boundingBox(text->rect());

    const auto optOpacity = displayOpacity(text);
    if (optOpacity)
        node["opacity"_L1] = *optOpacity;

    node["blendMode"_L1] = blendModeToFigma(text);

    // Characters (full text)
    const auto runs = text->runs();
    QString characters;
    for (const auto &run : runs)
        characters += run.text;
    node["characters"_L1] = characters;

    // Global style from first run
    if (!runs.isEmpty()) {
        const auto &firstRun = runs.first();
        QJsonObject style;
        style["fontFamily"_L1] = firstRun.originalFontName.isEmpty()
            ? firstRun.font.family() : firstRun.originalFontName;
        style["fontSize"_L1] = firstRun.font.pointSizeF();
        style["fontWeight"_L1] = static_cast<int>(firstRun.font.weight());
        if (firstRun.font.italic())
            style["italic"_L1] = true;
        if (firstRun.font.letterSpacing() != 0)
            style["letterSpacing"_L1] = firstRun.font.letterSpacing();
        if (firstRun.lineHeight > 0)
            style["lineHeightPx"_L1] = firstRun.lineHeight;

        // Text alignment
        style["textAlignHorizontal"_L1] = horizontalAlignmentString(
            firstRun.alignment,
            {"LEFT"_L1, "RIGHT"_L1, "CENTER"_L1, "JUSTIFIED"_L1});
        style["textAlignVertical"_L1] = verticalAlignmentString(
            firstRun.alignment,
            {"TOP"_L1, "BOTTOM"_L1, "CENTER"_L1});

        // Text auto-resize
        if (text->textType() == QPsdTextLayerItem::TextType::ParagraphText)
            style["textAutoResize"_L1] = "NONE"_L1;
        else
            style["textAutoResize"_L1] = "WIDTH_AND_HEIGHT"_L1;

        node["style"_L1] = style;

        // Fills from first run color
        node["fills"_L1] = solidFill(firstRun.color);
    }

    // Styled text segments
    QJsonArray segments;
    for (const auto &run : runs) {
        QJsonObject seg;
        seg["characters"_L1] = run.text;
        seg["fontFamily"_L1] = run.originalFontName.isEmpty()
            ? run.font.family() : run.originalFontName;
        seg["fontSize"_L1] = run.font.pointSizeF();
        seg["fontWeight"_L1] = static_cast<int>(run.font.weight());
        if (run.font.italic())
            seg["italic"_L1] = true;
        if (run.font.letterSpacing() != 0)
            seg["letterSpacing"_L1] = run.font.letterSpacing();
        if (run.lineHeight > 0)
            seg["lineHeightPx"_L1] = run.lineHeight;

        seg["fills"_L1] = solidFill(run.color);

        if (run.font.underline())
            seg["textDecoration"_L1] = "UNDERLINE"_L1;
        else if (run.font.strikeOut())
            seg["textDecoration"_L1] = "STRIKETHROUGH"_L1;

        segments.append(seg);
    }
    node["styledTextSegments"_L1] = segments;

    // Effects
    const auto effects = buildEffects(text);
    if (!effects.isEmpty())
        node["effects"_L1] = effects;

    return node;
}

QJsonObject QPsdExporterFigmaPlugin::buildShapeNode(const QPsdShapeLayerItem *shape, const QModelIndex &index) const
{
    Q_UNUSED(index)
    const auto pathInfo = shape->pathInfo();
    const QRect rect = shape->rect();
    const QRectF bounds(0, 0, rect.width(), rect.height());

    QJsonObject node;
    node["name"_L1] = shape->name();
    node["id"_L1] = QString::number(shape->id());
    node["visible"_L1] = shape->isVisible();
    node["absoluteBoundingBox"_L1] = boundingBox(rect);

    const auto optOpacity = displayOpacity(shape);
    if (optOpacity)
        node["opacity"_L1] = *optOpacity;

    node["blendMode"_L1] = blendModeToFigma(shape);

    // Determine node type based on path info
    switch (pathInfo.type) {
    case QPsdAbstractLayerItem::PathInfo::Rectangle:
        node["type"_L1] = "RECTANGLE"_L1;
        break;
    case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
        node["type"_L1] = "RECTANGLE"_L1;
        node["cornerRadius"_L1] = pathInfo.radius;
        node["rectangleCornerRadii"_L1] = QJsonArray {
            pathInfo.radius, pathInfo.radius,
            pathInfo.radius, pathInfo.radius
        };
        break;
    case QPsdAbstractLayerItem::PathInfo::Path:
    case QPsdAbstractLayerItem::PathInfo::None:
        node["type"_L1] = "VECTOR"_L1;
        break;
    }

    // Fills
    const auto *gradient = effectiveGradient(shape);
    if (gradient) {
        node["fills"_L1] = gradientToFills(gradient, bounds, shape->gradientOpacity());
    } else if (shape->brush().style() != Qt::NoBrush) {
        node["fills"_L1] = brushToFills(shape->brush(), bounds, shape->fillOpacity());
    } else {
        node["fills"_L1] = QJsonArray {};
    }

    // Strokes
    if (shape->pen().style() != Qt::NoPen) {
        const auto pen = shape->pen();
        QJsonArray strokes;
        QColor strokeColor = pen.color();
        const qreal strokeAlpha = strokeColor.alphaF();
        strokeColor.setAlphaF(1.0);
        strokes.append(QJsonObject {
            {"type"_L1, "SOLID"_L1},
            {"color"_L1, colorToJson(strokeColor)},
            {"opacity"_L1, strokeAlpha},
            {"visible"_L1, true},
        });
        node["strokes"_L1] = strokes;
        node["strokeWeight"_L1] = pen.widthF();

        switch (shape->strokeAlignment()) {
        case QPsdShapeLayerItem::StrokeInside:
            node["strokeAlign"_L1] = "INSIDE"_L1;
            break;
        case QPsdShapeLayerItem::StrokeOutside:
            node["strokeAlign"_L1] = "OUTSIDE"_L1;
            break;
        case QPsdShapeLayerItem::StrokeCenter:
        default:
            node["strokeAlign"_L1] = "CENTER"_L1;
            break;
        }
    }

    // Fill geometry (SVG path data) for VECTOR types
    if (pathInfo.type == QPsdAbstractLayerItem::PathInfo::Path
        || pathInfo.type == QPsdAbstractLayerItem::PathInfo::None) {
        const auto commands = pathToCommands(pathInfo.path);
        const QString svgPath = commandsToSvgPath(commands);
        if (!svgPath.isEmpty()) {
            QJsonArray fillGeometry;
            fillGeometry.append(QJsonObject {
                {"path"_L1, svgPath},
                {"windingRule"_L1, "NONZERO"_L1},
            });
            node["fillGeometry"_L1] = fillGeometry;
        }
    }

    // Effects
    const auto effects = buildEffects(shape);
    if (!effects.isEmpty())
        node["effects"_L1] = effects;

    return node;
}

QJsonObject QPsdExporterFigmaPlugin::buildImageNode(const QPsdImageLayerItem *image, const QModelIndex &index) const
{
    Q_UNUSED(index)
    QJsonObject node;
    node["type"_L1] = "RECTANGLE"_L1;
    node["name"_L1] = image->name();
    node["id"_L1] = QString::number(image->id());
    node["visible"_L1] = image->isVisible();
    node["absoluteBoundingBox"_L1] = boundingBox(image->rect());

    const auto optOpacity = displayOpacity(image);
    if (optOpacity)
        node["opacity"_L1] = *optOpacity;

    node["blendMode"_L1] = blendModeToFigma(image);

    // Image fill with imageRef
    const QString imageRef = QString::number(image->id());
    QJsonArray fills;
    fills.append(QJsonObject {
        {"type"_L1, "IMAGE"_L1},
        {"scaleMode"_L1, "FILL"_L1},
        {"imageRef"_L1, imageRef},
        {"visible"_L1, true},
    });
    node["fills"_L1] = fills;

    // Effects
    const auto effects = buildEffects(image);
    if (!effects.isEmpty())
        node["effects"_L1] = effects;

    return node;
}

QJsonObject QPsdExporterFigmaPlugin::buildFolderNode(const QPsdFolderLayerItem *folder,
                                                      const QModelIndex &index,
                                                      const QPsdExporterTreeItemModel *model,
                                                      QJsonObject &images) const
{
    QJsonObject node;
    node["name"_L1] = folder->name();
    node["id"_L1] = QString::number(folder->id());
    node["visible"_L1] = folder->isVisible();

    const auto optOpacity = displayOpacity(folder);
    if (optOpacity)
        node["opacity"_L1] = *optOpacity;

    node["blendMode"_L1] = blendModeToFigma(folder);

    const QRect artboardRect = folder->artboardRect();
    const bool isArtboard = artboardRect.isValid();

    if (isArtboard) {
        // Top-level frame (artboard)
        node["type"_L1] = "FRAME"_L1;
        node["absoluteBoundingBox"_L1] = boundingBox(artboardRect);

        // Background fill
        const QColor bg = folder->artboardBackground();
        if (bg.isValid() && bg.alpha() > 0) {
            node["fills"_L1] = solidFill(bg);
        }
        node["clipsContent"_L1] = true;
    } else {
        // Check if this folder has a vector mask (clip)
        const auto vectorMask = folder->vectorMask();
        if (vectorMask.type != QPsdAbstractLayerItem::PathInfo::None) {
            node["type"_L1] = "FRAME"_L1;
            node["absoluteBoundingBox"_L1] = boundingBox(folder->rect());
            node["clipsContent"_L1] = true;
            node["fills"_L1] = QJsonArray {};

            if (vectorMask.type == QPsdAbstractLayerItem::PathInfo::RoundedRectangle) {
                node["cornerRadius"_L1] = vectorMask.radius;
            }
        } else {
            node["type"_L1] = "GROUP"_L1;
            node["absoluteBoundingBox"_L1] = boundingBox(folder->rect());
        }
    }

    // Children
    QJsonArray children;
    for (int i = 0; i < model->rowCount(index); i++) {
        const auto childIndex = model->index(i, 0, index);
        QJsonObject child = buildNode(childIndex, model, images);
        if (!child.isEmpty())
            children.append(child);
    }
    node["children"_L1] = children;

    // Effects
    const auto effects = buildEffects(folder);
    if (!effects.isEmpty())
        node["effects"_L1] = effects;

    return node;
}

// ─── Recursive Node Builder ──────────────────────────────────────────────

QJsonObject QPsdExporterFigmaPlugin::buildNode(const QModelIndex &index,
                                                const QPsdExporterTreeItemModel *model,
                                                QJsonObject &images) const
{
    const auto *item = model->layerItem(index);
    if (!item)
        return {};

    const auto hint = model->layerHint(index);
    if (hint.type == QPsdExporterTreeItemModel::ExportHint::Skip)
        return {};

    QJsonObject node;

    switch (item->type()) {
    case QPsdAbstractLayerItem::Text:
        node = buildTextNode(dynamic_cast<const QPsdTextLayerItem *>(item), index);
        break;
    case QPsdAbstractLayerItem::Shape:
        node = buildShapeNode(dynamic_cast<const QPsdShapeLayerItem *>(item), index);
        break;
    case QPsdAbstractLayerItem::Image: {
        const auto *imageItem = dynamic_cast<const QPsdImageLayerItem *>(item);
        node = buildImageNode(imageItem, index);

        // Store image data in the images section
        QImage qimage = imageItem->linkedImage();
        if (qimage.isNull())
            qimage = imageItem->image();
        if (!qimage.isNull()) {
            images[QString::number(imageItem->id())] = imageToBase64(qimage);
        }
        break;
    }
    case QPsdAbstractLayerItem::Folder:
        node = buildFolderNode(dynamic_cast<const QPsdFolderLayerItem *>(item), index, model, images);
        break;
    default:
        break;
    }

    return node;
}

// ─── Export Entry Point ──────────────────────────────────────────────────

bool QPsdExporterFigmaPlugin::exportTo(const QPsdExporterTreeItemModel *model,
                                        const QString &to,
                                        const QVariantMap &hint) const
{
    Q_UNUSED(hint)

    QJsonObject images;

    // Build top-level children
    QJsonArray topChildren;
    QModelIndex rootIndex;
    for (int i = 0; i < model->rowCount(rootIndex); i++) {
        const auto childIndex = model->index(i, 0, rootIndex);
        QJsonObject child = buildNode(childIndex, model, images);
        if (!child.isEmpty())
            topChildren.append(child);
    }

    // Wrap in CANVAS → DOCUMENT structure
    QJsonObject canvas;
    canvas["type"_L1] = "CANVAS"_L1;
    canvas["name"_L1] = "Page 1"_L1;
    canvas["children"_L1] = topChildren;

    QJsonObject document;
    document["type"_L1] = "DOCUMENT"_L1;
    document["children"_L1] = QJsonArray { canvas };

    QJsonObject root;
    root["document"_L1] = document;

    if (!images.isEmpty())
        root["images"_L1] = images;

    QJsonDocument doc;
    doc.setObject(root);

    QFile file(to);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(doc.toJson());
    file.close();
    return true;
}

QT_END_NAMESPACE

#include "figma.moc"
