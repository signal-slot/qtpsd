// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "flutter.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QGradient>
#include <QRegularExpression>
#include <QImageReader>
#include <QImageWriter>

QT_BEGIN_NAMESPACE

class QPsdExporterFlutterPlugin : public QPsdExporterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdExporterFactoryInterface" FILE "flutter.json")
public:
    int priority() const override { return 50; }
    QIcon icon() const override {
        return QIcon(":/flutter/flutter.png");
    }
    QString name() const override {
        return u"Flutter"_s;
    }

private:
    struct Element {
        QString type;
        QVariant noNamedParam;
        QHash<QString, QVariant> properties;
        QList<Element> children;
    };

    struct PropertyInfo {
        QString type;
        QString format;
        QString objname;
        QString defValue = {};

        QString name() const {
            return toLowerCamelCase(format.arg(objname));
        }

        QString defaultValue() const {
            if (defValue.isEmpty()) {
                return {};
            }
            if (type == "String") {
                return u"'%1'"_s.arg(defValue);
            } else {
                return defValue;
            }
        }
    };

    using ImportData = QSet<QString>;
    using ExportData = QHash<QString, PropertyInfo>;

    mutable QDir dir;
    mutable QPsdImageStore imageStore;
    mutable qreal horizontalScale = 1.0;
    mutable qreal verticalScale = 1.0;
    mutable qreal unitScale = 1.0;
    mutable qreal fontScaleFactor = 1.0;
    mutable bool makeCompact = false;
    mutable bool imageScaling = false;
    mutable QHash<const QPsdAbstractLayerItem *, QRect> rectMap;
    mutable QMultiMap<const QPsdAbstractLayerItem *, const QPsdAbstractLayerItem *> mergeMap;

    // Value Formatting Methods
    QString indentString(int level) const;
    QString valueAsText(const QVariant &value) const;
    QString imagePath(const QString &path) const;
    QString colorValue(const QColor &color) const;

    // Utility Methods
    void findChildren(const QPsdAbstractLayerItem *item, QRect *rect) const;
    void generateRectMap(const QPsdAbstractLayerItem *item, const QPoint &topLeft) const;
    bool generateMergeData(const QPsdAbstractLayerItem *item) const;
    bool outputRectProp(const QRectF &rect, Element *element, bool skipEmpty = false, bool outputPos = false) const;
    bool outputPath(const QPainterPath &path, Element *element) const;
    bool outputPositioned(const QPsdAbstractLayerItem *item, Element *element) const;
    bool outputPositionedTextBounds(const QPsdTextLayerItem *item, Element *element) const;
    bool outputTextElement(const QPsdTextLayerItem::Run run, const QString &text, Element *element) const;
    bool outputGradient(const QGradient *gradient, const QRectF &rect, Element *element) const;

    // Core Output Methods
    bool outputText(const QPsdTextLayerItem *text, Element *element) const;
    bool outputShape(const QPsdShapeLayerItem *shape, Element *element, ImportData *imports, ExportData *exports) const;
    bool outputImage(const QPsdImageLayerItem *image, Element *element) const;
    bool outputFolder(const QPsdFolderLayerItem *folder, Element *element, ImportData *imports, ExportData *exports) const;

    // High-Level Methods
    bool traverseElement(QTextStream &out, const Element *element, int level, bool skipFirstIndent) const;
    bool traverseTree(const QPsdAbstractLayerItem *item, Element *parent, ImportData *imports, ExportData *exports, QPsdAbstractLayerItem::ExportHint::Type hintOverload) const;
    bool saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const;
    bool exportTo(const QPsdFolderLayerItem *tree, const QString &to, const QVariantMap &hint) const override;
};

Q_DECLARE_METATYPE(QPsdExporterFlutterPlugin::Element)

// Value Formatting Methods
QString QPsdExporterFlutterPlugin::indentString(int level) const
{
    return QString(level * 2, ' ');
}

