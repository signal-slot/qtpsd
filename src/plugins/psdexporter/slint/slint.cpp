// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdGui/qpsdexporterplugin.h>
#include <QtPsdGui/qpsdimagestore.h>

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

    bool exportTo(const QPsdFolderLayerItem *tree, const QString &to, const QVariantMap &hint) const override;

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
    mutable QHash<const QPsdAbstractLayerItem *, QRect> rectMap;
    mutable QMultiMap<const QPsdAbstractLayerItem *, const QPsdAbstractLayerItem *> mergeMap;
    mutable QDir dir;

    using ImportData = QHash<QString, QSet<QString>>;
    struct Export {
        QString type;
        QString id;
        QString name;
        QVariant value;
    };
    using ExportData = QList<Export>;
    void generateRectMap(const QPsdAbstractLayerItem *item, const QPoint &topLeft) const;
    bool generateMergeData(const QPsdAbstractLayerItem *item) const;
    bool traverseTree(const QPsdAbstractLayerItem *, Element *, ImportData *, ExportData *, QPsdAbstractLayerItem::ExportHint::Type) const;
    bool converTo(Element *element, ImportData *imports, const QPsdAbstractLayerItem::ExportHint &hint) const;
    bool outputPath(const QPainterPath &path, Element *element) const;
    void findChildren(const QPsdAbstractLayerItem *item, QRect *rect) const;

    bool outputRect(const QRectF &rect, Element *element, bool skipEmpty = false) const;
    bool outputBase(const QPsdAbstractLayerItem *item, Element *element, ImportData *imports, QRect rectBounds = {}) const;
    bool outputFolder(const QPsdFolderLayerItem *folder, Element *element, ImportData *imports, ExportData *exports) const;
    bool outputText(const QPsdTextLayerItem *text, Element *element, ImportData *imports) const;
    bool outputShape(const QPsdShapeLayerItem *shape, Element *element, ImportData *imports, const QString &base = u"Rectangle"_s) const;
    bool outputImage(const QPsdImageLayerItem *image, Element *element, ImportData *imports) const;

    bool saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const;
};

bool QPsdExporterSlintPlugin::exportTo(const QPsdFolderLayerItem *tree, const QString &to, const QVariantMap &hint) const
{
    dir = QDir(to);

    const QSize originalSize = tree->rect().size();
    const QSize targetSize = hint.value("resolution", originalSize).toSize();
    horizontalScale = targetSize.width() / qreal(originalSize.width());
    verticalScale = targetSize.height() / qreal(originalSize.height());
    unitScale = std::min(horizontalScale, verticalScale);
    fontScaleFactor = hint.value("fontScaleFactor", 1.0).toReal() * verticalScale;
    makeCompact = hint.value("makeCompact", false).toBool();
    imageScaling = hint.value("imageScaling", false).toBool();

    auto children = tree->children();
    for (const auto *child : children) {
        if (!generateMergeData(child))
            return false;
    }

    ImportData imports;
    ExportData exports;

    Element window;
    window.type = "Window";
    outputRect(tree->rect(), &window);
    window.properties.insert("title", u"\"%1\""_s.arg(tree->name()));

    std::reverse(children.begin(), children.end());
    for (const auto *child : children) {
        if (!traverseTree(child, &window, &imports, &exports, QPsdAbstractLayerItem::ExportHint::None))
            return false;
    }

    return saveTo("MainWindow", &window, imports, exports);
}

