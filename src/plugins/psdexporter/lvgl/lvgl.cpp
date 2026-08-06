// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/qpsdexporterplugin.h>
#include <QtPsdExporter/qpsdimagestore.h>

#include <QtCore/QCborMap>
#include <QtCore/QDir>
#include <QtCore/QTextStream>

#include <optional>
#include <QtGui/QBrush>
#include <QtGui/QConicalGradient>
#include <QtGui/QFontMetrics>
#include <QtGui/QLinearGradient>
#include <QtGui/QPen>
#include <QtGui/QRadialGradient>

#include <QtPsdGui/QPsdBorder>

QT_BEGIN_NAMESPACE

// Generates plain C code against the MIT-licensed LVGL C API.
// The LVGL XML format is intentionally not used: since LVGL 9.5 the XML
// engine is a commercial component and the XML specification's license
// prohibits third-party tools from writing it.
class QPsdExporterLvglPlugin : public QPsdExporterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdExporterFactoryInterface" FILE "lvgl.json")
public:
    int priority() const override { return 50; }
    QIcon icon() const override {
        return QIcon(":/lvgl/lvgl.png");
    }
    QString name() const override {
        return tr("&LVGL");
    }
    ExportType exportType() const override { return QPsdExporterPlugin::Directory; }

    bool exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const ExportConfig &config) const override;

private:
    struct Element {
        QString type;
        QString id;
        QMap<QString, QString> attributes;
        QList<Element> children;
    };

    mutable QList<QPair<QString, QString>> exportedImages; // name, filename

    struct GradientStop {
        int offset; // 0-255
        quint32 rgb; // 0xRRGGBB
        int opa; // 0-255
    };
    struct GradientDef {
        QString name;
        enum Type { Linear, Radial, Conical } type = Linear;
        // Linear: (p1x,p1y)-(p2x,p2y) start/end points
        // Radial: (p1x,p1y) center, (p2x,p2y) point on the end circle
        // Conical: (p1x,p1y) center, p2x start angle, p2y end angle (degrees)
        int p1x = 0, p1y = 0, p2x = 0, p2y = 0;
        QList<GradientStop> stops;
    };
    mutable QList<GradientDef> exportedGradients;
    mutable int gradientCounter = 0;

    using ExportData = QList<QPair<QString, QString>>; // id, type for API props

    bool traverseTree(const QModelIndex &index, Element *parent, ExportData *exports, std::optional<QPsdExporterTreeItemModel::ExportHint::Type> hintOverload = std::nullopt) const;

    bool outputRect(const QRect &rect, Element *element) const;
    bool outputBase(const QModelIndex &index, Element *element, QRect rectBounds = {}) const;
    bool outputFolder(const QModelIndex &folderIndex, Element *element, ExportData *exports) const;
    bool outputText(const QModelIndex &textIndex, Element *element) const;
    bool outputShape(const QModelIndex &shapeIndex, Element *element) const;
    bool outputImage(const QModelIndex &imageIndex, Element *element) const;

    QString generateGradientName(const QString &layerName) const;
    GradientDef makeGradientDef(const QString &name, const QGradient *gradient) const;

    // Photoshop blend modes LVGL can express natively
    static QString lvglBlendMode(QPsdBlend::Mode mode) {
        switch (mode) {
        case QPsdBlend::Mode::Multiply: return "LV_BLEND_MODE_MULTIPLY"_L1;
        case QPsdBlend::Mode::LinearDodge: return "LV_BLEND_MODE_ADDITIVE"_L1;
        case QPsdBlend::Mode::Subtract: return "LV_BLEND_MODE_SUBTRACTIVE"_L1;
        case QPsdBlend::Mode::Difference: return "LV_BLEND_MODE_DIFFERENCE"_L1;
        default: return QString();
        }
    }
    static QList<QPsdBlend::Mode> nativeBlendModes() {
        return { QPsdBlend::Mode::Multiply, QPsdBlend::Mode::LinearDodge,
                 QPsdBlend::Mode::Subtract, QPsdBlend::Mode::Difference };
    }

    bool saveHeader(const QString &baseName, const ExportData &exports) const;
    bool saveSource(const QString &baseName, const Element &root, const ExportData &exports, const QSize &targetSize) const;

    static QString escapeCString(const QString &text);
    static QString fontExpression(const QString &fontValue);
    QString imageSourcePath(const QString &imageName) const;
    void emitGradient(QTextStream &out, const GradientDef &grad) const;
    void emitElement(QTextStream &out, const Element &element, const QString &parentVar,
                     int indent, int *counter, const QSet<QString> &exportIds) const;
};