QString QPsdExporterFlutterPlugin::valueAsText(const QVariant &value) const
{
    switch (value.typeId()) {
    case QMetaType::Bool:
        return value.toBool() ? "true"_L1 : "false"_L1;
    case QMetaType::Int:
    case QMetaType::Double:
        return value.toString();
    case QMetaType::QString:
        return u"'%1'"_s.arg(value.toString());
    case QMetaType::QVariantList: {
        QString result = "["_L1;
        const auto list = value.toList();
        for (const auto &item : list) {
            result += valueAsText(item) + ", "_L1;
        }
        result += "]"_L1;
        return result;
    }
    default:
        return value.toString();
    }
}

QString QPsdExporterFlutterPlugin::imagePath(const QString &path) const
{
    return u"'%1'"_s.arg(path);
}

QString QPsdExporterFlutterPlugin::colorValue(const QColor &color) const
{
    return u"Color(0x%1)"_s.arg(color.rgba(), 8, 16, QLatin1Char('0'));
}

// Utility Methods
void QPsdExporterFlutterPlugin::findChildren(const QPsdAbstractLayerItem *item, QRect *rect) const
{
    const auto children = item->children();
    for (const auto *child : children) {
        if (child->exportHint().type == QPsdAbstractLayerItem::ExportHint::Skip)
            continue;
        if (child->exportHint().type == QPsdAbstractLayerItem::ExportHint::Merge)
            continue;
        if (child->rect().isValid())
            rect->setRect(std::min(rect->left(), child->rect().left()),
                         std::min(rect->top(), child->rect().top()),
                         std::max(rect->right(), child->rect().right()),
                         std::max(rect->bottom(), child->rect().bottom()));
        findChildren(child, rect);
    }
}

void QPsdExporterFlutterPlugin::generateRectMap(const QPsdAbstractLayerItem *item, const QPoint &topLeft) const
{
    const auto children = item->children();
    for (const auto *child : children) {
        if (child->exportHint().type == QPsdAbstractLayerItem::ExportHint::Skip)
            continue;
        if (child->exportHint().type == QPsdAbstractLayerItem::ExportHint::Merge)
            continue;

        QRect rect;
        if (child->rect().isValid()) {
            rect = child->rect();
        } else {
            rect.setCoords(INT_MAX, INT_MAX, INT_MIN, INT_MIN);
            findChildren(child, &rect);
        }
        rect.translate(topLeft);
        rectMap.insert(child, rect);

        generateRectMap(child, rect.topLeft());
    }
}

bool QPsdExporterFlutterPlugin::generateMergeData(const QPsdAbstractLayerItem *item) const
{
    const auto children = item->children();
    for (const auto *child : children) {
        if (child->exportHint().type == QPsdAbstractLayerItem::ExportHint::Skip)
            continue;

        if (child->exportHint().type == QPsdAbstractLayerItem::ExportHint::Merge) {
            const auto *parent = child->parent();
            while (parent) {
                if (parent->exportHint().type == QPsdAbstractLayerItem::ExportHint::Native) {
                    mergeMap.insert(parent, child);
                    break;
                }
                parent = parent->parent();
            }
            if (!parent) {
                qWarning() << "No native component found for" << child->name();
                return false;
            }
        }

        if (!generateMergeData(child))
            return false;
    }
    return true;
}

bool QPsdExporterFlutterPlugin::outputRectProp(const QRectF &rect, Element *element, bool skipEmpty, bool outputPos) const
{
    if (rect.isEmpty() && skipEmpty)
        return false;

    if (outputPos) {
        element->properties.insert("left", rect.left() * horizontalScale);
        element->properties.insert("top", rect.top() * verticalScale);
    }
    element->properties.insert("width", rect.width() * horizontalScale);
    element->properties.insert("height", rect.height() * verticalScale);

    return true;
}

