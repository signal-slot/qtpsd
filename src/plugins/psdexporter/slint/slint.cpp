// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/qpsdexporterplugin.h>
#include <QtPsdExporter/qpsdimagestore.h>

#include <QtCore/QCborMap>
#include <QtCore/QDir>
#include <QtGui/QBrush>
#include <QtGui/QPen>

QT_BEGIN_NAMESPACE

#define ARGF(x) arg(qRound(x))

class QPsdExporterSlintPlugin : public QPsdExporterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdExporterFactoryInterface" FILE "slint.json")
public:
    int priority() const override { return 10; }
    QIcon icon() const override {
        return QIcon(":/slint/slint.png");
    }
    QString name() const override {
        return tr("&Slint");
    }
    ExportType exportType() const override { return QPsdExporterPlugin::Directory; }

    bool exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const override;

private:
    struct Element {
        QString type;
        QString id;
        QVariantHash properties;
        QList<Element> children;
    };

    mutable qreal horizontalScale = 0;
    mutable qreal verticalScale = 0;
    mutable qreal unitScale = 0;
    mutable qreal fontScaleFactor = 0;
    mutable bool makeCompact = false;
    mutable bool imageScaling = false;
    mutable QDir dir;
    mutable QString licenseText;

    using ImportData = QHash<QString, QSet<QString>>;
    struct Export {
        QString type;
        QString id;
        QString name;
        QVariant value;
    };
    using ExportData = QList<Export>;
    bool traverseTree(const QModelIndex &index, Element *, ImportData *, ExportData *, QPsdExporterTreeItemModel::ExportHint::Type) const;
    bool converTo(Element *element, ImportData *imports, const QPsdExporterTreeItemModel::ExportHint &hint) const;
    bool outputPath(const QPainterPath &path, Element *element) const;

    bool outputRect(const QRectF &rect, Element *element, bool skipEmpty = false) const;
    bool outputBase(const QModelIndex &index, Element *element, ImportData *imports, QRect rectBounds = {}) const;
    bool outputFolder(const QModelIndex &folderIndex, Element *element, ImportData *imports, ExportData *exports) const;
    bool outputText(const QModelIndex &textIndex, Element *element, ImportData *imports) const;
    bool outputShape(const QModelIndex &shapeIndex, Element *element, ImportData *imports, const QString &base = u"Rectangle"_s) const;
    bool outputImage(const QModelIndex &imageIndex, Element *element, ImportData *imports) const;

    bool saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const;
};

bool QPsdExporterSlintPlugin::exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const
{
    setModel(model);
    dir = QDir(to);

    const QSize originalSize = model->size();
    const QSize targetSize = hint.value("resolution", originalSize).toSize();
    horizontalScale = targetSize.width() / qreal(originalSize.width());
    verticalScale = targetSize.height() / qreal(originalSize.height());
    unitScale = std::min(horizontalScale, verticalScale);
    fontScaleFactor = hint.value("fontScaleFactor", 1.0).toReal() * verticalScale;
    makeCompact = hint.value("makeCompact", false).toBool();
    imageScaling = hint.value("imageScaling", false).toBool();
    licenseText = hint.value("licenseText").toString();

    generateMaps();

    ImportData imports;
    ExportData exports;

    Element window;
    window.type = "Window";
    outputRect(QRect { QPoint { 0, 0 }, model->size() }, &window);
    window.properties.insert("title", "\"\""_L1);

    for (int i = model->rowCount(QModelIndex {}) - 1; i >= 0; i--) {
        QModelIndex childIndex = model->index(i, 0, QModelIndex {});
        if (!traverseTree(childIndex, &window, &imports, &exports, QPsdExporterTreeItemModel::ExportHint::None))
            return false;
    }

    return saveTo("MainWindow", &window, imports, exports);
}

bool QPsdExporterSlintPlugin::outputBase(const QModelIndex &index, Element *element, ImportData *imports, QRect rectBounds) const
{
    Q_UNUSED(imports);
    const QPsdAbstractLayerItem *item = model()->layerItem(index);
    QRect rect;
    if (rectBounds.isEmpty()) {
        rect = item->rect();
        if (makeCompact) {
            rect = indexRectMap.value(index);
        }
    } else {
        rect = rectBounds;
    }
    if (model()->layerHint(index).type == QPsdExporterTreeItemModel::ExportHint::Merge) {
        auto parentIndex = indexMergeMap.key(index);
        while (parentIndex.isValid()) {
            const auto *parent = model()->layerItem(parentIndex);
            rect.translate(-parent->rect().topLeft());
            parentIndex = model()->parent(parentIndex);
        }
    }
    // Handle opacity and fillOpacity
    // In Photoshop: opacity affects everything (including effects), fillOpacity affects only content
    const qreal opacity = item->opacity();
    const qreal fillOpacity = item->fillOpacity();
    const bool hasEffects = !item->dropShadow().isEmpty() || !item->effects().isEmpty();

    if (hasEffects) {
        // When there are effects, apply opacity to the whole item
        // fillOpacity will be applied to the content in outputImage/outputShape
        if (opacity < 1.0) {
            element->properties.insert("opacity", opacity);
        }
    } else {
        // When no effects, combine opacity and fillOpacity
        const qreal combinedOpacity = opacity * fillOpacity;
        if (combinedOpacity < 1.0) {
            element->properties.insert("opacity", combinedOpacity);
        }
    }
    outputRect(rect, element, true);
    return true;
};

