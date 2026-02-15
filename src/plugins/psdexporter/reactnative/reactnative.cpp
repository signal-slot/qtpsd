// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/qpsdexporterplugin.h>
#include <QtPsdExporter/qpsdimagestore.h>

#include <QtCore/QCborMap>
#include <QtCore/QDir>
#include <QtCore/QQueue>

#include <QtGui/QBrush>
#include <QtGui/QPen>

#include <QtPsdCore/QPsdSofiEffect>
#include <QtPsdGui/QPsdBorder>

QT_BEGIN_NAMESPACE

class QPsdExporterReactNativePlugin : public QPsdExporterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdExporterFactoryInterface" FILE "reactnative.json")
public:
    int priority() const override { return 50; }
    QIcon icon() const override {
        return QIcon(":/reactnative/reactnative.png");
    }
    QString name() const override {
        return tr("&React Native");
    }
    ExportType exportType() const override { return QPsdExporterPlugin::Directory; }

    bool exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const override;

    struct StyleProperty {
        QString name;
        QVariant value;
    };

    struct Element {
        QString type;
        QString id;
        QVariantHash props;
        QList<StyleProperty> styles;
        QList<Element> children;
        QString textContent;
    };

private:
    using ImportData = QSet<QString>;
    struct Export {
        QString type;
        QString name;
        QVariant defaultValue;
    };
    using ExportData = QList<Export>;

    mutable bool needsSvg = false;

    static QString colorValue(const QColor &color);
    static QString imagePath(const QString &name);

    bool outputRect(const QRectF &rect, Element *element, bool skipEmpty = false) const;
    bool outputBase(const QModelIndex &index, Element *element, QRect rectBounds = {}) const;
    bool outputFolder(const QModelIndex &folderIndex, Element *element, ImportData *imports, ExportData *exports) const;
    bool outputText(const QModelIndex &textIndex, Element *element, ImportData *imports) const;
    bool outputShape(const QModelIndex &shapeIndex, Element *element, ImportData *imports) const;
    bool outputImage(const QModelIndex &imageIndex, Element *element, ImportData *imports) const;
    QString outputPathData(const QPainterPath &path) const;

    bool traverseTree(const QModelIndex &index, Element *parent, ImportData *imports, ExportData *exports, QPsdExporterTreeItemModel::ExportHint::Type hintOverload) const;
    bool saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const;
};

QString QPsdExporterReactNativePlugin::colorValue(const QColor &color)
{
    if (color.alpha() == 255) {
        return u"'%1'"_s.arg(color.name().toUpper());
    } else {
        return u"'rgba(%1, %2, %3, %4)'"_s.arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alphaF(), 0, 'f', 2);
    }
}

QString QPsdExporterReactNativePlugin::imagePath(const QString &name)
{
    return u"require('../assets/images/%1')"_s.arg(name);
}

bool QPsdExporterReactNativePlugin::outputRect(const QRectF &rect, Element *element, bool skipEmpty) const
{
    element->styles.append({"left"_L1, qRound(rect.x() * horizontalScale)});
    element->styles.append({"top"_L1, qRound(rect.y() * verticalScale)});
    if (rect.isEmpty() && skipEmpty)
        return true;
    element->styles.append({"width"_L1, qRound(rect.width() * horizontalScale)});
    element->styles.append({"height"_L1, qRound(rect.height() * verticalScale)});
    return true;
}

bool QPsdExporterReactNativePlugin::outputBase(const QModelIndex &index, Element *element, QRect rectBounds) const
{
    const auto *item = model()->layerItem(index);
    QRect rect;
    if (rectBounds.isEmpty()) {
        rect = item->rect();
        if (makeCompact) {
            rect = indexRectMap.value(index);
        }
    } else {
        rect = rectBounds;
    }
    rect = adjustRectForMerge(index, rect);

    element->styles.append({"position"_L1, "'absolute'"_L1});
    outputRect(rect, element, true);

    const auto [combinedOpacity, hasEffects] = computeEffectiveOpacity(item);
    if (hasEffects) {
        if (item->opacity() < 1.0) {
            element->styles.append({"opacity"_L1, item->opacity()});
        }
    } else {
        if (combinedOpacity < 1.0) {
            element->styles.append({"opacity"_L1, combinedOpacity});
        }
    }

    return true;
}