bool QPsdExporterFlutterPlugin::outputPath(const QPainterPath &path, Element *element) const
{
    QList<QVariant> pathElements;

    for (int i = 0; i < path.elementCount(); ++i) {
        const auto elem = path.elementAt(i);
        switch (elem.type) {
        case QPainterPath::MoveToElement:
            pathElements.append(u"moveTo"_s);
            pathElements.append(elem.x * horizontalScale);
            pathElements.append(elem.y * verticalScale);
            break;
        case QPainterPath::LineToElement:
            pathElements.append(u"lineTo"_s);
            pathElements.append(elem.x * horizontalScale);
            pathElements.append(elem.y * verticalScale);
            break;
        case QPainterPath::CurveToElement: {
            const auto c1 = path.elementAt(i);
            const auto c2 = path.elementAt(i + 1);
            const auto endPoint = path.elementAt(i + 2);
            pathElements.append(u"cubicTo"_s);
            pathElements.append(c1.x * horizontalScale);
            pathElements.append(c1.y * verticalScale);
            pathElements.append(c2.x * horizontalScale);
            pathElements.append(c2.y * verticalScale);
            pathElements.append(endPoint.x * horizontalScale);
            pathElements.append(endPoint.y * verticalScale);
            i += 2;
            break;
        }
        case QPainterPath::CurveToDataElement:
            break;
        }
    }

    if (path.elementCount() > 0) {
        element->properties.insert("path", QVariant::fromValue(pathElements));
    }

    return true;
}

bool QPsdExporterFlutterPlugin::outputPositioned(const QPsdAbstractLayerItem *item, Element *element) const
{
    const auto rect = rectMap.value(item);
    if (!rect.isValid())
        return false;

    element->type = "Positioned";
    return outputRectProp(rect, element, false, true);
}

bool QPsdExporterFlutterPlugin::outputPositionedTextBounds(const QPsdTextLayerItem *item, Element *element) const
{
    const auto rect = rectMap.value(item);
    if (!rect.isValid())
        return false;

    element->type = "Positioned";
    return outputRectProp(rect, element, false, true);
}

bool QPsdExporterFlutterPlugin::outputTextElement(const QPsdTextLayerItem::Run run, const QString &text, Element *element) const
{
    element->type = "Text";
    element->noNamedParam = text;

    Element textStyle;
    textStyle.type = "TextStyle";
    textStyle.properties.insert("fontSize", run.size * fontScaleFactor);
    textStyle.properties.insert("color", colorValue(run.color));
    if (run.font.bold()) {
        textStyle.properties.insert("fontWeight", "FontWeight.bold");
    }
    if (run.font.italic()) {
        textStyle.properties.insert("fontStyle", "FontStyle.italic");
    }
    element->properties.insert("style", QVariant::fromValue(textStyle));

    return true;
}

bool QPsdExporterFlutterPlugin::outputGradient(const QGradient *gradient, const QRectF &rect, Element *element) const
{
    switch (gradient->type()) {
    case QGradient::LinearGradient: {
        const auto *g = static_cast<const QLinearGradient *>(gradient);
        element->type = "LinearGradient";
        element->properties.insert("begin", u"Alignment(%1, %2)"_s.arg(g->start().x()).arg(g->start().y()));
        element->properties.insert("end", u"Alignment(%1, %2)"_s.arg(g->finalStop().x()).arg(g->finalStop().y()));
        break;
    }
    case QGradient::RadialGradient: {
        const auto *g = static_cast<const QRadialGradient *>(gradient);
        element->type = "RadialGradient";
        element->properties.insert("center", u"Alignment(%1, %2)"_s.arg(g->center().x()).arg(g->center().y()));
        element->properties.insert("radius", g->radius());
        break;
    }
    default:
        return false;
    }

    QList<QVariant> colors;
    QList<QVariant> stops;
    const auto stops_ = gradient->stops();
    for (const auto &stop : stops_) {
        colors.append(colorValue(stop.second));
        stops.append(stop.first);
    }
    element->properties.insert("colors", QVariant::fromValue(colors));
    element->properties.insert("stops", QVariant::fromValue(stops));

    return true;
}

// Core Output Methods
bool QPsdExporterFlutterPlugin::outputText(const QPsdTextLayerItem *text, Element *element) const
{
    const auto runs = text->runs();

    Element columnElem;
    columnElem.type = "Column";
    Element rowElem;
    rowElem.type = "Row";

    for (const auto &run : runs) {
        auto texts = run.text.trimmed().split("\n");
        bool first = true;
        for (const auto &text : texts) {
            if (first) {
                first = false;
            } else {
                if (rowElem.children.size() == 1) {
                    columnElem.children.append(rowElem.children.at(0));
                } else {
                    columnElem.children.append(rowElem);
                }
                rowElem.children.clear();
            }

            Element textElement;
            outputTextElement(run, text, &textElement);
            rowElem.children.append(textElement);
        }
    }

    if (rowElem.children.size() == 1) {
        columnElem.children.append(rowElem.children.at(0));
    } else {
        columnElem.children.append(rowElem);
    }
    *element = columnElem;

    return true;
}