bool QPsdExporterSlintPlugin::outputRect(const QRectF &rect, Element *element, bool skipEmpty) const
{
    element->properties.insert("x", u"%1px"_s.ARGF(rect.x() * horizontalScale));
    element->properties.insert("y", u"%1px"_s.ARGF(rect.y() * verticalScale));
    if (rect.isEmpty() && skipEmpty)
        return true;
    element->properties.insert("width", u"%1px"_s.ARGF(rect.width() * horizontalScale));
    element->properties.insert("height", u"%1px"_s.ARGF(rect.height() * verticalScale));
    return true;
}

bool QPsdExporterSlintPlugin::outputFolder(const QModelIndex &folderIndex, Element *element, ImportData *imports, ExportData *exports) const
{
    const auto *folder = dynamic_cast<const QPsdFolderLayerItem *>(model()->layerItem(folderIndex));
    if (element->type.isEmpty())
        element->type = "Rectangle"_L1;
    if (!outputBase(folderIndex, element, imports))
        return false;

    if (folder->artboardRect().isValid() && folder->artboardBackground() != Qt::transparent) {
        Element artboard;
        artboard.type = "Rectangle"_L1;
        outputRect(folder->artboardRect(), &artboard);
        artboard.properties.insert("color"_L1, folder->artboardBackground().name());
        element->children.append(artboard);
    }

    for (int i = model()->rowCount(folderIndex) - 1; i >= 0; i--) {
        QModelIndex childIndex = model()->index(i, 0, folderIndex);
        if (!traverseTree(childIndex, element, imports, exports, QPsdExporterTreeItemModel::ExportHint::None))
            return false;
    }
    return true;
};

