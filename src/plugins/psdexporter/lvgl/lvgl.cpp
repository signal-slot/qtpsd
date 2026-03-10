// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/qpsdexporterplugin.h>
#include <QtPsdExporter/qpsdimagestore.h>

#include <QtCore/QCborMap>
#include <QtCore/QDir>

#include <optional>
#include <QtCore/QXmlStreamWriter>
#include <QtGui/QBrush>
#include <QtGui/QConicalGradient>
#include <QtGui/QFontMetrics>
#include <QtGui/QLinearGradient>
#include <QtGui/QPen>
#include <QtGui/QRadialGradient>

#include <QtPsdGui/QPsdBorder>

QT_BEGIN_NAMESPACE

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
        QString color; // "0xRRGGBB"
        int opa; // 0-255
    };
    struct GradientDef {
        QString name;
        QString type; // "linear", "radial", "conical"
        QMap<QString, QString> attributes;
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

    bool saveComponentXml(const QString &baseName, Element *element, const ExportData &exports) const;
    bool saveGlobalsXml() const;
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
    view.attributes.insert("extends", "lv_obj");
    outputRect(QRect(QPoint(0, 0), targetSize), &view);
    view.attributes.insert("style_pad_all", "0");
    view.attributes.insert("scrollbar_mode", "off");

    for (int i = model->rowCount(QModelIndex {}) - 1; i >= 0; i--) {
        QModelIndex childIndex = model->index(i, 0, QModelIndex {});
        if (!traverseTree(childIndex, &view, &exports, std::nullopt))
            return false;
    }

    // Flattened PSD fallback: if no layers were produced, use the merged image
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

    if (!saveComponentXml("MainScreen", &view, exports))
        return false;

    if (!saveGlobalsXml())
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
        gs.color = u"0x%1"_s.arg(stop.second.rgb() & 0xFFFFFF, 6, 16, QChar('0'));
        gs.opa = stop.second.alpha();
        def.stops.append(gs);
    }

    switch (gradient->type()) {
    case QGradient::LinearGradient: {
        def.type = "linear"_L1;
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
        def.attributes.insert("start"_L1, u"%1 %2"_s.arg(qRound(s.x())).arg(qRound(s.y())));
        def.attributes.insert("end"_L1, u"%1 %2"_s.arg(qRound(e.x())).arg(qRound(e.y())));
        def.attributes.insert("extend"_L1, "pad"_L1);
        break; }
    case QGradient::RadialGradient: {
        def.type = "radial"_L1;
        const auto *rg = static_cast<const QRadialGradient *>(gradient);
        def.attributes.insert("center"_L1, u"%1 %2"_s.arg(qRound(rg->center().x())).arg(qRound(rg->center().y())));
        // edge point: center + radius in the x direction
        int edgeX = qRound(rg->center().x() + rg->radius());
        int edgeY = qRound(rg->center().y());
        def.attributes.insert("edge"_L1, u"%1 %2"_s.arg(edgeX).arg(edgeY));
        break; }
    case QGradient::ConicalGradient: {
        def.type = "conical"_L1;
        const auto *cg = static_cast<const QConicalGradient *>(gradient);
        def.attributes.insert("center"_L1, u"%1 %2"_s.arg(qRound(cg->center().x())).arg(qRound(cg->center().y())));
        // LVGL uses tenths of degrees (0-3600 range)
        int startAngle = qRound(cg->angle()) * 10;
        def.attributes.insert("angle"_L1, u"%1 %2"_s.arg(startAngle).arg(startAngle + 3600));
        // LVGL uses clockwise like PSD, but Qt uses counter-clockwise.
        // brushFromGdFl reversed the stops for Qt — undo for LVGL.
        for (auto &stop : def.stops)
            stop.offset = 255 - stop.offset;
        std::reverse(def.stops.begin(), def.stops.end());
        break; }
    default:
        def.type = "linear"_L1;
        break;
    }

    return def;
}

bool QPsdExporterLvglPlugin::saveComponentXml(const QString &baseName, Element *element, const ExportData &exports) const
{
    QFile file(dir.absoluteFilePath(baseName + ".xml"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);

    xml.writeStartDocument();
    xml.writeStartElement("component");

    // Write API section
    if (!exports.isEmpty()) {
        xml.writeStartElement("api");
        for (const auto &exp : exports) {
            xml.writeStartElement("prop");
            xml.writeAttribute("name", exp.first + "_visible");
            xml.writeAttribute("type", exp.second);
            xml.writeAttribute("default", "true");
            xml.writeEndElement();
        }
        xml.writeEndElement(); // api
    }

    // Write gradient definitions (must be in component scope for LVGL XML lookup)
    if (!exportedGradients.isEmpty()) {
        xml.writeStartElement("gradients");
        for (const auto &grad : exportedGradients) {
            xml.writeStartElement(grad.type);
            xml.writeAttribute("name", grad.name);
            QStringList gkeys = grad.attributes.keys();
            gkeys.sort();
            for (const QString &key : gkeys) {
                xml.writeAttribute(key, grad.attributes.value(key));
            }
            for (const auto &stop : grad.stops) {
                xml.writeStartElement("stop");
                xml.writeAttribute("color", stop.color);
                xml.writeAttribute("offset", QString::number(stop.offset));
                xml.writeAttribute("opa", QString::number(stop.opa));
                xml.writeEndElement();
            }
            xml.writeEndElement();
        }
        xml.writeEndElement(); // gradients
    }

    // Write view and children
    std::function<void(const Element &)> writeElement;
    writeElement = [&](const Element &el) {
        xml.writeStartElement(el.type);
        if (!el.id.isEmpty())
            xml.writeAttribute("id", el.id);
        // Write attributes in sorted order for consistency
        QStringList keys = el.attributes.keys();
        keys.sort();
        for (const QString &key : keys) {
            xml.writeAttribute(key, el.attributes.value(key));
        }
        for (const Element &child : el.children) {
            writeElement(child);
        }
        xml.writeEndElement();
    };

    writeElement(*element);

    xml.writeEndElement(); // component
    xml.writeEndDocument();

    return true;
}

bool QPsdExporterLvglPlugin::saveGlobalsXml() const
{
    QFile file(dir.absoluteFilePath("globals.xml"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);

    xml.writeStartDocument();
    xml.writeStartElement("globals");
    xml.writeStartElement("images");

    QSet<QString> writtenImages;
    for (const auto &img : exportedImages) {
        if (writtenImages.contains(img.first))
            continue;
        writtenImages.insert(img.first);

        xml.writeStartElement("file");
        xml.writeAttribute("name", img.first);
        xml.writeAttribute("src_path", u"A:images/%1"_s.arg(img.second));
        xml.writeEndElement();
    }

    xml.writeEndElement(); // images

    xml.writeEndElement(); // globals
    xml.writeEndDocument();

    return true;
}

QT_END_NAMESPACE

#include "lvgl.moc"