bool QPsdExporterFlutterPlugin::outputShape(const QPsdShapeLayerItem *shape, Element *element, ImportData *imports, ExportData *exports) const
{
    const auto hint = shape->exportHint();
    if (hint.type == QPsdAbstractLayerItem::ExportHint::Custom) {
        imports->insert("package:flutter/material.dart");
    }

    element->type = "CustomPaint";
    element->properties.insert("painter", u"%1Painter"_s.arg(hint.id));

    PropertyInfo prop {
        "double"_L1, "%1_opacity"_L1, hint.id, QString::number(shape->opacity())
    };
    element->properties.insert("opacity", prop.name());
    exports->insert(prop.name(), prop);

    return true;
}

bool QPsdExporterFlutterPlugin::outputImage(const QPsdImageLayerItem *image, Element *element) const
{
    element->type = "Image";

    const auto path = imageStore.store(image->image());
    if (path.isEmpty())
        return false;

    element->properties.insert("image", u"AssetImage(%1)"_s.arg(imagePath(path)));

    if (imageScaling) {
        element->properties.insert("width", image->rect().width() * horizontalScale);
        element->properties.insert("height", image->rect().height() * verticalScale);
        element->properties.insert("fit", "BoxFit.fill");
    }

    return true;
}

bool QPsdExporterFlutterPlugin::outputFolder(const QPsdFolderLayerItem *folder, Element *element, ImportData *imports, ExportData *exports) const
{
    element->type = "Container";
    if (folder->artboardRect().isValid() && folder->artboardBackground() != Qt::transparent) {
        element->properties.insert("color", colorValue(folder->artboardBackground()));
        if (!outputRectProp(folder->artboardRect(), element))
            return false;
    }
    auto children = folder->children();
    std::reverse(children.begin(), children.end());

    //TODO don't need Stack widget if chidlen.size() == 1
    Element stackElement;
    stackElement.type = "Stack";
    for (const auto *child : children) {
        if (!traverseTree(child, &stackElement, imports, exports, QPsdAbstractLayerItem::ExportHint::None))
            return false;
    }
    element->properties.insert("child", QVariant::fromValue(stackElement));

    return true;
}

// High-Level Methods
bool QPsdExporterFlutterPlugin::traverseElement(QTextStream &out, const Element *element, int level, bool skipFirstIndent) const
{
    if (!skipFirstIndent) {
        out << indentString(level);
    }

    out << element->type << "(\n";
    level++;

    if (element->noNamedParam.isValid() && element->noNamedParam.typeId() != QMetaType::Void) {
        out << indentString(level) << valueAsText(element->noNamedParam) << ", \n";
    }

    auto keys = element->properties.keys();
    QList<QString> elementKeys;
    std::sort(keys.begin(), keys.end(), std::less<QString>());
    for (const auto &key : keys) {
        const auto value = element->properties.value(key);

        if (value.typeId() == qMetaTypeId<Element>()) {
            elementKeys.append(key);
        } else if (value.typeId() == QMetaType::QVariantList) {
            out << indentString(level) << key << ": " << "[\n";
            for (const auto &item : value.value<QVariantList>()) {
                out << indentString(level + 1) << valueAsText(item) << ",\n";
            }
            out << indentString(level) << "],\n";
        } else {
            out << indentString(level) << key << ": " << valueAsText(value) << ",\n";
        }
    }

    QVariant childValue;
    for (const auto &key : elementKeys) {
        const auto value = element->properties.value(key);

        if (value.typeId() == qMetaTypeId<Element>()) {
            if (key == "child") {
                childValue = value;
            } else {
                Element elem = value.value<Element>();
                out << indentString(level) << key << ": ";

                traverseElement(out, &elem, level, true);
                out << ",\n";
            }
        }
    }

    if (childValue.isValid()) {
        Element elem = childValue.value<Element>();
        out << indentString(level) << "child: ";

        traverseElement(out, &elem, level, true);
        out << ",\n";
    }
    
    if (!element->children.isEmpty()) {
        out << indentString(level) << "children: [\n";

        for (auto &child : element->children) {
                traverseElement(out, &child, level + 1, false);
            out << ",\n";
        }

        out << indentString(level) << "],\n";
    }

    level--;
    out << indentString(level) << ")";
    return true;
}