bool QPsdExporterSlintPlugin::traverseTree(const QModelIndex &index, Element *parent, ImportData *imports, ExportData *exports, QPsdExporterTreeItemModel::ExportHint::Type hintOverload) const
{
    const QPsdAbstractLayerItem *item = model()->layerItem(index);
    const auto hint = model()->layerHint(index);
    const auto id = toKebabCase(hint.id);
    auto type = hint.type;
    if (hintOverload != QPsdExporterTreeItemModel::ExportHint::None) {
        type = hintOverload;
    }

    switch (type) {
    case QPsdExporterTreeItemModel::ExportHint::Embed: {
        Element element;
        element.id = id;
        outputBase(index, &element, imports);
        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            if (!id.isEmpty()) {
                outputFolder(index, &element, imports, exports);
            } else {
                outputFolder(index, parent, imports, exports);
                return true;
            }
            break; }
        case QPsdAbstractLayerItem::Text: {
            outputText(index, &element, imports);
            break; }
        case QPsdAbstractLayerItem::Shape: {
            outputShape(index, &element, imports);
            break; }
        case QPsdAbstractLayerItem::Image: {
            outputImage(index, &element, imports);
            break; }
        default:
            break;
        }

        if (indexMergeMap.contains(index)) {
            const auto &list = indexMergeMap.values(index);
            for (auto it = list.constBegin(); it != list.constEnd(); it++) {
                traverseTree(*it, &element, imports, exports, QPsdExporterTreeItemModel::ExportHint::Embed);
            }
        }

        if (!hint.visible)
            element.properties.insert("visible", "false");
        if (!id.isEmpty()) {
            if (hint.baseElement == QPsdExporterTreeItemModel::ExportHint::TouchArea) {
                Element touchArea { "TouchArea", element.id, {}, {} };
                outputBase(index, &touchArea, imports);
                if (!hint.visible)
                    touchArea.properties.insert("visible", "false");
                element.id = QString();
                if (item->type() == QPsdAbstractLayerItem::Folder) {
                    element.properties.remove("x");
                    element.properties.remove("y");
                } else {
                    element.properties.insert("x", u"%1.pressed ? 2px : 0px"_s.arg(id));
                    element.properties.insert("y", "self.x");
                }
                touchArea.properties.insert("visible", element.properties.contains("visible") ? element.properties.take("visible") : true);
                touchArea.children.append(element);
                parent->children.append(touchArea);
                exports->append({"callback", id, "clicked", {}});
                if (hint.properties.contains("visible"))
                    exports->append({"bool", id, "visible", touchArea.properties.value("visible", true)});
                if (hint.properties.contains("position")) {
                    exports->append({"length", id, "x", touchArea.properties.value("x")});
                    exports->append({"length", id, "y", touchArea.properties.value("y")});
                }
                if (hint.properties.contains("size")) {
                    if (touchArea.properties.contains("width"))
                        exports->append({"length", id, "width", touchArea.properties.value("width")});
                    if (touchArea.properties.contains("height"))
                        exports->append({"length", id, "height", touchArea.properties.value("height")});
                }
            } else if (element.type == "Text") {
                if (hint.properties.contains("text"))
                    exports->append({"string", id, "text", element.properties.value("text")});
                if (hint.properties.contains("color"))
                    exports->append({"color", id, "color", element.properties.value("color")});
                if (!element.properties.contains("visible"))
                    element.properties.insert("visible", true);
                if (hint.properties.contains("visible"))
                    exports->append({"bool", id, "visible", element.properties.value("visible")});
                parent->children.append(element);
            } else {
                if (!element.properties.contains("visible"))
                    element.properties.insert("visible", true);
                if (hint.properties.contains("visible"))
                    exports->append({"bool", id, "visible", element.properties.value("visible")});
                if (hint.properties.contains("position")) {
                    exports->append({"length", id, "x", element.properties.value("x")});
                    exports->append({"length", id, "y", element.properties.value("y")});
                }
                if (hint.properties.contains("size")) {
                    if (element.properties.contains("width"))
                        exports->append({"length", id, "width", element.properties.value("width")});
                    if (element.properties.contains("height"))
                        exports->append({"length", id, "height", element.properties.value("height")});
                }
                parent->children.append(element);
            }
        } else {
            parent->children.append(element);
        }
        break; }
    case QPsdExporterTreeItemModel::ExportHint::Native: {
        Element element;
        element.id = id;
        outputBase(index, &element, imports);
        converTo(&element, imports, hint);
        if (element.type == "Button"_L1) {
            if (indexMergeMap.contains(index)) {
                for (auto it = indexMergeMap.constBegin(); it != indexMergeMap.constEnd(); it++) {
                    const auto *i = model()->layerItem(it.value());
                    switch (i->type()) {
                    case QPsdAbstractLayerItem::Text: {
                        const auto *textItem = dynamic_cast<const QPsdTextLayerItem *>(i);
                        const auto runs = textItem->runs();
                        QString text;
                        for (const auto &run : runs) {
                            text += run.text.trimmed();
                        }
                        element.properties.insert("text", u"\"%1\""_s.arg(text));
                        break; }
                    default:
                        qWarning() << i->type() << "is not supported";
                    }
                }
            }
        }

        if (!hint.visible)
            element.properties.insert("visible", "false");
        if (!id.isEmpty()) {
            if (element.type == "Button" || element.type == "TouchArea") {
                exports->append({"callback", element.id, "clicked", {}});
            } else if (element.type == "Text") {
                if (hint.properties.contains("text"))
                    exports->append({"string", id, "text", element.properties.value("text")});
                if (hint.properties.contains("color"))
                    exports->append({"color", id, "color", element.properties.value("color")});
            } else {
                qWarning() << element.type << "is not supported for" << element.id;
            }
        }

        parent->children.append(element);
        break; }
    case QPsdExporterTreeItemModel::ExportHint::Component: {
        ImportData i;
        ExportData x;
        Element component;
        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            outputFolder(index, &component, &i, &x);
            switch (hint.baseElement) {
            case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
                component.type = "";
                break;
            case QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea:
                component.type = "TouchArea";
                if (!id.isEmpty())
                    exports->append({"callback", id, "clicked", {}});
                break;
            case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button:
            case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted:
                (*imports)["std-widgets.slint"].insert("Button");
                component.type = "Button";
                component.properties.insert("primary", (hint.baseElement == QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted));
                if (!id.isEmpty())
                    exports->append({"callback", id, "clicked", {}});
                break;
            default:
                break;
            }
            if (!id.isEmpty()) {
                switch (hint.baseElement) {
                case QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea:
                case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button:
                case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted:
                    exports->append({"callback", id, "clicked", {}});
                    break;
                default:
                    break;
                }
                if (hint.properties.contains("visible"))
                    exports->append({"bool", id, "visible", component.properties.value("visible", true)});
            }
            break; }
        case QPsdAbstractLayerItem::Text: {
            outputText(index, &component, &i);
            if (!id.isEmpty() && hint.properties.contains("text"))
                exports->append({"string", id, "text", component.properties.value("text")});
            if (!id.isEmpty() && hint.properties.contains("color"))
                exports->append({"color", id, "color", component.properties.value("color")});
            break; }
        case QPsdAbstractLayerItem::Shape: {
            switch (hint.baseElement) {
            case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
                outputShape(index, &component, &i);
                break;
            case QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea:
                outputShape(index, &component, &i, "TouchArea");
                break;
            case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button:
            case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted:
                i["std-widgets.slint"].insert("Button");
                outputShape(index, &component, &i, "Button");
                break;
            default:
                break;
            }

            if (!id.isEmpty()) {
                if (component.type == "Button" || component.type == "TouchArea") {
                    exports->append({"callback", id, "clicked", {}});
                } else if (component.type == "Text") {
                    if (hint.properties.contains("text"))
                        exports->append({"string", id, "text", component.properties.value("text")});
                    if (hint.properties.contains("color"))
                        exports->append({"color", id, "color", component.properties.value("color")});
                } else {
                    qWarning() << component.type << "is not supported for" << id;
                }
            }

            break; }
        case QPsdAbstractLayerItem::Image: {
            outputImage(index, &component, &i);
            if (!id.isEmpty()) {
                switch (hint.baseElement) {
                case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
                    if (hint.properties.contains("visible"))
                        exports->append({"bool", id, "visible", component.properties.value("visible", true)});
                    break;
                case QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea:
                case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button:
                case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted:
                    exports->append({"callback", id, "clicked", {}});
                    break;
                default:
                    exports->append({"image", id, "source", component.properties.value("source")});
                    break;
                }
            }
            break; }
        }
        saveTo(hint.componentName, &component, i, x);

        for (auto v = x.cbegin(), end = x.cend(); v != end; ++v) {
            const auto entry = *v;
            exports->append({entry.type, id, u"%1-%2"_s.arg(entry.id, entry.name), entry.value});
        }

        (*imports)[hint.componentName + ".slint"].insert(hint.componentName);
        Element element;
        element.type = hint.componentName;
        element.id = id;
        outputBase(index, &element, imports);
        if (!hint.visible)
            element.properties.insert("visible", "false");
        parent->children.append(element);
        break; }
    case QPsdExporterTreeItemModel::ExportHint::Merge:
    case QPsdExporterTreeItemModel::ExportHint::Skip:
        return true;
    }
    return true;
};

