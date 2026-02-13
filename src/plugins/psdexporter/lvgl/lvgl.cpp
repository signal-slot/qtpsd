// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/qpsdexporterplugin.h>
#include <QtPsdExporter/qpsdimagestore.h>

#include <QtCore/QCborMap>
#include <QtCore/QDir>
#include <QtCore/QXmlStreamWriter>
#include <QtGui/QBrush>
#include <QtGui/QFontMetrics>
#include <QtGui/QPen>

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

    bool exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const override;

private:
    struct Element {
        QString type;
        QString id;
        QMap<QString, QString> attributes;
        QList<Element> children;
    };

    mutable QDir dir;
    mutable QList<QPair<QString, QString>> exportedImages; // name, filename

    using ExportData = QList<QPair<QString, QString>>; // id, type for API props

    bool traverseTree(const QModelIndex &index, Element *parent, ExportData *exports, QPsdExporterTreeItemModel::ExportHint::Type hintOverload) const;

    bool outputRect(const QRect &rect, Element *element) const;
    bool outputBase(const QModelIndex &index, Element *element, QRect rectBounds = {}) const;
    bool outputFolder(const QModelIndex &folderIndex, Element *element, ExportData *exports) const;
    bool outputText(const QModelIndex &textIndex, Element *element) const;
    bool outputShape(const QModelIndex &shapeIndex, Element *element) const;
    bool outputImage(const QModelIndex &imageIndex, Element *element) const;

    bool saveComponentXml(const QString &baseName, Element *element, const ExportData &exports) const;
    bool saveGlobalsXml() const;
};

bool QPsdExporterLvglPlugin::exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const
{
    setModel(model);
    dir = QDir(to);
    exportedImages.clear();

    const QSize originalSize = model->size();
    const QSize targetSize = hint.value("resolution", originalSize).toSize();

    generateMaps();

    ExportData exports;

    Element view;
    view.type = "view";
    view.attributes.insert("extends", "lv_obj");
    outputRect(QRect(QPoint(0, 0), targetSize), &view);
    view.attributes.insert("style_pad_all", "0");
    view.attributes.insert("scrollbar_mode", "off");

    for (int i = model->rowCount(QModelIndex {}) - 1; i >= 0; i--) {
        QModelIndex childIndex = model->index(i, 0, QModelIndex {});
        if (!traverseTree(childIndex, &view, &exports, QPsdExporterTreeItemModel::ExportHint::None))
            return false;
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
        outputRect(folder->artboardRect(), &artboard);
        QColor color = folder->artboardBackground();
        artboard.attributes.insert("style_bg_color", u"0x%1"_s.arg(color.rgb() & 0xFFFFFF, 6, 16, QChar('0')));
        artboard.attributes.insert("style_bg_opa", "255");
        element->children.append(artboard);
    }

    for (int i = model()->rowCount(folderIndex) - 1; i >= 0; i--) {
        QModelIndex childIndex = model()->index(i, 0, folderIndex);
        if (!traverseTree(childIndex, element, exports, QPsdExporterTreeItemModel::ExportHint::None))
            return false;
    }
    return true;
}

bool QPsdExporterLvglPlugin::traverseTree(const QModelIndex &index, Element *parent, ExportData *exports, QPsdExporterTreeItemModel::ExportHint::Type hintOverload) const
{
    const QPsdAbstractLayerItem *item = model()->layerItem(index);
    const auto hint = model()->layerHint(index);
    const auto id = toSnakeCase(hint.id);
    auto type = hint.type;
    if (hintOverload != QPsdExporterTreeItemModel::ExportHint::None) {
        type = hintOverload;
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

        const bool hasRenderableElement = !element.type.isEmpty();
        Element *mergeParent = hasRenderableElement ? &element : parent;
        if (indexMergeMap.contains(index)) {
            const auto &list = indexMergeMap.values(index);
            for (auto it = list.constBegin(); it != list.constEnd(); it++) {
                traverseTree(*it, mergeParent, exports, QPsdExporterTreeItemModel::ExportHint::Embed);
            }
        }

        if (!hasRenderableElement)
            return true;

        if (!hint.visible)
            element.attributes.insert("hidden", "true");

        if (!id.isEmpty()) {
            if (hint.baseElement == QPsdExporterTreeItemModel::ExportHint::TouchArea) {
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
    case QPsdExporterTreeItemModel::ExportHint::Merge:
    case QPsdExporterTreeItemModel::ExportHint::Skip:
    case QPsdExporterTreeItemModel::ExportHint::None:
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
    QRect rect;
    if (text->textType() == QPsdTextLayerItem::TextType::ParagraphText) {
        rect = text->bounds().toRect();
    } else {
        const auto &firstRun = runs.first();
        QFont metricsFont = firstRun.font;
        metricsFont.setPixelSize(qRound(firstRun.font.pointSizeF()));
        QFontMetrics fm(metricsFont);
        QRectF adjustedBounds = text->bounds();
        adjustedBounds.setY(text->textOrigin().y() - fm.ascent());
        adjustedBounds.setHeight(fm.height());
        rect = adjustedBounds.toRect();
    }
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

    // Horizontal alignment
    const Qt::Alignment horizontalAlignment = static_cast<Qt::Alignment>(firstRun.alignment & Qt::AlignHorizontal_Mask);
    switch (horizontalAlignment) {
    case Qt::AlignLeft:
        element->attributes.insert("style_text_align", "left");
        break;
    case Qt::AlignRight:
        element->attributes.insert("style_text_align", "right");
        break;
    case Qt::AlignHCenter:
        element->attributes.insert("style_text_align", "center");
        break;
    default:
        element->attributes.insert("style_text_align", "left");
        break;
    }

    return true;
}

bool QPsdExporterLvglPlugin::outputShape(const QModelIndex &shapeIndex, Element *element) const
{
    const auto *shape = dynamic_cast<const QPsdShapeLayerItem *>(model()->layerItem(shapeIndex));
    const auto path = shape->pathInfo();

    switch (path.type) {
    case QPsdAbstractLayerItem::PathInfo::Rectangle:
    case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
        element->type = "lv_obj";
        if (!outputBase(shapeIndex, element))
            return false;

        if (shape->brush() != Qt::NoBrush) {
            QColor color = shape->brush().color();
            element->attributes.insert("style_bg_color", u"0x%1"_s.arg(color.rgb() & 0xFFFFFF, 6, 16, QChar('0')));
            element->attributes.insert("style_bg_opa", QString::number(color.alpha()));
        }

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