bool QPsdExporterReactNativePlugin::outputFolder(const QModelIndex &folderIndex, Element *element, ImportData *imports, ExportData *exports) const
{
    const auto *folder = dynamic_cast<const QPsdFolderLayerItem *>(model()->layerItem(folderIndex));
    element->type = "View"_L1;
    if (!outputBase(folderIndex, element))
        return false;

    if (folder->artboardRect().isValid() && folder->artboardBackground() != Qt::transparent) {
        element->styles.append({"backgroundColor"_L1, colorValue(folder->artboardBackground())});
    }

    for (int i = model()->rowCount(folderIndex) - 1; i >= 0; i--) {
        QModelIndex childIndex = model()->index(i, 0, folderIndex);
        if (!traverseTree(childIndex, element, imports, exports, QPsdExporterTreeItemModel::ExportHint::None))
            return false;
    }
    return true;
}

bool QPsdExporterReactNativePlugin::outputText(const QModelIndex &textIndex, Element *element, ImportData *imports) const
{
    Q_UNUSED(imports);
    const auto *text = dynamic_cast<const QPsdTextLayerItem *>(model()->layerItem(textIndex));
    const auto runs = text->runs();

    if (runs.size() == 1) {
        const auto run = runs.first();
        element->type = "Text"_L1;
        QRect rect = computeTextBounds(text);
        if (!outputBase(textIndex, element, rect))
            return false;

        element->textContent = run.text.trimmed().replace("\n"_L1, "\\n"_L1);
        element->styles.append({"fontFamily"_L1, u"'%1'"_s.arg(run.font.family())});
        element->styles.append({"fontSize"_L1, qRound(run.font.pointSizeF() * fontScaleFactor)});
        element->styles.append({"color"_L1, colorValue(run.color)});

        if (run.font.bold()) {
            element->styles.append({"fontWeight"_L1, "'bold'"_L1});
        }
        if (run.font.italic()) {
            element->styles.append({"fontStyle"_L1, "'italic'"_L1});
        }

        element->styles.append({"textAlign"_L1,
            horizontalAlignmentString(run.alignment, {"'left'"_L1, "'right'"_L1, "'center'"_L1, "'justify'"_L1})});
    } else {
        // Multiple runs - wrap in View with nested Text elements
        element->type = "View"_L1;
        QRect multiRect = computeTextBounds(text);
        if (!outputBase(textIndex, element, multiRect))
            return false;

        for (const auto &run : runs) {
            const auto texts = run.text.trimmed().split("\n"_L1);
            for (const auto &textLine : texts) {
                Element textElement;
                textElement.type = "Text"_L1;
                textElement.textContent = textLine;
                textElement.styles.append({"fontFamily"_L1, u"'%1'"_s.arg(run.font.family())});
                textElement.styles.append({"fontSize"_L1, qRound(run.font.pointSizeF() * fontScaleFactor)});
                textElement.styles.append({"color"_L1, colorValue(run.color)});
                if (run.font.bold()) {
                    textElement.styles.append({"fontWeight"_L1, "'bold'"_L1});
                }
                if (run.font.italic()) {
                    textElement.styles.append({"fontStyle"_L1, "'italic'"_L1});
                }
                element->children.append(textElement);
            }
        }
    }
    return true;
}

QString QPsdExporterReactNativePlugin::outputPathData(const QPainterPath &path) const
{
    QString d;
    const auto commands = pathToCommands(path, horizontalScale, verticalScale);
    for (const auto &cmd : commands) {
        switch (cmd.type) {
        case PathCommand::MoveTo:
            d += u"M %1 %2 "_s.arg(cmd.x).arg(cmd.y);
            break;
        case PathCommand::LineTo:
            d += u"L %1 %2 "_s.arg(cmd.x).arg(cmd.y);
            break;
        case PathCommand::CubicTo:
            d += u"C %1 %2 %3 %4 %5 %6 "_s.arg(cmd.c1x).arg(cmd.c1y).arg(cmd.c2x).arg(cmd.c2y).arg(cmd.x).arg(cmd.y);
            break;
        case PathCommand::Close:
            d += "Z"_L1;
            break;
        }
    }
    return d.trimmed();
}