bool QPsdExporterSlintPlugin::outputText(const QModelIndex &textIndex, Element *element, ImportData *imports) const
{
    const auto *text = dynamic_cast<const QPsdTextLayerItem *>(model()->layerItem(textIndex));
    const auto dropShadow = text->dropShadow();
    const auto runs = text->runs();
    if (runs.size() == 1) {
        const auto run = runs.first();
        element->type = "Text";
        QRect rect;
        if (text->textType() == QPsdTextLayerItem::TextType::ParagraphText) {
            rect = text->bounds().toRect();
        } else {
            rect = text->fontAdjustedBounds().toRect();
        }
        if (!outputBase(textIndex, element, imports, rect))
            return false;
        element->properties.insert("text", u"\"%1\""_s.arg(run.text.trimmed().replace("\n", "\\n")));
        element->properties.insert("font-family", u"\"%1\""_s.arg(run.font.family()));
        element->properties.insert("font-size", u"%1px"_s.ARGF(run.font.pointSizeF() * fontScaleFactor));
        element->properties.insert("color", run.color.name());
        // Use proper horizontal alignment from PSD
        const Qt::Alignment horizontalAlignment = static_cast<Qt::Alignment>(run.alignment & Qt::AlignHorizontal_Mask);
        switch (horizontalAlignment) {
        case Qt::AlignLeft:
            element->properties.insert("horizontal-alignment", "left");
            break;
        case Qt::AlignRight:
            element->properties.insert("horizontal-alignment", "right");
            break;
        case Qt::AlignHCenter:
            element->properties.insert("horizontal-alignment", "center");
            break;
        case Qt::AlignJustify:
            element->properties.insert("horizontal-alignment", "left"); // Slint doesn't support justify
            break;
        default:
            element->properties.insert("horizontal-alignment", "left");
            break;
        }
        switch (run.alignment) {
        case Qt::AlignTop:
            element->properties.insert("vertical-alignment", "top");
            break;
        case Qt::AlignBottom:
            element->properties.insert("vertical-alignment", "bottom");
            break;
        case Qt::AlignVCenter:
            element->properties.insert("vertical-alignment", "center");
            break;
        }
        if (!dropShadow.isEmpty()) {
            // slint doesn't support dropshadow for text
            element->properties.insert("stroke-width", u"%1px"_s.ARGF(2 * unitScale));
            QColor color(dropShadow.value("color").toString());
            color.setAlphaF(dropShadow.value("opacity").toDouble());
            element->properties.insert("stroke", color.name(QColor::HexArgb));
        }
    } else {
        element->type = "Rectangle";
        if (!outputBase(textIndex, element, imports, text->fontAdjustedBounds().toRect()))
            return false;

        Element verticalLayout;
        verticalLayout.type = "VerticalLayout";
        verticalLayout.properties.insert("padding", 0);

        Element horizontalLayout;
        horizontalLayout.type = "HorizontalLayout";
        horizontalLayout.properties.insert("padding", 0);
        horizontalLayout.properties.insert("spacing", 0);

        for (const auto &run : runs) {
            const auto texts = run.text.trimmed().split("\n");
            bool first = true;
            for (const auto &text : texts) {
                if (first) {
                    first = false;
                } else {
                    verticalLayout.children.append(horizontalLayout);
                    horizontalLayout.children.clear();
                }
                Element textElement;
                textElement.type = "Text";
                textElement.properties.insert("text", u"\"%1\""_s.arg(text));
                textElement.properties.insert("font-family", u"\"%1\""_s.arg(run.font.family()));
                textElement.properties.insert("font-size", u"%1px"_s.ARGF(run.font.pointSizeF() * fontScaleFactor));
                textElement.properties.insert("color", run.color.name());
                // Use proper horizontal alignment from PSD
                const Qt::Alignment horizontalAlignment = static_cast<Qt::Alignment>(run.alignment & Qt::AlignHorizontal_Mask);
                switch (horizontalAlignment) {
                case Qt::AlignLeft:
                    textElement.properties.insert("horizontal-alignment", "left");
                    break;
                case Qt::AlignRight:
                    textElement.properties.insert("horizontal-alignment", "right");
                    break;
                case Qt::AlignHCenter:
                    textElement.properties.insert("horizontal-alignment", "center");
                    break;
                case Qt::AlignJustify:
                    textElement.properties.insert("horizontal-alignment", "left"); // Slint doesn't support justify
                    break;
                default:
                    textElement.properties.insert("horizontal-alignment", "left");
                    break;
                }
                switch (run.alignment) {
                case Qt::AlignTop:
                    textElement.properties.insert("vertical-alignment", "top");
                    break;
                case Qt::AlignBottom:
                    textElement.properties.insert("vertical-alignment", "bottom");
                    break;
                case Qt::AlignVCenter:
                    textElement.properties.insert("vertical-alignment", "center");
                    break;
                }
                if (!dropShadow.isEmpty()) {
                    // slint doesn't support dropshadow for text
                    textElement.properties.insert("stroke-width", u"%1px"_s.ARGF(2 * unitScale));
                    QColor color(dropShadow.value("color").toString());
                    color.setAlphaF(dropShadow.value("opacity").toDouble());
                    textElement.properties.insert("stroke", color.name(QColor::HexArgb));
                }
                horizontalLayout.children.append(textElement);
            }
        }
        verticalLayout.children.append(horizontalLayout);
        element->children.append(verticalLayout);
    }
    return true;
};

