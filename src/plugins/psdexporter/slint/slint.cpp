// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/qpsdexporterplugin.h>
#include <QtPsdExporter/qpsdimagestore.h>

#include <QtCore/QCborMap>
#include <QtCore/QDir>

#include <optional>
#include <QtGui/QBrush>
#include <QtGui/QFontMetrics>
#include <QtGui/QPen>

#include <QtPsdCore/QPsdSofiEffect>
#include <QtPsdGui/QPsdBorder>

QT_BEGIN_NAMESPACE

#define ARGF(x) arg(qRound(x))

// Slint uses CSS-style #rrggbbaa (alpha at end), not Qt's #aarrggbb
static QString colorToSlint(const QColor &color)
{
    if (color.alpha() == 255)
        return color.name();
    return QStringLiteral("#%1%2%3%4")
        .arg(color.red(), 2, 16, QChar('0'))
        .arg(color.green(), 2, 16, QChar('0'))
        .arg(color.blue(), 2, 16, QChar('0'))
        .arg(color.alpha(), 2, 16, QChar('0'));
}

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

    bool exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const ExportConfig &config) const override;

private:
    struct Element {
        QString type;
        QString id;
        QVariantHash properties;
        QList<Element> children;
    };

    using ImportData = QHash<QString, QSet<QString>>;
    struct Export {
        QString type;
        QString id;
        QString name;
        QVariant value;
    };
    using ExportData = QList<Export>;
    bool traverseTree(const QModelIndex &index, Element *, ImportData *, ExportData *, std::optional<QPsdExporterTreeItemModel::ExportHint::Type> = std::nullopt) const;
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

bool QPsdExporterSlintPlugin::exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const ExportConfig &config) const
{
    if (!initializeExport(model, to, config)) {
        return false;
    }

    ImportData imports;
    ExportData exports;

    Element window;
    window.type = "Window";
    outputRect(QRect { QPoint { 0, 0 }, canvasSize() }, &window);
    window.properties.insert("title", "\"\""_L1);

    for (int i = model->rowCount(QModelIndex {}) - 1; i >= 0; i--) {
        QModelIndex childIndex = model->index(i, 0, QModelIndex {});
        if (!traverseTree(childIndex, &window, &imports, &exports, std::nullopt))
            return false;
    }

    // Flattened PSD fallback: if no layers were produced, use the merged image
    if (window.children.isEmpty()) {
        const QImage merged = model->guiLayerTreeItemModel()->mergedImage();
        if (!merged.isNull()) {
            imageStore = QPsdImageStore(dir, "images"_L1);
            const QString name = imageStore.save("merged.png"_L1, merged, "PNG");
            Element img;
            img.type = "Image";
            outputRect(QRect { QPoint { 0, 0 }, canvasSize() }, &img);
            img.properties.insert("source", u"@image-url(\"images/%1\")"_s.arg(name));
            img.properties.insert("image-fit", "contain");
            window.children.append(img);
        }
    }

    return saveTo("MainWindow", &window, imports, exports);
}

bool QPsdExporterSlintPlugin::outputBase(const QModelIndex &index, Element *element, ImportData *imports, QRect rectBounds) const
{
    Q_UNUSED(imports);
    QRect rect = computeBaseRect(index, rectBounds);
    if (auto opac = displayOpacity(model()->layerItem(index)))
        element->properties.insert("opacity", *opac);
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

    const auto *folderItem = model()->layerItem(folderIndex);
    const auto mask = folderItem->vectorMask();
    if (mask.type != QPsdAbstractLayerItem::PathInfo::None) {
        const bool filled = (mask.rect.topLeft() == QPointF(0, 0) && mask.rect.size() == folderItem->rect().size());
        if (filled && mask.type == QPsdAbstractLayerItem::PathInfo::Rectangle && mask.radius <= 0) {
            element->properties.insert("clip", true);
        }
    }

    if (folder->artboardRect().isValid() && folder->artboardBackground() != Qt::transparent) {
        Element artboard;
        artboard.type = "Rectangle"_L1;
        QRect bgRect(QPoint(0, 0), folder->artboardRect().size());
        outputRect(bgRect, &artboard);
        artboard.properties.insert("background"_L1, folder->artboardBackground().name());
        element->children.append(artboard);
    }

    for (int i = model()->rowCount(folderIndex) - 1; i >= 0; i--) {
        QModelIndex childIndex = model()->index(i, 0, folderIndex);
        if (!traverseTree(childIndex, element, imports, exports, std::nullopt))
            return false;
    }
    return true;
};