bool QPsdExporterFlutterPlugin::traverseTree(const QPsdAbstractLayerItem *item, Element *parent, ImportData *imports, ExportData *exports, QPsdAbstractLayerItem::ExportHint::Type hintOverload) const
{
    const auto hint = item->exportHint();
    const auto id = toLowerCamelCase(hint.id);
    auto type = hint.type;
    if (hintOverload != QPsdAbstractLayerItem::ExportHint::None) {
        type = hintOverload;
    }
    switch (type) {
    case QPsdAbstractLayerItem::ExportHint::Embed: {
        Element element;
        Element positionedElement;
        bool existsPositioned = false;
        Element visibilityElement;

        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            auto folder = reinterpret_cast<const QPsdFolderLayerItem *>(item);
            outputFolder(folder, &element, imports, exports);
            break; }
        case QPsdAbstractLayerItem::Text: {
            const auto text = reinterpret_cast<const QPsdTextLayerItem *>(item);
            outputText(text, &element);
            existsPositioned = outputPositionedTextBounds(text, &positionedElement);
            break; }
        case QPsdAbstractLayerItem::Shape: {
            const auto shape = reinterpret_cast<const QPsdShapeLayerItem *>(item);
            outputShape(shape, &element, imports, exports);
            existsPositioned = outputPositioned(item, &positionedElement);
            break; }
        case QPsdAbstractLayerItem::Image: {
            const auto image = reinterpret_cast<const QPsdImageLayerItem *>(item);
            outputImage(image, &element);
            existsPositioned = outputPositioned(item, &positionedElement);
            break; }
        default:
            break;
        }

        Element *pElement = &element;

        if (hint.properties.contains("visible")) {
            PropertyInfo prop {
                "bool"_L1, "%1_visibility"_L1, hint.id, hint.visible ? "true"_L1 : "false"_L1
            };
            visibilityElement.type = "Visibility";
            visibilityElement.properties.insert("visible", prop.name());
            visibilityElement.properties.insert("child", QVariant::fromValue(*pElement));
            exports->insert(prop.name(), prop);
            pElement = &visibilityElement;
        }

        if (existsPositioned) {
            positionedElement.properties.insert("child"_L1, QVariant::fromValue(*pElement));
            pElement = &positionedElement;
        }

        parent->children.append(*pElement);
        break; }
    case QPsdAbstractLayerItem::ExportHint::Native: {
        Element element;
        switch (hint.baseElement) {
        case QPsdAbstractLayerItem::ExportHint::NativeComponent::Container:
            element.type = "Container"_L1;
            break;
        case QPsdAbstractLayerItem::ExportHint::NativeComponent::TouchArea: {
            PropertyInfo prop {
                "void Function()?"_L1, "on_%1_Tap"_L1, hint.id
            };
            exports->insert(prop.name(), prop);

            element.type = "GestureDetector"_L1;
            element.properties.insert("onTap"_L1, prop.name());
            element.properties.insert("behavior"_L1, "HitTestBehavior.opaque"_L1);
            break;
        }
        case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button:
        case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button_Highlighted: {
            PropertyInfo prop {
                "void Function()?"_L1, "on_%1_Pressed"_L1, hint.id
            };
            exports->insert(prop.name(), prop);
            if (hint.baseElement == QPsdAbstractLayerItem::ExportHint::NativeComponent::Button) {
                element.type = "ElevatedButton"_L1;
            } else {
                element.type = "FilledButton"_L1;
            }
            element.properties.insert("onPressed"_L1, prop.name());

            if (mergeMap.contains(item)) {
                for (const auto *i : mergeMap.values(item)) {
                    switch (i->type()) {
                    case QPsdAbstractLayerItem::Text: {
                        const auto *textItem = reinterpret_cast<const QPsdTextLayerItem *>(i);
                        Element textElem;
                        outputText(textItem, &textElem);
                        element.properties.insert("child", QVariant::fromValue(textElem));
                        break; }
                    default:
                        qWarning() << i->type() << "is not supported";
                    }
                }
            } else {
                element.properties.insert("child", "null");
            }
            break;
        }
        }
        Element visibilityElement;
        Element *pElement = &element;

        if (hint.properties.contains("visible")) {
            PropertyInfo prop {
                "bool"_L1, "%1_visibility"_L1, hint.id, hint.visible ? "true"_L1 : "false"_L1
            };
            visibilityElement.type = "Visibility";
            visibilityElement.properties.insert("visible", prop.name());
            visibilityElement.properties.insert("child", QVariant::fromValue(element));
            exports->insert(prop.name(), prop);
            pElement = &visibilityElement;
        }

        Element positionedElement;
        if (outputPositioned(item, &positionedElement)) {
            positionedElement.properties.insert("child", QVariant::fromValue(*pElement));
            parent->children.append(positionedElement);
        } else {
            parent->children.append(*pElement);
        }
        break;
    }
    case QPsdAbstractLayerItem::ExportHint::Custom: {
        ImportData i;
        i.insert("package:flutter/material.dart");
        ExportData x;

        Element component;
        Element positionedElement;
        bool existsPositioned = false;

        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            auto folder = reinterpret_cast<const QPsdFolderLayerItem *>(item);
            outputFolder(folder, &component, &i, &x);
            break; }
        case QPsdAbstractLayerItem::Text: {
            const auto text = reinterpret_cast<const QPsdTextLayerItem *>(item);
            outputText(text, &component);
            existsPositioned = outputPositionedTextBounds(text, &positionedElement);
            break; }
        case QPsdAbstractLayerItem::Shape: {
            const auto shape = reinterpret_cast<const QPsdShapeLayerItem *>(item);
            outputShape(shape, &component, &i, &x);
            existsPositioned = outputPositioned(item, &positionedElement);
            break; }
        case QPsdAbstractLayerItem::Image: {
            const auto image = reinterpret_cast<const QPsdImageLayerItem *>(item);
            outputImage(image, &component);
            existsPositioned = outputPositioned(item, &positionedElement);
            break; }
        }

        Element base;
        bool isMaterial = false;
        switch (hint.baseElement) {
        case QPsdAbstractLayerItem::ExportHint::NativeComponent::Container:
            base.type = "Container";
            break;
        case QPsdAbstractLayerItem::ExportHint::NativeComponent::TouchArea: {
            PropertyInfo prop {
                "void Function()?"_L1, "on_%1_Tap"_L1, {}
            };
            x.insert(prop.name(), prop);
            base.type = "InkWell";
            base.properties.insert("onTap", prop.name());
            isMaterial = true;
            break;
        }
        case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button: {
            PropertyInfo prop {
                "void Function()?"_L1, "on_%1_Pressed"_L1, {}
            };
            x.insert(prop.name(), prop);
            base.type = "ElevatedButton";
            base.properties.insert("onPressed", prop.name());
            break;
        }
        case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button_Highlighted: {
            PropertyInfo prop {
                "void Function()?"_L1, "on_%1_Pressed"_L1, {}
            };
            x.insert(prop.name(), prop);
            base.type = "FilledButton";
            base.properties.insert("onPressed", prop.name());
            break;
        }
        }

        base.properties.insert("child", QVariant::fromValue(component));
        Element *pBase = &base;
        Element materialElement;
        if (isMaterial) {
            materialElement.type = "Material";
            materialElement.properties.insert("child", QVariant::fromValue(base));
            pBase = &materialElement;
        }

        Element classElement;
        classElement.type = hint.componentName;
        if (existsPositioned) {
            positionedElement.properties.insert("child", QVariant::fromValue(*pBase));
            classElement.properties.insert("child", QVariant::fromValue(positionedElement));
        } else {
            classElement.properties.insert("child", QVariant::fromValue(*pBase));
        }

        saveTo(hint.componentName, &classElement, i, x);

        imports->insert(u"./%1.dart"_s.arg(toSnakeCase(hint.componentName)));

        Element element;
        element.type = hint.componentName;
        const auto keys = x.keys();
        for (const auto &key : keys) {
            const PropertyInfo &iprop = x.value(key);
            PropertyInfo prop {
                iprop.type,
                iprop.format,
                "%1_%2"_L1.arg(hint.id.isEmpty() ? hint.componentName : hint.id, iprop.objname)
            };
            exports->insert(prop.name(), prop);
            element.properties.insert(iprop.name(), prop.name());
        }

        Element visibilityElement;
        if (hint.properties.contains("visible")) {
            PropertyInfo prop {
                "bool"_L1, "%1_visibility"_L1, hint.componentName, hint.visible ? "true"_L1 : "false"_L1
            };
            visibilityElement.type = "Visibility";
            visibilityElement.properties.insert("visible", prop.name());
            visibilityElement.properties.insert("child", QVariant::fromValue(element));
            exports->insert(prop.name(), prop);
            parent->children.append(visibilityElement);
        } else {
            parent->children.append(element);
        }

        break;
    }
    case QPsdAbstractLayerItem::ExportHint::Merge:
    case QPsdAbstractLayerItem::ExportHint::Skip:
        return true;
    }

    return true;
}