bool QPsdExporterSlintPlugin::outputShape(const QModelIndex &shapeIndex, Element *element, ImportData *imports, const QString &base) const
{
    const auto *shape = dynamic_cast<const QPsdShapeLayerItem *>(model()->layerItem(shapeIndex));
    const auto path = shape->pathInfo();
    switch (path.type) {
    case QPsdAbstractLayerItem::PathInfo::Rectangle:
    case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
        bool filled = (path.rect.topLeft() == QPointF(0, 0) && path.rect.size() == shape->rect().size());
        if (!filled || base != "Rectangle") {
            element->type = base;
            if (!outputBase(shapeIndex, element, imports))
                return false;
        }

        Element element2;
        element2.type = "Rectangle";
        if (filled) {
            if (!outputBase(shapeIndex, &element2, imports))
                return false;
        } else {
            outputRect(path.rect, &element2);
        }
        const auto dropShadow = shape->dropShadow();
        if (!dropShadow.isEmpty()) {
            QColor color(dropShadow.value("color").toString());
            color.setAlphaF(dropShadow.value("opacity").toDouble());
            element2.properties.insert("drop-shadow-color", color.name(QColor::HexArgb));
            const auto angle = (180 - dropShadow.value("angle").toDouble()) * M_PI / 180.0;
            const auto distance = dropShadow.value("distance").toDouble();
            element2.properties.insert("drop-shadow-offset-x", u"%1px"_s.ARGF(std::cos(angle) * distance));
            element2.properties.insert("drop-shadow-offset-y", u"%1px"_s.ARGF(std::sin(angle) * distance));
            element2.properties.insert("drop-shadow-blur", u"%1px"_s.ARGF(dropShadow.value("size").toDouble()));
        }

        if (shape->gradient()) {
            const auto g = shape->gradient();
            switch (g->type()) {
            case QGradient::LinearGradient: {
                const auto linear = reinterpret_cast<const QLinearGradient *>(g);
                const auto angle = std::atan2(linear->finalStop().x() - linear->start().x(), linear->finalStop().y() - linear->start().y());
                QStringList grad = { "@linear-gradient(" + QString::number(angle * 180.0 / M_PI - 180.0 ) + "deg" };
                for (const auto &stop : linear->stops()) {
                    grad.append(stop.second.name() + " " + QString::number(stop.first * 100) + "%");
                }
                const QString gradString = grad.join(", ") + ")";
                element2.properties.insert("background"_L1, gradString);
                break; }
            case QGradient::RadialGradient: {
                const auto radial = reinterpret_cast<const QRadialGradient *>(g);
                QStringList grad = { "@radial-gradient(circle" };
                for (const auto &stop : radial->stops()) {
                    grad.append(stop.second.name() + " " + QString::number(stop.first * 100) + "%");
                }
                const QString gradString = grad.join(", ") + ")";
                element2.properties.insert("background"_L1, gradString);
                break; }
            default:
                qFatal() << "Unsupported gradient type"_L1 << g->type();
            }
        } else {
            if (shape->pen().style() != Qt::NoPen) {
                qreal dw = std::max(1.0, shape->pen().width() * unitScale);
                outputRect(path.rect.adjusted(-dw, -dw, dw, dw), &element2);
                element2.properties.insert("border-width", u"%1px"_s.ARGF(dw));
                element2.properties.insert("border-color", shape->pen().color().name());
            }
            // TODO: merge code with above
            const auto g = shape->brush().gradient();
            if (g) {
                switch (g->type()) {
                case QGradient::LinearGradient: {
                    const auto linear = reinterpret_cast<const QLinearGradient *>(g);
                    const auto angle = std::atan2(linear->finalStop().x() - linear->start().x(), linear->finalStop().y() - linear->start().y());
                    QStringList grad = { "@linear-gradient(" + QString::number(angle * 180.0 / M_PI - 180.0 ) + "deg" };
                    for (const auto &stop : linear->stops()) {
                        grad.append(stop.second.name() + " " + QString::number(stop.first * 100) + "%");
                    }
                    const QString gradString = grad.join(", ") + ")";
                    element2.properties.insert("background", gradString);
                    break; }
                case QGradient::RadialGradient: {
                    const auto radial = reinterpret_cast<const QRadialGradient *>(g);
                    QStringList grad = { "@radial-gradient(circle" };
                    for (const auto &stop : radial->stops()) {
                        grad.append(stop.second.name() + " " + QString::number(stop.first * 100) + "%");
                    }
                    const QString gradString = grad.join(", ") + ")";
                    element2.properties.insert("background", gradString);
                    break; }
                default:
                    qFatal() << "Unsupported gradient type" << g->type();
                }
            } else {
                if (shape->brush() != Qt::NoBrush)
                    element2.properties.insert("background", shape->brush().color().name());
            }
        }
        if (path.radius > 0)
            element2.properties.insert("border-radius", u"%1px"_s.ARGF(path.radius * horizontalScale));
        if (!filled || base != "Rectangle") {
            element->children.append(element2);
        } else {
            *element = element2;
        }
        break; }
    case QPsdAbstractLayerItem::PathInfo::Path: {
        element->type = "Path";
        if (!outputBase(shapeIndex, element, imports))
            return false;
        if (shape->gradient()) {
            element->properties.insert("stroke", "transparent");
            const auto g = shape->gradient();
            switch (g->type()) {
            case QGradient::LinearGradient: {
                const auto linear = reinterpret_cast<const QLinearGradient *>(g);
                const auto angle = std::atan2(linear->finalStop().x() - linear->start().x(), linear->finalStop().y() - linear->start().y());
                QStringList grad { "@linear-gradient(" + QString::number(180.0 - (angle) * 180.0 / M_PI) + "deg " };
                for (const auto &stop : linear->stops()) {
                    grad.append(stop.second.name() + " " + QString::number(stop.first * 100) + "%");
                }
                const QString gradString = grad.join(", ") + ")";
                element->properties.insert("fill", gradString);
                break; }
            case QGradient::RadialGradient: {
                const auto radial = reinterpret_cast<const QRadialGradient *>(g);
                QStringList grad = { "@radial-gradient(circle" };
                for (const auto &stop : radial->stops()) {
                    grad.append(stop.second.name() + " " + QString::number(stop.first * 100) + "%");
                }
                const QString gradString = grad.join(", ") + ")";
                element->properties.insert("fill", gradString);
                break; }
            default:
                qFatal() << "Unsupported gradient type" << g->type();
            }
        } else {
            element->properties.insert("stroke-width", u"%1px"_s.ARGF(shape->pen().width() * unitScale));
            if (shape->pen().style() == Qt::NoPen)
                element->properties.insert("stroke", "transparent");
            else
                element->properties.insert("stroke", shape->pen().color().name());
            element->properties.insert("fill", shape->brush().color().name());
        }
        if (!outputPath(path.path, element))
            return false;
        break; }
    default:
        break;
    }
    return true;
};