bool QPsdExporterReactNativePlugin::outputShape(const QModelIndex &shapeIndex, Element *element, ImportData *imports) const
{
    const auto *shape = dynamic_cast<const QPsdShapeLayerItem *>(model()->layerItem(shapeIndex));
    const auto path = shape->pathInfo();

    switch (path.type) {
    case QPsdAbstractLayerItem::PathInfo::Rectangle:
    case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
        element->type = "View"_L1;

        QRect rectBounds;
        if (shape->pen().style() != Qt::NoPen) {
            qreal dw = computeStrokeWidth(shape->pen(), unitScale);
            rectBounds = adjustRectForStroke(QRectF(shape->rect()), shape->strokeAlignment(), dw).toRect();
        }
        if (!outputBase(shapeIndex, element, rectBounds))
            return false;

        // Background color
        const QGradient *g = effectiveGradient(shape);
        if (g) {
            // React Native doesn't support gradients natively
            // Use first color as fallback
            if (!g->stops().isEmpty()) {
                element->styles.append({"backgroundColor"_L1, colorValue(g->stops().first().second)});
            }
        } else {
            if (shape->brush() != Qt::NoBrush) {
                element->styles.append({"backgroundColor"_L1, colorValue(shape->brush().color())});
            }
        }

        // Border
        if (shape->pen().style() != Qt::NoPen) {
            qreal dw = computeStrokeWidth(shape->pen(), unitScale);
            element->styles.append({"borderWidth"_L1, qRound(dw)});
            element->styles.append({"borderColor"_L1, colorValue(shape->pen().color())});
        }

        // Border radius
        if (path.type == QPsdAbstractLayerItem::PathInfo::RoundedRectangle) {
            element->styles.append({"borderRadius"_L1, qRound(path.radius * unitScale)});
        }
        break;
    }
    case QPsdAbstractLayerItem::PathInfo::Path: {
        // Use react-native-svg for complex paths
        needsSvg = true;
        imports->insert("react-native-svg"_L1);

        element->type = "Svg"_L1;
        if (!outputBase(shapeIndex, element))
            return false;

        // Get bounding rect for SVG viewBox
        const QRectF bounds = path.path.boundingRect();
        element->props.insert("width"_L1, qRound(bounds.width() * horizontalScale));
        element->props.insert("height"_L1, qRound(bounds.height() * verticalScale));

        Element pathElement;
        pathElement.type = "Path"_L1;
        pathElement.props.insert("d"_L1, outputPathData(path.path));

        // Fill color
        const QGradient *g = effectiveGradient(shape);
        if (g) {
            if (!g->stops().isEmpty()) {
                pathElement.props.insert("fill"_L1, shape->brush().color().name().toUpper());
            }
        } else {
            if (shape->brush() != Qt::NoBrush) {
                pathElement.props.insert("fill"_L1, shape->brush().color().name().toUpper());
            } else {
                pathElement.props.insert("fill"_L1, "none"_L1);
            }
        }

        // Stroke
        if (shape->pen().style() != Qt::NoPen) {
            pathElement.props.insert("stroke"_L1, shape->pen().color().name().toUpper());
            pathElement.props.insert("strokeWidth"_L1, qRound(shape->pen().width() * unitScale));
        }

        element->children.append(pathElement);
        break;
    }
    default:
        break;
    }

    return true;
}