bool QPsdExporterSlintPlugin::traverseTree(const QModelIndex &index, Element *parent, ImportData *imports, ExportData *exports, std::optional<QPsdExporterTreeItemModel::ExportHint::Type> hintOverload) const
{
    const QPsdAbstractLayerItem *item = model()->layerItem(index);
    const auto hint = model()->layerHint(index);
    const auto id = toKebabCase(hint.id);
    auto type = hint.type;
    if (hintOverload.has_value()) {
        type = *hintOverload;
    }

    switch (type) {
    case QPsdExporterTreeItemModel::ExportHint::Embed: {
        Element element;
        element.id = id;
        outputBase(index, &element, imports);
        bool generated = false;
        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            if (!id.isEmpty()) {
                generated = outputFolder(index, &element, imports, exports);
            } else {
                // Check if this folder needs clipping — if so, create a wrapper Rectangle
                const auto mask = item->vectorMask();
                const bool needsClip = mask.type == QPsdAbstractLayerItem::PathInfo::Rectangle
                    && mask.radius <= 0
                    && mask.rect.topLeft() == QPointF(0, 0)
                    && mask.rect.size() == item->rect().size();
                if (needsClip) {
                    generated = outputFolder(index, &element, imports, exports);
                } else {
                    // Check if folder needs wrapping for opacity/visibility
                    const bool needsWrapper = displayOpacity(item).has_value()
                        || !hint.visible || !item->isVisible();
                    if (needsWrapper) {
                        generated = outputFolder(index, &element, imports, exports);
                    } else {
                        // Flatten into parent: just add artboard background and children
                        const auto *f = dynamic_cast<const QPsdFolderLayerItem *>(item);
                        if (f && f->artboardRect().isValid() && f->artboardBackground() != Qt::transparent) {
                            Element artboard;
                            artboard.type = "Rectangle"_L1;
                            QRect bgRect = computeBaseRect(index);
                            outputRect(bgRect, &artboard);
                            artboard.properties.insert("background"_L1, f->artboardBackground().name());
                            parent->children.append(artboard);
                        }
                        for (int i = model()->rowCount(index) - 1; i >= 0; i--) {
                            QModelIndex childIndex = model()->index(i, 0, index);
                            if (!traverseTree(childIndex, parent, imports, exports, std::nullopt))
                                return false;
                        }
                        return true;
                    }
                }
            }
            break; }
        case QPsdAbstractLayerItem::Text: {
            generated = outputText(index, &element, imports);
            break; }
        case QPsdAbstractLayerItem::Shape: {
            generated = outputShape(index, &element, imports);
            break; }
        case QPsdAbstractLayerItem::Image: {
            generated = outputImage(index, &element, imports);
            break; }
        case QPsdAbstractLayerItem::Adjustment: {
            imageStore = QPsdImageStore(dir, "images"_L1);
            QString name = saveLayerImage(item);
            if (name.isEmpty())
                return true;
            element.type = "Image";
            if (!outputBase(index, &element, imports))
                return false;
            element.properties.insert("source", u"@image-url(\"images/%1\")"_s.arg(name));
            element.properties.insert("image-fit", "contain");
            generated = true;
            break; }
        default:
            return true;
        }
        if (!generated)
            return false;

        const bool hasRenderableContent = !element.type.isEmpty()
                                          || !element.children.isEmpty();
        if (!hasRenderableContent)
            return true;
        if (element.type.isEmpty()) {
            if (element.children.isEmpty())
                return true;
            element.type = "Rectangle";
        }

        if (!hint.visible || !item->isVisible())
            element.properties.insert("visible", "false");
        if (!id.isEmpty()) {
            if (hint.interactive) {
                Element touchArea { "TouchArea", element.id, {}, {} };
                outputBase(index, &touchArea, imports);
                if (!hint.visible || !item->isVisible())
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
                const auto &mergedLayers = indexMergeMap.values(index);
                for (const auto &mergedIndex : mergedLayers) {
                    const auto *i = model()->layerItem(mergedIndex);
                    switch (i->type()) {
                    case QPsdAbstractLayerItem::Text: {
                        const auto *textItem = dynamic_cast<const QPsdTextLayerItem *>(i);
                        const auto runs = textItem->runs();
                        QString text;
                        for (const auto &run : runs) {
                            text += run.text.trimmed();
                        }
                        const bool translatable = model()->layerHint(mergedIndex).properties.contains("translatable"_L1);
                        element.properties.insert("text",
                            translatable ? u"@tr(\"%1\")"_s.arg(text) : u"\"%1\""_s.arg(text));
                        break; }
                    case QPsdAbstractLayerItem::Image:
                    case QPsdAbstractLayerItem::Shape: {
                        Element imageElem;
                        const QString name = saveLayerImage(i);
                        imageElem.type = "Image"_L1;
                        imageElem.properties.insert("source", u"@image-url(\"images/%1\")"_s.arg(name));
                        imageElem.properties.insert("image-fit", "contain");
                        element.children.append(imageElem);
                        break; }
                    default:
                        qWarning() << i->type() << "is not supported";
                    }
                }
            }
        }

        if (!hint.visible || !item->isVisible())
            element.properties.insert("visible", "false");
        if (element.type.isEmpty())
            return true;
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
        bool generated = false;
        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            generated = outputFolder(index, &component, &i, &x);
            switch (hint.baseElement) {
            case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
                component.type = "";
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
            generated = outputText(index, &component, &i);
            if (!id.isEmpty() && hint.properties.contains("text"))
                exports->append({"string", id, "text", component.properties.value("text")});
            if (!id.isEmpty() && hint.properties.contains("color"))
                exports->append({"color", id, "color", component.properties.value("color")});
            break; }
        case QPsdAbstractLayerItem::Shape: {
            switch (hint.baseElement) {
            case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
                generated = outputShape(index, &component, &i);
                break;
            case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button:
            case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted:
                i["std-widgets.slint"].insert("Button");
                generated = outputShape(index, &component, &i, "Button");
                break;
            default:
                return true;
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
            generated = outputImage(index, &component, &i);
            if (!id.isEmpty()) {
                switch (hint.baseElement) {
                case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
                    if (hint.properties.contains("visible"))
                        exports->append({"bool", id, "visible", component.properties.value("visible", true)});
                    break;
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
        default:
            return true;
        }
        if (!generated)
            return false;
        if (!saveTo(hint.componentName, &component, i, x))
            return false;

        for (auto v = x.cbegin(), end = x.cend(); v != end; ++v) {
            const auto entry = *v;
            exports->append({entry.type, id, u"%1-%2"_s.arg(entry.id, entry.name), entry.value});
        }

        (*imports)[hint.componentName + ".slint"].insert(hint.componentName);
        Element element;
        element.type = hint.componentName;
        element.id = id;
        outputBase(index, &element, imports);
        if (!hint.visible || !item->isVisible())
            element.properties.insert("visible", "false");
        parent->children.append(element);
        break; }
    case QPsdExporterTreeItemModel::ExportHint::Merged:
    case QPsdExporterTreeItemModel::ExportHint::Skip:
        return true;
    }
    return true;
};

bool QPsdExporterSlintPlugin::outputText(const QModelIndex &textIndex, Element *element, ImportData *imports) const
{
    const auto *text = dynamic_cast<const QPsdTextLayerItem *>(model()->layerItem(textIndex));
    if (!text) {
        qWarning() << "Invalid text layer item for index" << textIndex;
        return false;
    }
    const bool translatable = model()->layerHint(textIndex).properties.contains("translatable"_L1);
    auto formatText = [translatable](const QString &literal) {
        return translatable ? u"@tr(\"%1\")"_s.arg(literal) : u"\"%1\""_s.arg(literal);
    };
    const auto shadow = parseDropShadow(text->dropShadow());
    const auto runs = text->runs();
    if (runs.isEmpty()) {
        element->type = "Text";
        if (!outputBase(textIndex, element, imports, text->bounds().toRect()))
            return false;
        element->properties.insert("text", formatText(QString()));
        element->properties.insert("color", "#000000");
        return true;
    }
    if (runs.size() == 1) {
        const auto run = runs.first();
        QRect rect = computeTextBounds(text);
        QString displayText = run.text.trimmed();
        if (run.fontCaps == 2)
            displayText = displayText.toUpper();
        displayText.replace("\n"_L1, "\\n"_L1);

        auto setTextProperties = [&](Element *e) {
            e->type = "Text";
            e->properties.insert("text", formatText(displayText));
            e->properties.insert("font-family", u"\"%1\""_s.arg(run.font.family()));
            e->properties.insert("font-size", u"%1px"_s.ARGF(run.font.pointSizeF() * fontScaleFactor));
            if (run.font.bold() || run.fauxBold)
                e->properties.insert("font-weight", 700);
            if (run.font.italic() || run.fauxItalic)
                e->properties.insert("font-italic", true);
            if (run.font.letterSpacingType() == QFont::AbsoluteSpacing) {
                const qreal ls = run.font.letterSpacing();
                if (qAbs(ls) > 0.01)
                    e->properties.insert("letter-spacing", u"%1px"_s.ARGF(ls * fontScaleFactor));
            } else if (run.font.letterSpacingType() == QFont::PercentageSpacing) {
                const qreal ls = run.font.letterSpacing();
                if (ls > 0.01 && qAbs(ls - 100.0) > 0.01)
                    e->properties.insert("letter-spacing", u"%1px"_s.ARGF((ls - 100.0) / 100.0 * run.font.pointSizeF() * fontScaleFactor));
            }
            e->properties.insert("horizontal-alignment",
                horizontalAlignmentString(run.alignment, {"left"_L1, "right"_L1, "center"_L1, "left"_L1}));
            {
                const auto vAlign = verticalAlignmentString(run.alignment, {"top"_L1, "bottom"_L1, "center"_L1});
                if (!vAlign.isEmpty())
                    e->properties.insert("vertical-alignment", vAlign);
            }
            if (text->textType() == QPsdTextLayerItem::TextType::ParagraphText) {
                e->properties.insert("overflow", "clip");
                e->properties.insert("wrap", "word-wrap");
            }
        };

        if (shadow) {
            element->type = "Rectangle";
            if (!outputBase(textIndex, element, imports, rect))
                return false;
            element->properties.insert("background", "transparent");

            Element shadowText;
            setTextProperties(&shadowText);
            shadowText.properties.insert("color", colorToSlint(shadow->color));
            const auto offset = dropShadowOffset(*shadow, true);
            shadowText.properties.insert("x", u"%1px"_s.ARGF(offset.x()));
            shadowText.properties.insert("y", u"%1px"_s.ARGF(offset.y()));
            shadowText.properties.insert("width", "100%");
            shadowText.properties.insert("height", "100%");
            element->children.append(shadowText);

            Element mainText;
            setTextProperties(&mainText);
            mainText.properties.insert("color", colorToSlint(run.color));
            mainText.properties.insert("width", "100%");
            mainText.properties.insert("height", "100%");
            element->children.append(mainText);
        } else {
            setTextProperties(element);
            if (!outputBase(textIndex, element, imports, rect))
                return false;
            element->properties.insert("color", colorToSlint(run.color));
        }
    } else {
        element->type = "Rectangle";
        if (text->textType() == QPsdTextLayerItem::TextType::ParagraphText)
            element->properties.insert("clip", true);
        QRect multiRect = computeTextBounds(text);
        if (!outputBase(textIndex, element, imports, multiRect))
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
                QString displayText = text;
                if (run.fontCaps == 2)
                    displayText = displayText.toUpper();
                textElement.properties.insert("text", formatText(displayText));
                textElement.properties.insert("font-family", u"\"%1\""_s.arg(run.font.family()));
                textElement.properties.insert("font-size", u"%1px"_s.ARGF(run.font.pointSizeF() * fontScaleFactor));
                if (run.font.bold() || run.fauxBold)
                    textElement.properties.insert("font-weight", 700);
                if (run.font.italic() || run.fauxItalic)
                    textElement.properties.insert("font-italic", true);
                if (run.font.letterSpacingType() == QFont::AbsoluteSpacing) {
                    const qreal ls = run.font.letterSpacing();
                    if (qAbs(ls) > 0.01)
                        textElement.properties.insert("letter-spacing", u"%1px"_s.ARGF(ls * fontScaleFactor));
                } else if (run.font.letterSpacingType() == QFont::PercentageSpacing) {
                    const qreal ls = run.font.letterSpacing();
                    if (ls > 0.01 && qAbs(ls - 100.0) > 0.01)
                        textElement.properties.insert("letter-spacing", u"%1px"_s.ARGF((ls - 100.0) / 100.0 * run.font.pointSizeF() * fontScaleFactor));
                }
                textElement.properties.insert("color", colorToSlint(run.color));
                textElement.properties.insert("horizontal-alignment",
                    horizontalAlignmentString(run.alignment, {"left"_L1, "right"_L1, "center"_L1, "left"_L1}));
                {
                    const auto vAlign = verticalAlignmentString(run.alignment, {"top"_L1, "bottom"_L1, "center"_L1});
                    if (!vAlign.isEmpty())
                        textElement.properties.insert("vertical-alignment", vAlign);
                }
                if (shadow) {
                    // slint doesn't support dropshadow for text
                    textElement.properties.insert("stroke-width", u"%1px"_s.ARGF(2 * unitScale));
                    textElement.properties.insert("stroke", shadow->color.name(QColor::HexArgb));
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
        // Ellipse detection: if radius >= half the smaller dimension and non-square,
        // Rectangle+border-radius produces a stadium shape. Use Path+CubicTo instead.
        {
            const qreal minDim = qMin(path.rect.width(), path.rect.height());
            if (path.radius * 2 >= minDim && path.rect.width() != path.rect.height()
                && path.type == QPsdAbstractLayerItem::PathInfo::RoundedRectangle) {
                element->type = "Path";
                if (!outputBase(shapeIndex, element, imports))
                    return false;
                const qreal cx = path.rect.center().x() * horizontalScale;
                const qreal cy = path.rect.center().y() * verticalScale;
                const qreal rx = path.rect.width() * horizontalScale / 2.0;
                const qreal ry = path.rect.height() * verticalScale / 2.0;
                if (shape->brush() != Qt::NoBrush)
                    element->properties.insert("fill", colorToSlint(shape->brush().color()));
                if (shape->pen().style() == Qt::NoPen) {
                    element->properties.insert("stroke", "transparent");
                } else {
                    element->properties.insert("stroke", colorToSlint(shape->pen().color()));
                    element->properties.insert("stroke-width",
                        u"%1px"_s.ARGF(computeStrokeWidth(shape->pen(), unitScale)));
                }
                // Approximate ellipse with 4 cubic bezier curves
                constexpr qreal k = 0.5522847498;
                auto fmt = [](qreal v) { return QString::number(v, 'f', 2); };
                auto addMoveTo = [&](qreal x, qreal y) {
                    Element cmd; cmd.type = "MoveTo";
                    cmd.properties.insert("x", fmt(x));
                    cmd.properties.insert("y", fmt(y));
                    element->children.append(cmd);
                };
                auto addCubicTo = [&](qreal c1x, qreal c1y, qreal c2x, qreal c2y, qreal x, qreal y) {
                    Element cmd; cmd.type = "CubicTo";
                    cmd.properties.insert("control-1-x", fmt(c1x));
                    cmd.properties.insert("control-1-y", fmt(c1y));
                    cmd.properties.insert("control-2-x", fmt(c2x));
                    cmd.properties.insert("control-2-y", fmt(c2y));
                    cmd.properties.insert("x", fmt(x));
                    cmd.properties.insert("y", fmt(y));
                    element->children.append(cmd);
                };
                addMoveTo(cx, cy - ry);
                addCubicTo(cx + rx * k, cy - ry, cx + rx, cy - ry * k, cx + rx, cy);
                addCubicTo(cx + rx, cy + ry * k, cx + rx * k, cy + ry, cx, cy + ry);
                addCubicTo(cx - rx * k, cy + ry, cx - rx, cy + ry * k, cx - rx, cy);
                addCubicTo(cx - rx, cy - ry * k, cx - rx * k, cy - ry, cx, cy - ry);
                return true;
            }
        }
        bool filled = isFilledRect(path, shape);
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
        const auto shapeShadow = parseDropShadow(shape->dropShadow());
        if (shapeShadow) {
            element2.properties.insert("drop-shadow-color", shapeShadow->color.name(QColor::HexArgb));
            const auto offset = dropShadowOffset(*shapeShadow, true);
            element2.properties.insert("drop-shadow-offset-x", u"%1px"_s.ARGF(offset.x()));
            element2.properties.insert("drop-shadow-offset-y", u"%1px"_s.ARGF(offset.y()));
            element2.properties.insert("drop-shadow-blur", u"%1px"_s.ARGF(shapeShadow->blur));
        }

        if (!shape->gradient()) {
            if (shape->pen().style() != Qt::NoPen) {
                qreal dw = computeStrokeWidth(shape->pen(), unitScale);
                outputRect(adjustRectForStroke(path.rect, shape->strokeAlignment(), dw), &element2);
                element2.properties.insert("border-width", u"%1px"_s.ARGF(dw));
                element2.properties.insert("border-color", shape->pen().color().name());
            }
        }
        const auto *g = effectiveGradient(shape);
        if (g) {
            switch (g->type()) {
            case QGradient::LinearGradient: {
                const auto linear = reinterpret_cast<const QLinearGradient *>(g);
                const auto angle = std::atan2(linear->finalStop().x() - linear->start().x(), linear->finalStop().y() - linear->start().y());
                QStringList grad = { "@linear-gradient(" + QString::number(180.0 - angle * 180.0 / M_PI, 'f', 3) + "deg" };
                for (const auto &stop : linear->stops()) {
                    grad.append(colorToSlint(stop.second) + " " + QString::number(stop.first * 100) + "%");
                }
                const QString gradString = grad.join(", ") + ")";
                element2.properties.insert("background", gradString);
                break; }
            case QGradient::RadialGradient: {
                const auto radial = reinterpret_cast<const QRadialGradient *>(g);
                const qreal radius = radial->radius();
                const QPointF center = radial->center();
                // Slint @radial-gradient(circle) uses CSS farthest-corner sizing
                // Scale stop positions: Qt radius vs CSS farthest-corner distance
                const qreal farthestCorner = std::sqrt(center.x() * center.x() + center.y() * center.y());
                const qreal scale = (farthestCorner > 0) ? (radius / farthestCorner) : 1.0;
                QStringList grad = { "@radial-gradient(circle" };
                const auto stops = radial->stops();
                for (const auto &stop : stops) {
                    grad.append(colorToSlint(stop.second) + " " + QString::number(stop.first * scale * 100) + "%");
                }
                if (!stops.isEmpty() && scale < 1.0) {
                    grad.append(colorToSlint(stops.last().second) + " 100%");
                }
                const QString gradString = grad.join(", ") + ")";
                element2.properties.insert("background", gradString);
                break; }
            case QGradient::ConicalGradient: {
                const auto conical = reinterpret_cast<const QConicalGradient *>(g);
                QStringList grad = { "@conic-gradient(from " +
                    QString::number(std::fmod(90.0 - conical->angle() + 360.0, 360.0), 'f', 3) + "deg" };
                const auto qtStops = conical->stops();
                for (int i = qtStops.size() - 1; i >= 0; --i) {
                    grad.append(colorToSlint(qtStops.at(i).second) + " " +
                        QString::number((1.0 - qtStops.at(i).first) * 360) + "deg");
                }
                const QString gradString = grad.join(", ") + ")";
                element2.properties.insert("background", gradString);
                break; }
            default:
                break;
            }
        } else {
            if (shape->brush() != Qt::NoBrush)
                element2.properties.insert("background", shape->brush().color().name());
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
        const auto *pathGrad = effectiveGradient(shape);
        if (pathGrad) {
            element->properties.insert("stroke", "transparent");
            switch (pathGrad->type()) {
            case QGradient::LinearGradient: {
                const auto linear = reinterpret_cast<const QLinearGradient *>(pathGrad);
                const auto angle = std::atan2(linear->finalStop().x() - linear->start().x(), linear->finalStop().y() - linear->start().y());
                QStringList grad { "@linear-gradient(" + QString::number(180.0 - angle * 180.0 / M_PI, 'f', 3) + "deg " };
                for (const auto &stop : linear->stops()) {
                    grad.append(colorToSlint(stop.second) + " " + QString::number(stop.first * 100) + "%");
                }
                const QString gradString = grad.join(", ") + ")";
                element->properties.insert("fill", gradString);
                break; }
            case QGradient::RadialGradient: {
                const auto radial = reinterpret_cast<const QRadialGradient *>(pathGrad);
                const qreal radius = radial->radius();
                const QPointF center = radial->center();
                const qreal farthestCorner = std::sqrt(center.x() * center.x() + center.y() * center.y());
                const qreal scale = (farthestCorner > 0) ? (radius / farthestCorner) : 1.0;
                QStringList grad = { "@radial-gradient(circle" };
                const auto stops = radial->stops();
                for (const auto &stop : stops) {
                    grad.append(colorToSlint(stop.second) + " " + QString::number(stop.first * scale * 100) + "%");
                }
                if (!stops.isEmpty() && scale < 1.0) {
                    grad.append(colorToSlint(stops.last().second) + " 100%");
                }
                const QString gradString = grad.join(", ") + ")";
                element->properties.insert("fill", gradString);
                break; }
            case QGradient::ConicalGradient: {
                const auto conical = reinterpret_cast<const QConicalGradient *>(pathGrad);
                QStringList grad = { "@conic-gradient(from " +
                    QString::number(std::fmod(90.0 - conical->angle() + 360.0, 360.0), 'f', 3) + "deg" };
                const auto qtStops = conical->stops();
                for (int i = qtStops.size() - 1; i >= 0; --i) {
                    grad.append(colorToSlint(qtStops.at(i).second) + " " +
                        QString::number((1.0 - qtStops.at(i).first) * 360) + "deg");
                }
                const QString gradString = grad.join(", ") + ")";
                element->properties.insert("fill", gradString);
                break; }
            default:
                break;
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
    case QPsdAbstractLayerItem::PathInfo::None: {
        element->type = base;
        if (!outputBase(shapeIndex, element, imports))
            return false;
        Element element2;
        element2.type = "Rectangle";
        if (!outputBase(shapeIndex, &element2, imports))
            return false;
        const auto *g = effectiveGradient(shape);
        if (g) {
            switch (g->type()) {
            case QGradient::LinearGradient: {
                const auto linear = reinterpret_cast<const QLinearGradient *>(g);
                const auto angle = std::atan2(linear->finalStop().x() - linear->start().x(), linear->finalStop().y() - linear->start().y());
                QStringList grad = { "@linear-gradient(" + QString::number(180.0 - angle * 180.0 / M_PI, 'f', 3) + "deg" };
                for (const auto &stop : linear->stops()) {
                    grad.append(colorToSlint(stop.second) + " " + QString::number(stop.first * 100) + "%");
                }
                const QString gradString = grad.join(", ") + ")";
                element2.properties.insert("background", gradString);
                break; }
            case QGradient::RadialGradient: {
                const auto radial = reinterpret_cast<const QRadialGradient *>(g);
                const qreal radius = radial->radius();
                const QPointF center = radial->center();
                const qreal farthestCorner = std::sqrt(center.x() * center.x() + center.y() * center.y());
                const qreal scale = (farthestCorner > 0) ? (radius / farthestCorner) : 1.0;
                QStringList grad = { "@radial-gradient(circle" };
                const auto stops = radial->stops();
                for (const auto &stop : stops) {
                    grad.append(colorToSlint(stop.second) + " " + QString::number(stop.first * scale * 100) + "%");
                }
                if (!stops.isEmpty() && scale < 1.0) {
                    grad.append(colorToSlint(stops.last().second) + " 100%");
                }
                const QString gradString = grad.join(", ") + ")";
                element2.properties.insert("background", gradString);
                break; }
            case QGradient::ConicalGradient: {
                const auto conical = reinterpret_cast<const QConicalGradient *>(g);
                QStringList grad = { "@conic-gradient(from " +
                    QString::number(std::fmod(90.0 - conical->angle() + 360.0, 360.0), 'f', 3) + "deg" };
                const auto qtStops = conical->stops();
                for (int i = qtStops.size() - 1; i >= 0; --i) {
                    grad.append(colorToSlint(qtStops.at(i).second) + " " +
                        QString::number((1.0 - qtStops.at(i).first) * 360) + "deg");
                }
                const QString gradString = grad.join(", ") + ")";
                element2.properties.insert("background", gradString);
                break; }
            default:
                break;
            }
        } else {
            if (shape->brush() != Qt::NoBrush) {
                element2.properties.insert("background", shape->brush().color().name());
            } else {
                // No gradient and no brush (e.g. pattern fill) — fall back to image
                imageStore = QPsdImageStore(dir, "images"_L1);
                const QImage img = shape->image();
                if (!img.isNull()) {
                    const QString name = saveLayerImage(shape);
                    if (!name.isEmpty()) {
                        element2.type = "Image";
                        element2.properties.insert("source", u"@image-url(\"images/%1\")"_s.arg(name));
                        element2.properties.insert("image-fit", "contain");
                    }
                }
            }
        }
        element->children.append(element2);
        break; }
    default:
        break;
    }
    return true;
};

bool QPsdExporterSlintPlugin::outputImage(const QModelIndex &imageIndex, Element *element, ImportData *imports) const
{
    const auto *image = dynamic_cast<const QPsdImageLayerItem *>(model()->layerItem(imageIndex));
    imageStore = QPsdImageStore(dir, "images"_L1);
    QString name = saveLayerImage(image);
    if (name.isEmpty())
        return true;

    // Apply color overlay (SOFI) effect by baking it into the exported PNG
    for (const auto &effect : image->effects()) {
        if (effect.canConvert<QPsdSofiEffect>()) {
            const auto sofi = effect.value<QPsdSofiEffect>();
            if (sofi.blendMode() == QPsdBlend::Mode::Normal) {
                const QString path = dir.absoluteFilePath("images/"_L1 + name);
                QImage img(path);
                if (!img.isNull()) {
                    img = img.convertToFormat(QImage::Format_ARGB32);
                    const QColor overlayColor(sofi.nativeColor());
                    const qreal opacity = sofi.opacity();
                    const int or_ = overlayColor.red();
                    const int og = overlayColor.green();
                    const int ob = overlayColor.blue();
                    for (int y = 0; y < img.height(); ++y) {
                        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
                        for (int x = 0; x < img.width(); ++x) {
                            const int a = qAlpha(line[x]);
                            if (a == 0) continue;
                            const int r = qRound(qRed(line[x]) * (1.0 - opacity) + or_ * opacity);
                            const int g = qRound(qGreen(line[x]) * (1.0 - opacity) + og * opacity);
                            const int b = qRound(qBlue(line[x]) * (1.0 - opacity) + ob * opacity);
                            line[x] = qRgba(r, g, b, a);
                        }
                    }
                    img.save(path);
                }
            } else {
                qWarning() << sofi.blendMode() << "not supported blend mode for Slint color overlay";
            }
        }
    }

    element->type = "Image";
    if (!outputBase(imageIndex, element, imports))
        return false;
    element->properties.insert("source", u"@image-url(\"images/%1\")"_s.arg(name));
    element->properties.insert("image-fit", "contain");

    const auto *border = image->border();
    if (border && border->isEnable()) {
        const qreal bw = border->size() * unitScale;
        Element wrapper;
        wrapper.type = "Rectangle";
        if (!outputBase(imageIndex, &wrapper, imports))
            return false;
        QRect baseRect = computeBaseRect(imageIndex);
        switch (border->position()) {
        case QPsdBorder::Outer:
            // Expand wrapper outward; image stays at border offset inside
            wrapper.properties.insert("x", u"%1px"_s.ARGF((baseRect.x() - border->size()) * horizontalScale));
            wrapper.properties.insert("y", u"%1px"_s.ARGF((baseRect.y() - border->size()) * verticalScale));
            wrapper.properties.insert("width", u"%1px"_s.ARGF((baseRect.width() + 2 * border->size()) * horizontalScale));
            wrapper.properties.insert("height", u"%1px"_s.ARGF((baseRect.height() + 2 * border->size()) * verticalScale));
            element->properties.insert("x", u"%1px"_s.ARGF(bw));
            element->properties.insert("y", u"%1px"_s.ARGF(bw));
            break;
        case QPsdBorder::Inner:
            // Keep wrapper at original size; image shrinks inside the border
            element->properties.insert("x", u"%1px"_s.ARGF(bw));
            element->properties.insert("y", u"%1px"_s.ARGF(bw));
            element->properties.insert("width", u"%1px"_s.ARGF((baseRect.width() - 2 * border->size()) * horizontalScale));
            element->properties.insert("height", u"%1px"_s.ARGF((baseRect.height() - 2 * border->size()) * verticalScale));
            break;
        case QPsdBorder::Center:
            // Expand wrapper by half stroke; image offset by half stroke
            wrapper.properties.insert("x", u"%1px"_s.ARGF((baseRect.x() - border->size() / 2.0) * horizontalScale));
            wrapper.properties.insert("y", u"%1px"_s.ARGF((baseRect.y() - border->size() / 2.0) * verticalScale));
            wrapper.properties.insert("width", u"%1px"_s.ARGF((baseRect.width() + border->size()) * horizontalScale));
            wrapper.properties.insert("height", u"%1px"_s.ARGF((baseRect.height() + border->size()) * verticalScale));
            element->properties.insert("x", u"%1px"_s.ARGF(bw / 2.0));
            element->properties.insert("y", u"%1px"_s.ARGF(bw / 2.0));
            break;
        }
        wrapper.properties.insert("border-width", u"%1px"_s.ARGF(bw));
        wrapper.properties.insert("border-color", border->color().name());
        wrapper.properties.insert("background", "transparent");
        wrapper.children.append(*element);
        *element = wrapper;
    }

    return true;
};

bool QPsdExporterSlintPlugin::converTo(Element *element, ImportData *imports, const QPsdExporterTreeItemModel::ExportHint &hint) const
{
    switch (hint.baseElement) {
    case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
        element->type = "Rectangle";
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

    const auto commands = pathToCommands(path, horizontalScale, verticalScale);
    for (const auto &cmd : commands) {
        switch (cmd.type) {
        case PathCommand::MoveTo: {
            Element moveTo;
            moveTo.type = "MoveTo";
            moveTo.properties.insert("x", u"%1"_s.ARGF(cmd.x));
            moveTo.properties.insert("y", u"%1"_s.ARGF(cmd.y));
            element->children.append(moveTo);
            break; }
        case PathCommand::LineTo: {
            Element lineTo;
            lineTo.type = "LineTo";
            lineTo.properties.insert("x", u"%1"_s.ARGF(cmd.x));
            lineTo.properties.insert("y", u"%1"_s.ARGF(cmd.y));
            element->children.append(lineTo);
            break; }
        case PathCommand::CubicTo: {
            Element cubicTo;
            cubicTo.type = "CubicTo";
            cubicTo.properties.insert("control-1-x", u"%1"_s.ARGF(cmd.c1x));
            cubicTo.properties.insert("control-1-y", u"%1"_s.ARGF(cmd.c1y));
            cubicTo.properties.insert("control-2-x", u"%1"_s.ARGF(cmd.c2x));
            cubicTo.properties.insert("control-2-y", u"%1"_s.ARGF(cmd.c2y));
            cubicTo.properties.insert("x", u"%1"_s.ARGF(cmd.x));
            cubicTo.properties.insert("y", u"%1"_s.ARGF(cmd.y));
            element->children.append(cubicTo);
            break; }
        case PathCommand::Close:
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

    writeLicenseHeader(out);

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