bool QPsdExporterSlintPlugin::outputImage(const QModelIndex &imageIndex, Element *element, ImportData *imports) const
{
    const auto *image = dynamic_cast<const QPsdImageLayerItem *>(model()->layerItem(imageIndex));
    QPsdImageStore imageStore(dir, "images"_L1);

    // Check if we need to apply fillOpacity to image content
    const qreal fillOpacity = image->fillOpacity();
    const bool hasEffects = !image->dropShadow().isEmpty() || !image->effects().isEmpty();
    const bool needsFillOpacity = hasEffects && fillOpacity < 1.0;

    auto applyFillOpacity = [fillOpacity](QImage &img) {
        if (fillOpacity >= 1.0)
            return;
        img = img.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < img.height(); ++y) {
            QRgb *scanLine = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                const int alpha = qAlpha(scanLine[x]);
                if (alpha > 0) {
                    const int newAlpha = qRound(alpha * fillOpacity);
                    scanLine[x] = qRgba(qRed(scanLine[x]), qGreen(scanLine[x]), qBlue(scanLine[x]), newAlpha);
                }
            }
        }
    };

    QString name;
    bool done = false;
    const auto linkedFile = image->linkedFile();
    if (!linkedFile.type.isEmpty()) {
        QImage qimage = image->linkedImage();
        if (!qimage.isNull()) {
            if (imageScaling) {
                qimage = qimage.scaled(image->rect().width() * horizontalScale, image->rect().height() * verticalScale, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            if (needsFillOpacity) {
                applyFillOpacity(qimage);
            }
            QByteArray format = linkedFile.type.trimmed();
            name = imageStore.save(imageFileName(linkedFile.name, QString::fromLatin1(format.constData())), qimage, format.constData());
            done = !name.isEmpty();
        }
    }
    if (!done) {
        QImage qimage = image->image();
        if (imageScaling) {
            qimage = qimage.scaled(image->rect().width() * horizontalScale, image->rect().height() * verticalScale, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        if (needsFillOpacity) {
            applyFillOpacity(qimage);
        }
        name = imageStore.save(imageFileName(image->name(), "PNG"_L1), qimage, "PNG");
    }

    element->type = "Image";
    if (!outputBase(imageIndex, element, imports))
        return false;
    element->properties.insert("source", u"@image-url(\"images/%1\")"_s.arg(name));
    element->properties.insert("image-fit", "contain");
    return true;
};

bool QPsdExporterSlintPlugin::converTo(Element *element, ImportData *imports, const QPsdExporterTreeItemModel::ExportHint &hint) const
{
    switch (hint.baseElement) {
    case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
        element->type = "Rectangle";
        break;
    case QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea:
        element->type = "TouchArea";
        break;
    case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted:
        element->properties.insert("primary", (hint.baseElement == QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted));
        Q_FALLTHROUGH();
    case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button:
        (*imports)["std-widgets.slint"].insert("Button");
        element->type = "Button";
        break;
    }
    return true;
};

bool QPsdExporterSlintPlugin::outputPath(const QPainterPath &path, Element *element) const
{
    switch (path.fillRule()) {
    case Qt::OddEvenFill:
        element->properties.insert("fill-rule", "evenodd");
        break;
    case Qt::WindingFill:
        element->properties.insert("fill-rule", "nonzero");
        break;
    }

    int control = 1;
    Element cubicTo;
    cubicTo.type = "CubicTo";
    for (int i = 0; i < path.elementCount(); i++) {
        const auto point = path.elementAt(i);
        const auto x = point.x * horizontalScale;
        const auto y = point.y * verticalScale;
        switch (point.type) {
        case QPainterPath::MoveToElement: {
            Element moveTo;
            moveTo.type = "MoveTo";
            moveTo.properties.insert("x", u"%1"_s.ARGF(x));
            moveTo.properties.insert("y", u"%1"_s.ARGF(y));
            element->children.append(moveTo);
            break; }
        case QPainterPath::LineToElement: {
            Element lineTo;
            lineTo.type = "LineTo";
            lineTo.properties.insert("x", u"%1"_s.ARGF(x));
            lineTo.properties.insert("y", u"%1"_s.ARGF(y));
            element->children.append(lineTo);
            break; }
        case QPainterPath::CurveToElement: {
            cubicTo.properties.insert("control-1-x", u"%1"_s.ARGF(x));
            cubicTo.properties.insert("control-1-y", u"%1"_s.ARGF(y));
            control = 1;
            break; }
        case QPainterPath::CurveToDataElement:
            switch (control) {
            case 1:
                cubicTo.properties.insert("control-2-x", u"%1"_s.ARGF(x));
                cubicTo.properties.insert("control-2-y", u"%1"_s.ARGF(y));
                control--;
                break;
            case 0:
                cubicTo.properties.insert("x", u"%1"_s.ARGF(x));
                cubicTo.properties.insert("y", u"%1"_s.ARGF(y));
                element->children.append(cubicTo);
                break;
            }
            break;
        }
    }
    return true;
};

bool QPsdExporterSlintPlugin::saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const
{
    QFile file(dir.absoluteFilePath(baseName + ".slint"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&file);

    if (!licenseText.isEmpty()) {
        const QStringList lines = licenseText.split('\n');
        for (const QString &line : lines) {
            out << "// " << line << "\n";
        }
        out << "\n";
    }

    for (auto i = imports.cbegin(), end = imports.cend(); i != end; ++i) {
        out << "import {" << i.value().values().join(", ") << "} from \"" << i.key() << "\";\n";
    }

    if (element->type.isEmpty())
        element->type = u"export component %1"_s.arg(baseName);
    else
        element->type = u"export component %1 inherits %2"_s.arg(baseName, element->type);

    std::function<bool(const Element *, int)> traverseElement;
    traverseElement = [&](const Element *element, int level) -> bool {
        if (element->id.isEmpty())
            out << QByteArray(level * 4, ' ') << element->type << " {\n";
        else
            out << QByteArray(level * 4, ' ') << element->id << " := " << element->type << " {\n";
        level++;

        // API
        if (level == 1) {
            QList<Export> list = exports;
            std::sort(list.begin(), list.end(), [&](const Export &a, const Export &b) {
                if (a.type == "callback" && b.type != "callback")
                    return false;
                if (a.type != "callback" && b.type == "callback")
                    return true;
                if (a.id == b.id)
                    return a.name < b.name;
                return a.id < b.id;
            });
            for (const auto &entry: list) {
                if (entry.type == "callback") {
                    out << QByteArray(level * 4, ' ') << u"%1 %2-%3 <=> %2.%3;\n"_s.arg(entry.type, entry.id, entry.name);
                } else {
                    auto value = entry.value;
                    QString text;
                    switch (value.typeId()) {
                    case QMetaType::QString:
                        text = value.toString();
                        break;
                    case QMetaType::Bool:
                        text = value.toBool() ? "true" : "false";
                        break;
                    default:
                        qWarning() << entry.type << "not supported" << value;
                    }
                    out << QByteArray(level * 4, ' ') << u"in-out property<%1> %2-%3: %4;\n"_s.arg(entry.type, entry.id, entry.name, text);
                }
            }
        }

        // property
        auto keys = element->properties.keys();
        std::sort(keys.begin(), keys.end(), std::less<QString>());
        for (const auto &key : keys) {
            QString valueAsText;
            // check if the property is exported, if so point it
            for (const auto &entry : exports) {
                if (entry.type != "callback" && entry.id == element->id && entry.name == key) {
                    valueAsText = u"root.%1-%2"_s.arg(entry.id, entry.name);
                    break;
                }
            }

            if (valueAsText.isEmpty()) {
                const auto value = element->properties.value(key);
                switch (value.typeId()) {
                case QMetaType::QString:
                    valueAsText = value.toString();
                    break;
                case QMetaType::Int:
                    valueAsText = QString::number(value.toInt());
                    break;
                case QMetaType::Double:
                    valueAsText = QString::number(value.toDouble());
                    break;
                case QMetaType::Bool:
                    valueAsText = value.toBool() ? "true" : "false";
                    break;
                default:
                    qFatal() << value.typeName() << "is not supported";
                }
            }
            out << QByteArray(level * 4, ' ') << key << ": " << valueAsText << ";\n";
        }

        // children
        for (const auto &child : element->children) {
            traverseElement(&child, level);
        }
        level--;
        out << QByteArray(level * 4, ' ') << "}\n";
        return true;
    };

    return traverseElement(element, 0);
};

QT_END_NAMESPACE

#include "slint.moc"