bool QPsdExporterReactNativePlugin::outputImage(const QModelIndex &imageIndex, Element *element, ImportData *imports) const
{
    Q_UNUSED(imports);
    const auto *image = dynamic_cast<const QPsdImageLayerItem *>(model()->layerItem(imageIndex));

    QString name = saveLayerImage(image);

    element->type = "Image"_L1;
    if (!outputBase(imageIndex, element))
        return false;
    element->props.insert("source"_L1, imagePath(name));
    element->props.insert("resizeMode"_L1, "'contain'"_L1);

    const auto *border = image->border();
    if (border && border->isEnable()) {
        element->styles.append({"borderWidth"_L1, qRound(border->size() * unitScale)});
        element->styles.append({"borderColor"_L1, colorValue(border->color())});
    }

    return true;
}

bool QPsdExporterReactNativePlugin::traverseTree(const QModelIndex &index, Element *parent, ImportData *imports, ExportData *exports, QPsdExporterTreeItemModel::ExportHint::Type hintOverload) const
{
    const auto *item = model()->layerItem(index);
    const auto hint = model()->layerHint(index);
    const auto id = toLowerCamelCase(hint.id);
    auto type = hint.type;
    if (hintOverload != QPsdExporterTreeItemModel::ExportHint::None) {
        type = hintOverload;
    }

    switch (type) {
    case QPsdExporterTreeItemModel::ExportHint::Embed: {
        Element element;
        element.id = id;

        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder:
            outputFolder(index, &element, imports, exports);
            break;
        case QPsdAbstractLayerItem::Text:
            outputText(index, &element, imports);
            break;
        case QPsdAbstractLayerItem::Shape:
            outputShape(index, &element, imports);
            break;
        case QPsdAbstractLayerItem::Image:
            outputImage(index, &element, imports);
            break;
        default:
            break;
        }

        if (indexMergeMap.contains(index)) {
            const auto &list = indexMergeMap.values(index);
            for (auto it = list.constBegin(); it != list.constEnd(); it++) {
                traverseTree(*it, &element, imports, exports, QPsdExporterTreeItemModel::ExportHint::Embed);
            }
        }

        if (!hint.visible) {
            // Skip invisible elements or wrap in conditional
            // For now, we'll add a comment style
        }

        if (!id.isEmpty() && hint.baseElement == QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea) {
            Element touchable;
            touchable.type = "TouchableOpacity"_L1;
            touchable.id = id;
            imports->insert("react-native"_L1);

            // Copy position styles to touchable
            for (const auto &style : element.styles) {
                if (style.name == "position"_L1 || style.name == "left"_L1 ||
                    style.name == "top"_L1 || style.name == "width"_L1 ||
                    style.name == "height"_L1) {
                    touchable.styles.append(style);
                }
            }

            // Remove position styles from inner element
            QList<StyleProperty> innerStyles;
            for (const auto &style : element.styles) {
                if (style.name != "position"_L1 && style.name != "left"_L1 &&
                    style.name != "top"_L1) {
                    innerStyles.append(style);
                }
            }
            element.styles = innerStyles;
            element.id.clear();

            touchable.children.append(element);

            exports->append({"() => void"_L1, u"on%1Press"_s.arg(toUpperCamelCase(hint.id)), QVariant()});
            parent->children.append(touchable);
        } else {
            parent->children.append(element);
        }
        break;
    }
    case QPsdExporterTreeItemModel::ExportHint::Native: {
        Element element;
        element.id = id;

        switch (hint.baseElement) {
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
            element.type = "View"_L1;
            outputBase(index, &element);
            break;
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea:
            element.type = "TouchableOpacity"_L1;
            outputBase(index, &element);
            exports->append({"() => void"_L1, u"on%1Press"_s.arg(toUpperCamelCase(hint.id)), QVariant()});
            break;
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button:
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted:
            element.type = "TouchableOpacity"_L1;
            outputBase(index, &element);
            exports->append({"() => void"_L1, u"on%1Press"_s.arg(toUpperCamelCase(hint.id)), QVariant()});

            if (indexMergeMap.contains(index)) {
                for (auto it = indexMergeMap.constBegin(); it != indexMergeMap.constEnd(); it++) {
                    const auto *i = model()->layerItem(it.value());
                    switch (i->type()) {
                    case QPsdAbstractLayerItem::Text: {
                        Element textElem;
                        outputText(it.value(), &textElem, imports);
                        element.children.append(textElem);
                        break;
                    }
                    case QPsdAbstractLayerItem::Image: {
                        Element imageElem;
                        outputImage(it.value(), &imageElem, imports);
                        element.children.append(imageElem);
                        break;
                    }
                    case QPsdAbstractLayerItem::Shape: {
                        Element shapeElem;
                        outputShape(it.value(), &shapeElem, imports);
                        element.children.append(shapeElem);
                        break;
                    }
                    default:
                        qWarning() << i->type() << "is not supported in button";
                    }
                }
            }
            break;
        }

        if (!hint.visible) {
            // Could add visible prop handling
        }

        parent->children.append(element);
        break;
    }
    case QPsdExporterTreeItemModel::ExportHint::Component: {
        ImportData i;
        ExportData x;

        Element component;

        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder:
            outputFolder(index, &component, &i, &x);
            break;
        case QPsdAbstractLayerItem::Text:
            outputText(index, &component, &i);
            break;
        case QPsdAbstractLayerItem::Shape:
            outputShape(index, &component, &i);
            break;
        case QPsdAbstractLayerItem::Image:
            outputImage(index, &component, &i);
            break;
        default:
            break;
        }

        // Handle base element wrapping
        switch (hint.baseElement) {
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea:
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button:
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted: {
            Element wrapper;
            wrapper.type = "TouchableOpacity"_L1;
            wrapper.children.append(component);
            component = wrapper;
            x.append({"() => void"_L1, "onPress"_L1, QVariant()});
            break;
        }
        default:
            break;
        }

        saveTo(hint.componentName, &component, i, x);

        // Add import for custom component
        imports->insert(u"./%1"_s.arg(hint.componentName));

        Element element;
        element.type = hint.componentName;
        element.id = id;
        outputBase(index, &element);

        // Forward exports
        for (const auto &exp : x) {
            Export forwarded;
            forwarded.type = exp.type;
            forwarded.name = u"%1%2"_s.arg(id.isEmpty() ? hint.componentName : id, toUpperCamelCase(exp.name));
            forwarded.defaultValue = exp.defaultValue;
            exports->append(forwarded);
        }

        parent->children.append(element);
        break;
    }
    case QPsdExporterTreeItemModel::ExportHint::Merge:
    case QPsdExporterTreeItemModel::ExportHint::Skip:
    case QPsdExporterTreeItemModel::ExportHint::None:
        return true;
    }

    return true;
}