bool QPsdExporterLvglPlugin::exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const ExportConfig &config) const
{
    if (!initializeExport(model, to, config)) {
        return false;
    }
    exportedImages.clear();
    exportedGradients.clear();
    gradientCounter = 0;
    const QSize targetSize = config.targetSize.isEmpty() ? canvasSize() : config.targetSize;

    ExportData exports;

    Element view;
    view.type = "view";
    outputRect(QRect(QPoint(0, 0), targetSize), &view);
    view.attributes.insert("style_pad_all", "0");
    view.attributes.insert("scrollbar_mode", "off");

    if (!needsRasterFallback(nativeBlendModes())) {
        for (int i = model->rowCount(QModelIndex {}) - 1; i >= 0; i--) {
            QModelIndex childIndex = model->index(i, 0, QModelIndex {});
            if (!traverseTree(childIndex, &view, &exports, std::nullopt))
                return false;
        }
    }

    // Flattened PSD fallback: used when no layers were produced or the
    // document needs blend modes/adjustment layers LVGL cannot express
    if (view.children.isEmpty()) {
        const QImage merged = model->guiLayerTreeItemModel()->mergedImage();
        if (!merged.isNull()) {
            imageStore = QPsdImageStore(dir, "images"_L1);
            const QString name = imageStore.save("merged.png"_L1, merged, "PNG");
            QFileInfo fi(name);
            QString imageName = fi.completeBaseName();
            exportedImages.append(qMakePair(imageName, name));
            Element img;
            img.type = "lv_image";
            outputRect(QRect { QPoint { 0, 0 }, canvasSize() }, &img);
            img.attributes.insert("src", imageName);
            view.children.append(img);
        }
    }

    const QString baseName = "main_screen"_L1;
    if (!saveHeader(baseName, exports))
        return false;

    if (!saveSource(baseName, view, exports, targetSize))
        return false;

    return true;
}

bool QPsdExporterLvglPlugin::outputBase(const QModelIndex &index, Element *element, QRect rectBounds) const
{
    const QPsdAbstractLayerItem *item = model()->layerItem(index);
    QRect rect;
    if (rectBounds.isEmpty()) {
        rect = item->rect();
    } else {
        rect = rectBounds;
    }

    const qreal opacity = item->opacity();
    const qreal fillOpacity = item->fillOpacity();
    const qreal combinedOpacity = opacity * fillOpacity;
    if (combinedOpacity < 1.0) {
        element->attributes.insert("style_opa", QString::number(qRound(combinedOpacity * 255)));
    }

    outputRect(rect, element);
    return true;
}

bool QPsdExporterLvglPlugin::outputRect(const QRect &rect, Element *element) const
{
    element->attributes.insert("x", QString::number(rect.x()));
    element->attributes.insert("y", QString::number(rect.y()));
    element->attributes.insert("width", QString::number(rect.width()));
    element->attributes.insert("height", QString::number(rect.height()));
    return true;
}

bool QPsdExporterLvglPlugin::outputFolder(const QModelIndex &folderIndex, Element *element, ExportData *exports) const
{
    const auto *folder = dynamic_cast<const QPsdFolderLayerItem *>(model()->layerItem(folderIndex));
    // Only set type and base attributes if this is a new element (not the root view)
    if (element->type.isEmpty()) {
        element->type = "lv_obj";
        if (!outputBase(folderIndex, element))
            return false;
    }

    if (folder->artboardRect().isValid() && folder->artboardBackground() != Qt::transparent) {
        Element artboard;
        artboard.type = "lv_obj";
        QRect bgRect(QPoint(0, 0), folder->artboardRect().size());
        outputRect(bgRect, &artboard);
        QColor color = folder->artboardBackground();
        artboard.attributes.insert("style_bg_color", u"0x%1"_s.arg(color.rgb() & 0xFFFFFF, 6, 16, QChar('0')));
        artboard.attributes.insert("style_bg_opa", "255");
        element->children.append(artboard);
    }

    for (int i = model()->rowCount(folderIndex) - 1; i >= 0; i--) {
        QModelIndex childIndex = model()->index(i, 0, folderIndex);
        if (!traverseTree(childIndex, element, exports, std::nullopt))
            return false;
    }
    return true;
}