bool QPsdExporterFlutterPlugin::saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const
{
    QFile file(dir.absoluteFilePath(toSnakeCase(baseName) + ".dart"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&file);

    auto importValues = imports.values();
    std::sort(importValues.begin(), importValues.end(), std::less<QString>());
    for (const auto &import : importValues) {
        out << "import '" << import << "';\n";
    }

    auto exportKeys = exports.keys();
    std::sort(exportKeys.begin(), exportKeys.end(), std::less<QString>());
    out << "\n";
    out << "class " << element->type << " extends StatelessWidget {\n";
    out << indentString(1);
    if (exportKeys.size() == 0) {
        out << "const ";
    }
    out << element->type << "({super.key";

    QString properties;
    QTextStream propStream(&properties);
    for (const auto &key : exportKeys) {
        const PropertyInfo &prop = exports.value(key);
        const auto defValue = prop.defaultValue();
        if (defValue.isEmpty()) {
            out << ", required this." << key;
        } else {
            out << ", this." << key << " = " << defValue;
        }
        propStream << indentString(1) << prop.type << " " << key << ";\n";
    }

    out << "});\n";
    out << "\n";

    if (exportKeys.size() > 0) {
        out << properties;
        out << "\n";
    }

    out << indentString(1) << "@override\n";
    out << indentString(1) << "Widget build(BuildContext context) {\n";
    out << indentString(2) << "return ";

    auto child = element->properties.value("child");
    auto typeId = qMetaTypeId<Element>();
    if (child.typeId() == typeId) {
        Element elem = child.value<Element>();
        traverseElement(out, &elem, 2, true);
    }
    out << ";\n";
    out << indentString(1) << "}\n";
    out << "}\n";

    return true;
}

bool QPsdExporterFlutterPlugin::exportTo(const QPsdFolderLayerItem *tree, const QString &to, const QVariantMap &hint) const
{
    dir = { to };
    imageStore = { dir, "assets/images"_L1 };

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
        generateRectMap(child, QPoint(0, 0));
        if (!generateMergeData(child))
            return false;
    }

    ImportData imports;
    imports.insert("package:flutter/material.dart");
    ExportData exports;

    Element sizedBox;
    sizedBox.type = "SizedBox";
    outputRectProp(tree->rect(), &sizedBox);

    Element container;
    container.type = "Stack";

    std::reverse(children.begin(), children.end());
    for (const auto *child : children) {
        if (!traverseTree(child, &container, &imports, &exports, QPsdAbstractLayerItem::ExportHint::None))
            return false;
    }

    sizedBox.properties.insert("child", QVariant::fromValue(container));

    Element window;
    window.type = "MainWindow";
    window.properties.insert("child", QVariant::fromValue(sizedBox));

    return saveTo("MainWindow", &window, imports, exports);
}

QT_END_NAMESPACE

#include "flutter.moc"