bool QPsdExporterSlintPlugin::outputBase(const QPsdAbstractLayerItem *item, Element *element, ImportData *imports, QRect rectBounds) const
{
    Q_UNUSED(imports);
    QRect rect;
    if (rectBounds.isEmpty()) {
        rect = item->rect();
        if (makeCompact) {
            rect = rectMap.value(item);
        }
    } else {
        rect = rectBounds;
    }
    if (item->exportHint().type == QPsdAbstractLayerItem::ExportHint::Merge) {
        auto parent = mergeMap.key(item);
        while (parent) {
            rect.translate(-parent->rect().topLeft());
            parent = parent->parent();
        }
    }
    if (item->opacity() < 1.0) {
        element->properties.insert("opacity", item->opacity());
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

bool QPsdExporterSlintPlugin::outputFolder(const QPsdFolderLayerItem *folder, Element *element, ImportData *imports, ExportData *exports) const
{
    if (element->type.isEmpty())
        element->type = "Rectangle"_L1;
    if (!outputBase(folder, element, imports))
        return false;

    if (folder->artboardRect().isValid() && folder->artboardBackground() != Qt::transparent) {
        Element artboard;
        artboard.type = "Rectangle"_L1;
        outputRect(folder->artboardRect(), &artboard);
        artboard.properties.insert("color"_L1, folder->artboardBackground().name());
        element->children.append(artboard);
    }

    auto children = folder->children();
    std::reverse(children.begin(), children.end());
    for (const auto *child : children) {
        if (!traverseTree(child, element, imports, exports, QPsdAbstractLayerItem::ExportHint::None))
            return false;
    }
    return true;
};

bool QPsdExporterSlintPlugin::traverseTree(const QPsdAbstractLayerItem *item, Element *parent, ImportData *imports, ExportData *exports, QPsdAbstractLayerItem::ExportHint::Type hintOverload) const
{
    const auto hint = item->exportHint();
    const auto id = toKebabCase(hint.id);
    auto type = hint.type;
    if (hintOverload != QPsdAbstractLayerItem::ExportHint::None) {
        type = hintOverload;
    }

    switch (type) {
    case QPsdAbstractLayerItem::ExportHint::Embed: {
        Element element;
        element.id = id;
        outputBase(item, &element, imports);
        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            auto folder = reinterpret_cast<const QPsdFolderLayerItem *>(item);
            if (!id.isEmpty()) {
                outputFolder(folder, &element, imports, exports);
            } else {
                outputFolder(folder, parent, imports, exports);
                return true;
            }
            break; }
        case QPsdAbstractLayerItem::Text: {
            const auto text = reinterpret_cast<const QPsdTextLayerItem *>(item);
            outputText(text, &element, imports);
            break; }
        case QPsdAbstractLayerItem::Shape: {
            const auto shape = reinterpret_cast<const QPsdShapeLayerItem *>(item);
            outputShape(shape, &element, imports);
            break; }
        case QPsdAbstractLayerItem::Image: {
            const auto image = reinterpret_cast<const QPsdImageLayerItem *>(item);
            outputImage(image, &element, imports);
            break; }
        default:
            break;
        }

        if (mergeMap.contains(item)) {
            for (const auto *i : mergeMap.values(item)) {
                traverseTree(i, &element, imports, exports, QPsdAbstractLayerItem::ExportHint::Embed);
            }
        }

        if (!hint.visible)
            element.properties.insert("visible", "false");
        if (!id.isEmpty()) {
            if (hint.baseElement == QPsdAbstractLayerItem::ExportHint::TouchArea) {
                Element touchArea { "TouchArea", element.id, {}, {} };
                outputBase(item, &touchArea, imports);
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
    case QPsdAbstractLayerItem::ExportHint::Native: {
        Element element;
        element.id = id;
        outputBase(item, &element, imports);
        converTo(&element, imports, hint);
        if (element.type == "Button"_L1) {
            if (mergeMap.contains(item)) {
                for (const auto *i : mergeMap.values(item)) {
                    switch (i->type()) {
                    case QPsdAbstractLayerItem::Text: {
                        const auto *textItem = reinterpret_cast<const QPsdTextLayerItem *>(i);
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
    case QPsdAbstractLayerItem::ExportHint::Custom: {
        ImportData i;
        ExportData x;
        Element component;
        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            auto folder = reinterpret_cast<const QPsdFolderLayerItem *>(item);
            outputFolder(folder, &component, &i, &x);
            switch (hint.baseElement) {
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::Container:
                component.type = "";
                break;
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::TouchArea:
                component.type = "TouchArea";
                if (!id.isEmpty())
                    exports->append({"callback", id, "clicked", {}});
                break;
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button:
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button_Highlighted:
                (*imports)["std-widgets.slint"].insert("Button");
                component.type = "Button";
                component.properties.insert("primary", (hint.baseElement == QPsdAbstractLayerItem::ExportHint::NativeComponent::Button_Highlighted));
                if (!id.isEmpty())
                    exports->append({"callback", id, "clicked", {}});
                break;
            default:
                break;
            }
            if (!id.isEmpty()) {
                switch (hint.baseElement) {
                case QPsdAbstractLayerItem::ExportHint::NativeComponent::TouchArea:
                case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button:
                case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button_Highlighted:
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
            const auto text = reinterpret_cast<const QPsdTextLayerItem *>(item);
            outputText(text, &component, &i);
            if (!id.isEmpty() && hint.properties.contains("text"))
                exports->append({"string", id, "text", component.properties.value("text")});
            if (!id.isEmpty() && hint.properties.contains("color"))
                exports->append({"color", id, "color", component.properties.value("color")});
            break; }
        case QPsdAbstractLayerItem::Shape: {
            const auto shape = reinterpret_cast<const QPsdShapeLayerItem *>(item);
            switch (hint.baseElement) {
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::Container:
                outputShape(shape, &component, &i);
                break;
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::TouchArea:
                outputShape(shape, &component, &i, "TouchArea");
                break;
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button:
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button_Highlighted:
                i["std-widgets.slint"].insert("Button");
                outputShape(shape, &component, &i, "Button");
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
            const auto image = reinterpret_cast<const QPsdImageLayerItem *>(item);
            outputImage(image, &component, &i);
            if (!id.isEmpty()) {
                switch (hint.baseElement) {
                case QPsdAbstractLayerItem::ExportHint::NativeComponent::Container:
                    if (hint.properties.contains("visible"))
                        exports->append({"bool", id, "visible", component.properties.value("visible", true)});
                    break;
                case QPsdAbstractLayerItem::ExportHint::NativeComponent::TouchArea:
                case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button:
                case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button_Highlighted:
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
        outputBase(item, &element, imports);
        if (!hint.visible)
            element.properties.insert("visible", "false");
        parent->children.append(element);
        break; }
    case QPsdAbstractLayerItem::ExportHint::Merge:
    case QPsdAbstractLayerItem::ExportHint::Skip:
        return true;
    }
    return true;
};

bool QPsdExporterSlintPlugin::outputText(const QPsdTextLayerItem *text, Element *element, ImportData *imports) const
{
    const auto dropShadow = text->dropShadow();
    const auto runs = text->runs();
    if (runs.size() == 1) {
        const auto run = runs.first();
        element->type = "Text";
        if (!outputBase(text, element, imports, text->bounds().toRect()))
            return false;
        element->properties.insert("text", u"\"%1\""_s.arg(run.text.trimmed().replace("\n", "\\n")));
        element->properties.insert("font-family", u"\"%1\""_s.arg(run.font.family()));
        element->properties.insert("font-size", u"%1px"_s.ARGF(run.font.pointSizeF() / 1.5 * fontScaleFactor));
        element->properties.insert("color", run.color.name());
        element->properties.insert("horizontal-alignment", "center");
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
        if (!outputBase(text, element, imports, text->bounds().toRect()))
            return false;

        Element verticalLayout;
        verticalLayout.type = "VerticalLayout";
        verticalLayout.properties.insert("padding", 0);

        Element horizontalLayout;
        horizontalLayout.type = "HorizontalLayout";
        horizontalLayout.properties.insert("padding", 0);
        horizontalLayout.properties.insert("spacing", 0);

        for (const auto &run : runs) {
            const auto texts = run.text.split("\n");
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
                textElement.properties.insert("font-size", u"%1px"_s.ARGF(run.font.pointSizeF() / 1.5 * fontScaleFactor));
                textElement.properties.insert("color", run.color.name());
                textElement.properties.insert("horizontal-alignment", "center");
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

bool QPsdExporterSlintPlugin::outputShape(const QPsdShapeLayerItem *shape, Element *element, ImportData *imports, const QString &base) const
{
    const auto path = shape->pathInfo();
    switch (path.type) {
    case QPsdAbstractLayerItem::PathInfo::Rectangle:
    case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
        bool filled = (path.rect.topLeft() == QPointF(0, 0) && path.rect.size() == shape->rect().size());
        if (!filled || base != "Rectangle") {
            element->type = base;
            if (!outputBase(shape, element, imports))
                return false;
        }

        Element element2;
        element2.type = "Rectangle";
        if (filled) {
            if (!outputBase(shape, &element2, imports))
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
        if (!outputBase(shape, element, imports))
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

bool QPsdExporterSlintPlugin::outputImage(const QPsdImageLayerItem *image, Element *element, ImportData *imports) const
{
    QPsdImageStore imageStore(dir, "images"_L1);

    QString name;
    bool done = false;
    const auto linkedFile = image->linkedFile();
    if (!linkedFile.type.isEmpty()) {
        QImage qimage = image->linkedImage();
        if (!qimage.isNull()) {
            if (imageScaling) {
                qimage = qimage.scaled(image->rect().width() * horizontalScale, image->rect().height() * verticalScale, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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
        name = imageStore.save(imageFileName(image->name(), "PNG"_L1), qimage, "PNG");
    }

    element->type = "Image";
    if (!outputBase(image, element, imports))
        return false;
    element->properties.insert("source", u"@image-url(\"images/%1\")"_s.arg(name));
    element->properties.insert("image-fit", "contain");
    return true;
};

bool QPsdExporterSlintPlugin::converTo(Element *element, ImportData *imports, const QPsdAbstractLayerItem::ExportHint &hint) const
{
    switch (hint.baseElement) {
    case QPsdAbstractLayerItem::ExportHint::NativeComponent::Container:
        element->type = "Rectangle";
        break;
    case QPsdAbstractLayerItem::ExportHint::NativeComponent::TouchArea:
        element->type = "TouchArea";
        break;
    case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button_Highlighted:
        element->properties.insert("primary", (hint.baseElement == QPsdAbstractLayerItem::ExportHint::NativeComponent::Button_Highlighted));
        Q_FALLTHROUGH();
    case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button:
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

void QPsdExporterSlintPlugin::findChildren(const QPsdAbstractLayerItem *item, QRect *rect) const
{
    switch (item->type()) {
    case QPsdAbstractLayerItem::Folder: {
        const auto folder = reinterpret_cast<const QPsdFolderLayerItem *>(item);
        for (const auto *child : folder->children()) {
            findChildren(child, rect);
        }
        break; }
    default:
        *rect |= item->rect();
        break;
    }
};

void QPsdExporterSlintPlugin::generateRectMap(const QPsdAbstractLayerItem *item, const QPoint &topLeft) const
{
    switch (item->type()) {
    case QPsdAbstractLayerItem::Folder: {
        const auto folder = reinterpret_cast<const QPsdFolderLayerItem *>(item);
        QRect contentRect;
        if (!item->parent()->parent()) {
            contentRect = item->parent()->rect();
        // if (item->parent() == tree) {
        //     contentRect = tree->rect();
        } else {
            for (const auto *child : folder->children()) {
                findChildren(child, &contentRect);
            }
        }
        rectMap.insert(item, contentRect.translated(-topLeft));
        for (const auto *child : folder->children()) {
            generateRectMap(child, contentRect.topLeft());
        }
        break; }
    default:
        rectMap.insert(item, item->rect().translated(-topLeft));
        break;
    }
};

bool QPsdExporterSlintPlugin::generateMergeData(const QPsdAbstractLayerItem *item) const
{
    switch (item->type()) {
    case QPsdAbstractLayerItem::Folder: {
        const auto folder = reinterpret_cast<const QPsdFolderLayerItem *>(item);
        auto children = folder->children();
        std::reverse(children.begin(), children.end());
        for (const auto *child : children) {
            if (!generateMergeData(child))
                return false;
        }
        break; }
    default: {
        const auto hint = item->exportHint();
        if (hint.type != QPsdAbstractLayerItem::ExportHint::Merge)
            return true;
        const auto group = item->group();
        for (const auto *i : group) {
            if (i == item)
                continue;
            if (i->name() == hint.componentName) {
                mergeMap.insert(i, item);
            }
        }
        break; }
    }

    return true;
};

QT_END_NAMESPACE

#include "slint.moc"