bool QPsdExporterLvglPlugin::traverseTree(const QModelIndex &index, Element *parent, ExportData *exports, std::optional<QPsdExporterTreeItemModel::ExportHint::Type> hintOverload) const
{
    const QPsdAbstractLayerItem *item = model()->layerItem(index);
    const auto hint = model()->layerHint(index);
    const auto id = toSnakeCase(hint.id);
    auto type = hint.type;
    if (hintOverload.has_value()) {
        type = *hintOverload;
    }

    // Layer is consumed by a Native Button's textSource/imageSource; the
    // button emits its content, so don't emit this layer standalone.
    if (isMergedSource(index))
        return true;

    switch (type) {
    case QPsdExporterTreeItemModel::ExportHint::Embed: {
        Element element;
        element.id = id;
        outputBase(index, &element);
        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            if (!id.isEmpty()) {
                outputFolder(index, &element, exports);
            } else {
                outputFolder(index, parent, exports);
                return true;
            }
            break; }
        case QPsdAbstractLayerItem::Text: {
            outputText(index, &element);
            break; }
        case QPsdAbstractLayerItem::Shape: {
            outputShape(index, &element);
            break; }
        case QPsdAbstractLayerItem::Image: {
            outputImage(index, &element);
            break; }
        default:
            break;
        }

        if (element.type.isEmpty())
            return true;

        if (!hint.visible)
            element.attributes.insert("hidden", "true");

        const QString lvBlend = lvglBlendMode(item->record().blendMode());
        if (!lvBlend.isEmpty())
            element.attributes.insert("blend_mode", lvBlend);

        if (!id.isEmpty()) {
            if (hint.interactive) {
                Element touchArea;
                touchArea.type = "lv_button";
                touchArea.id = element.id;
                outputBase(index, &touchArea);
                // Remove default button styling
                touchArea.attributes.insert("style_bg_opa", "0");
                touchArea.attributes.insert("style_border_width", "0");
                touchArea.attributes.insert("style_shadow_width", "0");
                touchArea.attributes.insert("style_pad_all", "0"); // Important: no padding to avoid child clipping
                if (!hint.visible)
                    touchArea.attributes.insert("hidden", "true");
                element.id = QString();
                // Child position should be relative to parent (0,0)
                element.attributes.insert("x", "0");
                element.attributes.insert("y", "0");
                touchArea.children.append(element);
                parent->children.append(touchArea);
                if (hint.properties.contains("visible"))
                    exports->append({id, "bool"});
            } else {
                if (hint.properties.contains("visible"))
                    exports->append({id, "bool"});
                parent->children.append(element);
            }
        } else {
            parent->children.append(element);
        }
        break; }
    case QPsdExporterTreeItemModel::ExportHint::Native:
    case QPsdExporterTreeItemModel::ExportHint::Component: {
        Element element;
        element.id = id;
        outputBase(index, &element);
        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            outputFolder(index, &element, exports);
            break; }
        case QPsdAbstractLayerItem::Text: {
            outputText(index, &element);
            break; }
        case QPsdAbstractLayerItem::Shape: {
            outputShape(index, &element);
            break; }
        case QPsdAbstractLayerItem::Image: {
            outputImage(index, &element);
            break; }
        default:
            break;
        }

        if (element.type.isEmpty())
            return true;

        if (!hint.visible)
            element.attributes.insert("hidden", "true");
        if (!id.isEmpty() && hint.properties.contains("visible"))
            exports->append({id, "bool"});
        parent->children.append(element);
        break; }
    case QPsdExporterTreeItemModel::ExportHint::Merged:
    case QPsdExporterTreeItemModel::ExportHint::Skip:
        return true;
    }
    return true;
}