bool QPsdExporterReactNativePlugin::saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const
{
    QFile file(dir.absoluteFilePath(u"components/%1.tsx"_s.arg(baseName)));
    QDir().mkpath(dir.absoluteFilePath("components"_L1));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&file);

    // License header
    writeLicenseHeader(out);

    // Imports
    out << "import React from 'react';\n";

    // Collect React Native imports
    QStringList rnImports = {"View"_L1, "StyleSheet"_L1};
    std::function<void(const Element *)> collectImports;
    collectImports = [&](const Element *elem) {
        if (elem->type == "Text"_L1 && !rnImports.contains("Text"_L1)) {
            rnImports.append("Text"_L1);
        } else if (elem->type == "Image"_L1 && !rnImports.contains("Image"_L1)) {
            rnImports.append("Image"_L1);
        } else if (elem->type == "TouchableOpacity"_L1 && !rnImports.contains("TouchableOpacity"_L1)) {
            rnImports.append("TouchableOpacity"_L1);
        }
        for (const auto &child : elem->children) {
            collectImports(&child);
        }
    };
    collectImports(element);
    out << "import { " << rnImports.join(", "_L1) << " } from 'react-native';\n";

    // SVG imports if needed
    if (imports.contains("react-native-svg"_L1)) {
        QStringList svgNamedImports;
        std::function<void(const Element *)> collectSvgImports;
        collectSvgImports = [&](const Element *elem) {
            if (elem->type == "Path"_L1 && !svgNamedImports.contains("Path"_L1)) {
                svgNamedImports.append("Path"_L1);
            } else if (elem->type == "Rect"_L1 && !svgNamedImports.contains("Rect"_L1)) {
                svgNamedImports.append("Rect"_L1);
            } else if (elem->type == "Circle"_L1 && !svgNamedImports.contains("Circle"_L1)) {
                svgNamedImports.append("Circle"_L1);
            }
            for (const auto &child : elem->children) {
                collectSvgImports(&child);
            }
        };
        collectSvgImports(element);
        if (svgNamedImports.isEmpty()) {
            out << "import Svg from 'react-native-svg';\n";
        } else {
            out << "import Svg, { " << svgNamedImports.join(", "_L1) << " } from 'react-native-svg';\n";
        }
    }

    // Custom component imports
    for (const auto &import : imports) {
        if (import.startsWith("./"_L1)) {
            out << "import " << import.mid(2) << " from '" << import << "';\n";
        }
    }

    out << "\n";

    // Remove duplicate exports
    ExportData uniqueExports;
    QSet<QString> seenNames;
    for (const auto &exp : exports) {
        if (!seenNames.contains(exp.name)) {
            seenNames.insert(exp.name);
            uniqueExports.append(exp);
        }
    }

    // Props interface
    if (!uniqueExports.isEmpty()) {
        out << "interface " << baseName << "Props {\n";
        for (const auto &exp : uniqueExports) {
            out << "  " << exp.name << "?: " << exp.type << ";\n";
        }
        out << "}\n\n";
    }

    // Component
    if (uniqueExports.isEmpty()) {
        out << "const " << baseName << ": React.FC = () => {\n";
    } else {
        out << "const " << baseName << ": React.FC<" << baseName << "Props> = ({\n";
        for (const auto &exp : uniqueExports) {
            out << "  " << exp.name << ",\n";
        }
        out << "}) => {\n";
    }

    out << "  return (\n";

    // Collect all styles
    QHash<QString, QList<StyleProperty>> styleMap;
    int styleCounter = 0;

    std::function<QString(Element *, int)> renderElement;
    renderElement = [&](Element *elem, int indent) -> QString {
        QString result;
        QString indentStr = QString(indent * 2, ' ');

        // Generate style name
        QString styleName;
        if (!elem->id.isEmpty()) {
            styleName = elem->id;
        } else {
            styleName = u"style%1"_s.arg(styleCounter++);
        }

        // Store styles
        if (!elem->styles.isEmpty()) {
            styleMap.insert(styleName, elem->styles);
        }

        // Render element
        if (elem->type == "Text"_L1) {
            result += indentStr + "<Text";
            if (!elem->styles.isEmpty()) {
                result += u" style={styles.%1}"_s.arg(styleName);
            }
            result += ">";
            result += elem->textContent;
            result += "</Text>\n";
        } else if (elem->type == "Image"_L1) {
            result += indentStr + "<Image\n";
            if (elem->props.contains("source"_L1)) {
                result += indentStr + "  source={" + elem->props.value("source"_L1).toString() + "}\n";
            }
            if (!elem->styles.isEmpty()) {
                result += indentStr + u"  style={styles.%1}\n"_s.arg(styleName);
            }
            if (elem->props.contains("resizeMode"_L1)) {
                result += indentStr + "  resizeMode=" + elem->props.value("resizeMode"_L1).toString() + "\n";
            }
            result += indentStr + "/>\n";
        } else if (elem->type == "Svg"_L1) {
            result += indentStr + "<Svg";
            if (elem->props.contains("width"_L1)) {
                result += u" width={%1}"_s.arg(elem->props.value("width"_L1).toInt());
            }
            if (elem->props.contains("height"_L1)) {
                result += u" height={%1}"_s.arg(elem->props.value("height"_L1).toInt());
            }
            if (!elem->styles.isEmpty()) {
                result += u" style={styles.%1}"_s.arg(styleName);
            }
            result += ">\n";
            for (auto &child : elem->children) {
                result += renderElement(&child, indent + 1);
            }
            result += indentStr + "</Svg>\n";
        } else if (elem->type == "Path"_L1) {
            result += indentStr + "<Path\n";
            if (elem->props.contains("d"_L1)) {
                result += indentStr + "  d=\"" + elem->props.value("d"_L1).toString() + "\"\n";
            }
            if (elem->props.contains("fill"_L1)) {
                result += indentStr + "  fill=\"" + elem->props.value("fill"_L1).toString() + "\"\n";
            }
            if (elem->props.contains("stroke"_L1)) {
                result += indentStr + "  stroke=\"" + elem->props.value("stroke"_L1).toString() + "\"\n";
            }
            if (elem->props.contains("strokeWidth"_L1)) {
                result += indentStr + u"  strokeWidth={%1}\n"_s.arg(elem->props.value("strokeWidth"_L1).toInt());
            }
            result += indentStr + "/>\n";
        } else if (elem->type == "TouchableOpacity"_L1) {
            result += indentStr + "<TouchableOpacity";
            if (!elem->styles.isEmpty()) {
                result += u" style={styles.%1}"_s.arg(styleName);
            }
            if (!elem->id.isEmpty()) {
                result += u" onPress={on%1Press}"_s.arg(toUpperCamelCase(elem->id));
            }
            result += ">\n";
            for (auto &child : elem->children) {
                result += renderElement(&child, indent + 1);
            }
            result += indentStr + "</TouchableOpacity>\n";
        } else {
            // View or other elements
            result += indentStr + "<" + elem->type;
            if (!elem->styles.isEmpty()) {
                result += u" style={styles.%1}"_s.arg(styleName);
            }
            if (elem->children.isEmpty() && elem->textContent.isEmpty()) {
                result += " />\n";
            } else {
                result += ">\n";
                for (auto &child : elem->children) {
                    result += renderElement(&child, indent + 1);
                }
                result += indentStr + "</" + elem->type + ">\n";
            }
        }

        return result;
    };

    out << renderElement(element, 2);

    out << "  );\n";
    out << "};\n\n";

    // StyleSheet
    out << "const styles = StyleSheet.create({\n";
    auto styleKeys = styleMap.keys();
    std::sort(styleKeys.begin(), styleKeys.end());
    for (const auto &key : styleKeys) {
        out << "  " << key << ": {\n";
        const auto &props = styleMap.value(key);
        for (const auto &prop : props) {
            QString valueStr;
            switch (prop.value.typeId()) {
            case QMetaType::QString:
                valueStr = prop.value.toString();
                break;
            case QMetaType::Int:
                valueStr = QString::number(prop.value.toInt());
                break;
            case QMetaType::Double:
                valueStr = QString::number(prop.value.toDouble());
                break;
            default:
                valueStr = prop.value.toString();
            }
            out << "    " << prop.name << ": " << valueStr << ",\n";
        }
        out << "  },\n";
    }
    out << "});\n\n";

    out << "export default " << baseName << ";\n";

    return true;
}

bool QPsdExporterReactNativePlugin::exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const
{
    if (!initializeExport(model, to, hint, "assets/images"_L1)) {
        return false;
    }
    needsSvg = false;

    // Create directories
    QDir().mkpath(dir.absoluteFilePath("assets/images"_L1));
    QDir().mkpath(dir.absoluteFilePath("components"_L1));

    ImportData imports;
    ExportData exports;

    Element root;
    root.type = "View"_L1;
    root.id = "root"_L1;
    root.styles.append({"flex"_L1, 1});
    root.styles.append({"width"_L1, qRound(model->size().width() * horizontalScale)});
    root.styles.append({"height"_L1, qRound(model->size().height() * verticalScale)});
    root.styles.append({"backgroundColor"_L1, "'#FFFFFF'"_L1});

    for (int i = model->rowCount(QModelIndex {}) - 1; i >= 0; i--) {
        QModelIndex childIndex = model->index(i, 0, QModelIndex {});
        if (!traverseTree(childIndex, &root, &imports, &exports, QPsdExporterTreeItemModel::ExportHint::None))
            return false;
    }

    return saveTo("MainScreen", &root, imports, exports);
}

QT_END_NAMESPACE

#include "reactnative.moc"