bool QPsdExporterLvglPlugin::outputText(const QModelIndex &textIndex, Element *element) const
{
    const auto *text = dynamic_cast<const QPsdTextLayerItem *>(model()->layerItem(textIndex));

    // Warped text: the layer raster contains Photoshop's warped rendering,
    // which lv_label cannot reproduce
    if (text->isWarped() && !text->image().isNull()) {
        QPsdImageStore imageStore(dir, "images"_L1);
        const QString name = imageStore.save(imageFileName(text->name(), "PNG"_L1), text->image(), "PNG");
        if (!name.isEmpty()) {
            QFileInfo fi(name);
            exportedImages.append(qMakePair(fi.completeBaseName(), name));
            element->type = "lv_image";
            if (!outputBase(textIndex, element))
                return false;
            element->attributes.insert("src", fi.completeBaseName());
            return true;
        }
    }

    const auto runs = text->runs();
    if (runs.isEmpty())
        return true;

    element->type = "lv_label";
    QRect rect = computeTextBounds(text);
    if (!outputBase(textIndex, element, rect))
        return false;

    QString fullText;
    for (const auto &run : runs) {
        fullText += run.text.trimmed();
    }
    element->attributes.insert("text", fullText);

    const auto &firstRun = runs.first();
    QColor color = firstRun.color;
    element->attributes.insert("style_text_color", u"0x%1"_s.arg(color.rgb() & 0xFFFFFF, 6, 16, QChar('0')));

    int fontSize = qRound(firstRun.font.pointSizeF());
    element->attributes.insert("style_text_font", u"montserrat %1"_s.arg(fontSize));

    element->attributes.insert("style_text_align",
        horizontalAlignmentString(firstRun.alignment, {"left"_L1, "right"_L1, "center"_L1, {}}));

    return true;
}

bool QPsdExporterLvglPlugin::outputShape(const QModelIndex &shapeIndex, Element *element) const
{
    const auto *shape = dynamic_cast<const QPsdShapeLayerItem *>(model()->layerItem(shapeIndex));
    const auto path = shape->pathInfo();

    auto outputGradientOrSolid = [&](Element *el) {
        const QGradient *g = effectiveGradient(shape);
        if (g) {
            el->attributes.insert("style_bg_opa", "255");
            QString gradName = generateGradientName(shape->name());
            exportedGradients.append(makeGradientDef(gradName, g));
            el->attributes.insert("style_bg_grad", gradName);
        } else if (shape->brush() != Qt::NoBrush) {
            QColor color = shape->brush().color();
            el->attributes.insert("style_bg_color", u"0x%1"_s.arg(color.rgb() & 0xFFFFFF, 6, 16, QChar('0')));
            el->attributes.insert("style_bg_opa", QString::number(color.alpha()));
        }
        // Remove default LVGL theme border for fill/shape elements
        el->attributes.insert("style_border_width", "0");
    };

    switch (path.type) {
    case QPsdAbstractLayerItem::PathInfo::None: {
        const QGradient *g = effectiveGradient(shape);
        if (g || shape->brush() != Qt::NoBrush) {
            element->type = "lv_obj";
            if (!outputBase(shapeIndex, element))
                return false;
            outputGradientOrSolid(element);
        } else {
            // No gradient and no brush — fall back to image
            QPsdImageStore imageStore(dir, "images"_L1);
            QImage qimage = shape->image();
            if (!qimage.isNull()) {
                QString name = imageStore.save(imageFileName(shape->name(), "PNG"_L1), qimage, "PNG");
                QFileInfo fi(name);
                QString imageName = fi.completeBaseName();
                exportedImages.append(qMakePair(imageName, name));
                element->type = "lv_image";
                if (!outputBase(shapeIndex, element))
                    return false;
                element->attributes.insert("src", imageName);
            }
        }
        break; }
    case QPsdAbstractLayerItem::PathInfo::Rectangle:
    case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
        element->type = "lv_obj";
        if (!outputBase(shapeIndex, element))
            return false;

        outputGradientOrSolid(element);

        if (path.radius > 0)
            element->attributes.insert("style_radius", QString::number(qRound(path.radius)));

        break; }
    case QPsdAbstractLayerItem::PathInfo::Path:
    default: {
        // For complex paths, export as image
        QPsdImageStore imageStore(dir, "images"_L1);
        QImage qimage = shape->image();
        QString name = imageStore.save(imageFileName(shape->name(), "PNG"_L1), qimage, "PNG");

        QFileInfo fi(name);
        QString imageName = fi.completeBaseName();
        exportedImages.append(qMakePair(imageName, name));

        element->type = "lv_image";
        if (!outputBase(shapeIndex, element))
            return false;
        element->attributes.insert("src", imageName);
        break; }
    }
    return true;
}

bool QPsdExporterLvglPlugin::outputImage(const QModelIndex &imageIndex, Element *element) const
{
    const auto *image = dynamic_cast<const QPsdImageLayerItem *>(model()->layerItem(imageIndex));
    QPsdImageStore imageStore(dir, "images"_L1);

    QString name;
    bool done = false;
    const auto linkedFile = image->linkedFile();
    if (!linkedFile.type.isEmpty()) {
        QImage qimage = image->linkedImage();
        if (!qimage.isNull()) {
            qimage = image->applyGradient(qimage);
            QByteArray format = linkedFile.type.trimmed();
            name = imageStore.save(imageFileName(linkedFile.name, QString::fromLatin1(format.constData()), linkedFile.uniqueId), qimage, format.constData());
            done = !name.isEmpty();
        }
    }
    if (!done) {
        QImage qimage = image->image();
        qimage = image->applyGradient(qimage);
        name = imageStore.save(imageFileName(image->name(), "PNG"_L1), qimage, "PNG");
    }

    QFileInfo fi(name);
    QString imageName = fi.completeBaseName();
    exportedImages.append(qMakePair(imageName, name));

    element->type = "lv_image";
    if (!outputBase(imageIndex, element))
        return false;
    element->attributes.insert("src", imageName);

    const auto *border = image->border();
    if (border && border->isEnable()) {
        QColor color = border->color();
        element->attributes.insert("style_border_width", QString::number(border->size()));
        element->attributes.insert("style_border_color", u"0x%1"_s.arg(color.rgb() & 0xFFFFFF, 6, 16, QChar('0')));
        element->attributes.insert("style_border_opa", QString::number(qRound(border->opacity() * 255)));
    }

    return true;
}

QString QPsdExporterLvglPlugin::generateGradientName(const QString &layerName) const
{
    QString base = toSnakeCase(layerName);
    if (base.isEmpty())
        base = "grad"_L1;
    return u"%1_%2"_s.arg(base).arg(gradientCounter++);
}

QPsdExporterLvglPlugin::GradientDef QPsdExporterLvglPlugin::makeGradientDef(const QString &name, const QGradient *gradient) const
{
    GradientDef def;
    def.name = name;

    // Convert stops
    for (const auto &stop : gradient->stops()) {
        GradientStop gs;
        gs.offset = qRound(stop.first * 255);
        gs.rgb = stop.second.rgb() & 0xFFFFFF;
        gs.opa = stop.second.alpha();
        def.stops.append(gs);
    }

    switch (gradient->type()) {
    case QGradient::LinearGradient: {
        def.type = GradientDef::Linear;
        const auto *lg = static_cast<const QLinearGradient *>(gradient);
        QPointF s = lg->start();
        QPointF e = lg->finalStop();
        // LVGL's software renderer requires start <= end for proper rendering.
        // If the gradient direction is reversed, swap start/end and invert stop offsets.
        bool needsReverse = false;
        if (qFuzzyCompare(s.x(), e.x())) {
            // Vertical gradient: check y direction
            needsReverse = (s.y() > e.y());
        } else {
            // Use the dominant axis
            needsReverse = (s.x() > e.x());
        }
        if (needsReverse) {
            std::swap(s, e);
            for (auto &stop : def.stops)
                stop.offset = 255 - stop.offset;
            std::reverse(def.stops.begin(), def.stops.end());
        }
        def.p1x = qRound(s.x());
        def.p1y = qRound(s.y());
        def.p2x = qRound(e.x());
        def.p2y = qRound(e.y());
        break; }
    case QGradient::RadialGradient: {
        def.type = GradientDef::Radial;
        const auto *rg = static_cast<const QRadialGradient *>(gradient);
        def.p1x = qRound(rg->center().x());
        def.p1y = qRound(rg->center().y());
        // edge point: center + radius in the x direction
        def.p2x = qRound(rg->center().x() + rg->radius());
        def.p2y = qRound(rg->center().y());
        break; }
    case QGradient::ConicalGradient: {
        def.type = GradientDef::Conical;
        const auto *cg = static_cast<const QConicalGradient *>(gradient);
        def.p1x = qRound(cg->center().x());
        def.p1y = qRound(cg->center().y());
        def.p2x = qRound(cg->angle());
        def.p2y = qRound(cg->angle()) + 360;
        // LVGL uses clockwise like PSD, but Qt uses counter-clockwise.
        // brushFromGdFl reversed the stops for Qt — undo for LVGL.
        for (auto &stop : def.stops)
            stop.offset = 255 - stop.offset;
        std::reverse(def.stops.begin(), def.stops.end());
        break; }
    default:
        def.type = GradientDef::Linear;
        break;
    }

    return def;
}

QString QPsdExporterLvglPlugin::escapeCString(const QString &text)
{
    QString ret;
    for (const QChar &ch : text) {
        switch (ch.unicode()) {
        case '"': ret += "\\\""_L1; break;
        case '\\': ret += "\\\\"_L1; break;
        case '\n': ret += "\\n"_L1; break;
        case '\r': ret += "\\r"_L1; break;
        case '\t': ret += "\\t"_L1; break;
        default:
            ret += ch;
            break;
        }
    }
    return ret;
}

QString QPsdExporterLvglPlugin::fontExpression(const QString &fontValue)
{
    // "montserrat <size>" → &lv_font_montserrat_<n> with n rounded to the
    // nearest size LVGL actually ships (even numbers 8..48)
    const auto parts = fontValue.split(QChar(' '));
    int size = 14;
    if (parts.size() == 2)
        size = parts.at(1).toInt();
    int rounded = qBound(8, (size + 1) / 2 * 2, 48);
    return u"&lv_font_montserrat_%1"_s.arg(rounded);
}

QString QPsdExporterLvglPlugin::imageSourcePath(const QString &imageName) const
{
    for (const auto &pair : exportedImages) {
        if (pair.first == imageName)
            return u"A:images/%1"_s.arg(pair.second);
    }
    return u"A:images/%1.png"_s.arg(imageName);
}

void QPsdExporterLvglPlugin::emitGradient(QTextStream &out, const GradientDef &grad) const
{
    const QString n = grad.name;
    out << "    static lv_grad_dsc_t grad_" << n << ";\n";
    out << "    {\n";
    out << "        static const lv_color_t stops_" << n << "_colors[] = {";
    for (const auto &stop : grad.stops)
        out << " LV_COLOR_MAKE(0x" << QString::number((stop.rgb >> 16) & 0xFF, 16)
            << ", 0x" << QString::number((stop.rgb >> 8) & 0xFF, 16)
            << ", 0x" << QString::number(stop.rgb & 0xFF, 16) << "),";
    out << " };\n";
    out << "        static const lv_opa_t stops_" << n << "_opa[] = {";
    for (const auto &stop : grad.stops)
        out << " " << stop.opa << ",";
    out << " };\n";
    out << "        static const uint8_t stops_" << n << "_fracs[] = {";
    for (const auto &stop : grad.stops)
        out << " " << stop.offset << ",";
    out << " };\n";
    switch (grad.type) {
    case GradientDef::Linear:
        out << "        lv_grad_linear_init(&grad_" << n << ", " << grad.p1x << ", " << grad.p1y
            << ", " << grad.p2x << ", " << grad.p2y << ", LV_GRAD_EXTEND_PAD);\n";
        break;
    case GradientDef::Radial:
        out << "        lv_grad_radial_init(&grad_" << n << ", " << grad.p1x << ", " << grad.p1y
            << ", " << grad.p2x << ", " << grad.p2y << ", LV_GRAD_EXTEND_PAD);\n";
        break;
    case GradientDef::Conical:
        out << "        lv_grad_conical_init(&grad_" << n << ", " << grad.p1x << ", " << grad.p1y
            << ", " << grad.p2x << ", " << grad.p2y << ", LV_GRAD_EXTEND_PAD);\n";
        break;
    }
    out << "        lv_grad_init_stops(&grad_" << n << ", stops_" << n << "_colors, stops_" << n
        << "_opa, stops_" << n << "_fracs, " << grad.stops.size() << ");\n";
    out << "    }\n";
}

void QPsdExporterLvglPlugin::emitElement(QTextStream &out, const Element &element, const QString &parentVar,
                                         int indent, int *counter, const QSet<QString> &exportIds) const
{
    const QString pad(indent * 4, QChar(' '));
    QString var = u"obj_%1"_s.arg((*counter)++);

    QString createFunc = "lv_obj_create"_L1;
    if (element.type == "lv_label"_L1)
        createFunc = "lv_label_create"_L1;
    else if (element.type == "lv_image"_L1)
        createFunc = "lv_image_create"_L1;
    else if (element.type == "lv_button"_L1)
        createFunc = "lv_button_create"_L1;

    out << pad << "{\n";
    const QString ipad = pad + "    "_L1;
    out << ipad << "lv_obj_t * " << var << " = " << createFunc << "(" << parentVar << ");\n";
    if (!element.id.isEmpty() && exportIds.contains(element.id))
        out << ipad << "s_" << element.id << " = " << var << ";\n";

    // Position and size first
    const auto &attrs = element.attributes;
    if (attrs.contains("x"_L1) && attrs.contains("y"_L1))
        out << ipad << "lv_obj_set_pos(" << var << ", " << attrs.value("x"_L1) << ", " << attrs.value("y"_L1) << ");\n";
    if (attrs.contains("width"_L1) && attrs.contains("height"_L1))
        out << ipad << "lv_obj_set_size(" << var << ", " << attrs.value("width"_L1) << ", " << attrs.value("height"_L1) << ");\n";

    for (auto it = attrs.constBegin(); it != attrs.constEnd(); ++it) {
        const QString &key = it.key();
        const QString &value = it.value();
        if (key == "x"_L1 || key == "y"_L1 || key == "width"_L1 || key == "height"_L1)
            continue;
        if (key == "hidden"_L1) {
            if (value == "true"_L1)
                out << ipad << "lv_obj_add_flag(" << var << ", LV_OBJ_FLAG_HIDDEN);\n";
        } else if (key == "blend_mode"_L1) {
            out << ipad << "lv_obj_set_style_blend_mode(" << var << ", " << value << ", 0);\n";
        } else if (key == "scrollbar_mode"_L1) {
            if (value == "off"_L1)
                out << ipad << "lv_obj_set_scrollbar_mode(" << var << ", LV_SCROLLBAR_MODE_OFF);\n";
        } else if (key == "style_pad_all"_L1) {
            out << ipad << "lv_obj_set_style_pad_all(" << var << ", " << value << ", 0);\n";
        } else if (key == "style_opa"_L1) {
            out << ipad << "lv_obj_set_style_opa(" << var << ", " << value << ", 0);\n";
        } else if (key == "style_bg_color"_L1) {
            out << ipad << "lv_obj_set_style_bg_color(" << var << ", lv_color_hex(" << value << "), 0);\n";
        } else if (key == "style_bg_opa"_L1) {
            out << ipad << "lv_obj_set_style_bg_opa(" << var << ", " << value << ", 0);\n";
        } else if (key == "style_bg_grad"_L1) {
            out << ipad << "lv_obj_set_style_bg_grad(" << var << ", &grad_" << value << ", 0);\n";
        } else if (key == "style_radius"_L1) {
            out << ipad << "lv_obj_set_style_radius(" << var << ", " << value << ", 0);\n";
        } else if (key == "style_border_width"_L1) {
            out << ipad << "lv_obj_set_style_border_width(" << var << ", " << value << ", 0);\n";
        } else if (key == "style_border_color"_L1) {
            out << ipad << "lv_obj_set_style_border_color(" << var << ", lv_color_hex(" << value << "), 0);\n";
        } else if (key == "style_border_opa"_L1) {
            out << ipad << "lv_obj_set_style_border_opa(" << var << ", " << value << ", 0);\n";
        } else if (key == "style_shadow_width"_L1) {
            out << ipad << "lv_obj_set_style_shadow_width(" << var << ", " << value << ", 0);\n";
        } else if (key == "text"_L1) {
            out << ipad << "lv_label_set_text(" << var << ", \"" << escapeCString(value) << "\");\n";
        } else if (key == "style_text_color"_L1) {
            out << ipad << "lv_obj_set_style_text_color(" << var << ", lv_color_hex(" << value << "), 0);\n";
        } else if (key == "style_text_font"_L1) {
            out << ipad << "lv_obj_set_style_text_font(" << var << ", " << fontExpression(value) << ", 0);\n";
        } else if (key == "style_text_align"_L1) {
            QString align;
            if (value == "left"_L1)
                align = "LV_TEXT_ALIGN_LEFT"_L1;
            else if (value == "right"_L1)
                align = "LV_TEXT_ALIGN_RIGHT"_L1;
            else if (value == "center"_L1)
                align = "LV_TEXT_ALIGN_CENTER"_L1;
            if (!align.isEmpty())
                out << ipad << "lv_obj_set_style_text_align(" << var << ", " << align << ", 0);\n";
        } else if (key == "src"_L1) {
            out << ipad << "lv_image_set_src(" << var << ", \"" << imageSourcePath(value) << "\");\n";
        } else {
            qWarning() << "lvgl exporter: unhandled attribute" << key << "=" << value;
        }
    }

    for (const Element &child : element.children)
        emitElement(out, child, var, indent + 1, counter, exportIds);

    out << pad << "}\n";
}

bool QPsdExporterLvglPlugin::saveHeader(const QString &baseName, const ExportData &exports) const
{
    QFile file(dir.absoluteFilePath(baseName + ".h"_L1));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    const QString guard = baseName.toUpper() + "_H"_L1;
    out << "/* Generated by Qt PSD Exporter - LVGL C exporter */\n";
    out << "#ifndef " << guard << "\n";
    out << "#define " << guard << "\n\n";
    out << "#include \"lvgl.h\"\n\n";
    out << "#ifdef __cplusplus\n";
    out << "extern \"C\" {\n";
    out << "#endif\n\n";
    out << "extern const int32_t " << baseName << "_width;\n";
    out << "extern const int32_t " << baseName << "_height;\n\n";
    out << "/* Create the screen contents as a child of `parent`.\n";
    out << " * Image assets are loaded from \"A:images/\"; register an LVGL\n";
    out << " * filesystem driver for the 'A' drive letter accordingly. */\n";
    out << "lv_obj_t * " << baseName << "_create(lv_obj_t * parent);\n";
    for (const auto &exp : exports) {
        out << "\nvoid " << baseName << "_set_" << exp.first << "_visible(bool visible);\n";
    }
    out << "\n#ifdef __cplusplus\n";
    out << "} /* extern \"C\" */\n";
    out << "#endif\n\n";
    out << "#endif /* " << guard << " */\n";
    return true;
}

bool QPsdExporterLvglPlugin::saveSource(const QString &baseName, const Element &root, const ExportData &exports, const QSize &targetSize) const
{
    QFile file(dir.absoluteFilePath(baseName + ".c"_L1));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QSet<QString> exportIds;
    for (const auto &exp : exports)
        exportIds.insert(exp.first);

    QTextStream out(&file);
    out << "/* Generated by Qt PSD Exporter - LVGL C exporter */\n";
    out << "#include \"" << baseName << ".h\"\n\n";
    out << "const int32_t " << baseName << "_width = " << targetSize.width() << ";\n";
    out << "const int32_t " << baseName << "_height = " << targetSize.height() << ";\n";

    for (const QString &id : std::as_const(exportIds))
        out << "\nstatic lv_obj_t * s_" << id << ";\n";

    out << "\nlv_obj_t * " << baseName << "_create(lv_obj_t * parent)\n";
    out << "{\n";

    for (const auto &grad : std::as_const(exportedGradients))
        emitGradient(out, grad);
    if (!exportedGradients.isEmpty())
        out << "\n";

    // Root object corresponds to the artboard/canvas
    out << "    lv_obj_t * root = lv_obj_create(parent);\n";
    Element rootCopy = root;
    const auto &attrs = rootCopy.attributes;
    out << "    lv_obj_set_pos(root, " << attrs.value("x"_L1, "0"_L1) << ", " << attrs.value("y"_L1, "0"_L1) << ");\n";
    out << "    lv_obj_set_size(root, " << attrs.value("width"_L1) << ", " << attrs.value("height"_L1) << ");\n";
    out << "    lv_obj_set_style_pad_all(root, 0, 0);\n";
    out << "    lv_obj_set_style_border_width(root, 0, 0);\n";
    out << "    lv_obj_set_style_radius(root, 0, 0);\n";
    out << "    lv_obj_set_style_bg_opa(root, 0, 0);\n";
    out << "    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);\n";

    int counter = 0;
    for (const Element &child : std::as_const(rootCopy.children))
        emitElement(out, child, "root"_L1, 1, &counter, exportIds);

    out << "    return root;\n";
    out << "}\n";

    for (const auto &exp : exports) {
        out << "\nvoid " << baseName << "_set_" << exp.first << "_visible(bool visible)\n";
        out << "{\n";
        out << "    if (s_" << exp.first << " == NULL) return;\n";
        out << "    if (visible) lv_obj_remove_flag(s_" << exp.first << ", LV_OBJ_FLAG_HIDDEN);\n";
        out << "    else lv_obj_add_flag(s_" << exp.first << ", LV_OBJ_FLAG_HIDDEN);\n";
        out << "}\n";
    }

    return true;
}

QT_END_NAMESPACE

#include "lvgl.moc"
