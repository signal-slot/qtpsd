// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdGui/qpsdexporterplugin.h>
#include <QtPsdGui/qpsdimagestore.h>

#include <QtCore/QCborMap>
#include <QtCore/QDir>
#include <QtCore/QQueue>

#include <QtGui/QBrush>
#include <QtGui/QPen>

#include <QtPsdCore/QPsdSofiEffect>

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
        return tr("&Flutter");
    }
    ExportType exportType() const override { return QPsdExporterPlugin::Directory; }

    bool exportTo(const QPsdFolderLayerItem *tree, const QString &to, const QVariantMap &hint) const override;

    struct Element {
        QString type;
        QString id;
        QVariantHash properties;
        QList<Element> children;
        QVariant noNamedParam = QVariant(QMetaType(QMetaType::Void));
    };

private:
    struct PropertyInfo {
        QString type;
        QString format;
        QString objname;
        QString defValue = {};

        QString name() const {
            return toLowerCamelCase(format.arg(objname));
        }

        QString defaultValue() const {
            const QHash<QString, QString> defaultValueHash {
                { "bool"_L1, "false"_L1 },
            };

            if (defValue.isEmpty()) {
                return defaultValueHash.value(type);
            } else {
                return defValue;
            }
        }
    };

    using ImportData = QSet<QString>;
    using ExportData = QHash<QString, PropertyInfo>;

    mutable qreal horizontalScale = 1.0;
    mutable qreal verticalScale = 1.0;
    mutable qreal unitScale = 1.0;
    mutable qreal fontScaleFactor = 1.0;
    mutable bool makeCompact = false;
    mutable bool imageScaling = false;
    mutable QHash<const QPsdAbstractLayerItem *, QRect> rectMap;
    mutable QMultiMap<const QPsdAbstractLayerItem *, const QPsdAbstractLayerItem *> mergeMap;
    mutable QDir dir;
    mutable QPsdImageStore imageStore;

    static QByteArray indentString(int level);
    static QString valueAsText(QVariant value);
    static QString imagePath(const QString &name);
    static QString colorValue(const QColor &color);

    // Value Formatting Methods
    static QByteArray indentString(int level);
    static QString valueAsText(QVariant value);
    static QString imagePath(const QString &name);
    static QString colorValue(const QColor &color);

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
};

Q_DECLARE_METATYPE(QPsdExporterFlutterPlugin::Element)

// Value Formatting Methods
QByteArray QPsdExporterFlutterPlugin::indentString(int level)
{
    return QByteArray(level * 2, ' ');
}

QString QPsdExporterFlutterPlugin::valueAsText(QVariant value)
{
    QString valueAsText;
    switch (value.typeId()) {
    case QMetaType::QString:
        valueAsText = value.toString();
        break;
    case QMetaType::Int:
        valueAsText = QString::number(value.toInt());
        break;
    case QMetaType::Float:
        valueAsText = QString::number(value.toFloat());
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
    return valueAsText;
}

QString QPsdExporterFlutterPlugin::imagePath(const QString &name)
{
    return "assets/images/" + name;
}

QString QPsdExporterFlutterPlugin::colorValue(const QColor &color)
{
    return u"Color.fromARGB(%1, %2, %3, %4)"_s.arg(color.alpha()).arg(color.red()).arg(color.green()).arg(color.blue());
}

// Utility Methods
void QPsdExporterFlutterPlugin::findChildren(const QPsdAbstractLayerItem *item, QRect *rect) const
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

bool QPsdExporterFlutterPlugin::outputRectProp(const QRectF &rect, Element *element, bool skipEmpty, bool outputPos) const
{
    if (outputPos) {
        element->properties.insert("left", rect.x() * horizontalScale);
        element->properties.insert("top", rect.y() * verticalScale);
    }
    if (rect.isEmpty() && skipEmpty)
        return true;
    element->properties.insert("width", rect.width() * horizontalScale);
    element->properties.insert("height", rect.height() * verticalScale);
    return true;
}

bool QPsdExporterFlutterPlugin::outputPath(const QPainterPath &path, Element *element) const
{
    switch (path.fillRule()) {
    case Qt::OddEvenFill:
        element->properties.insert("fillRule", "ShapePath.OddEvenFill");
        break;
    case Qt::WindingFill:
        element->properties.insert("fillRule", "ShapePath.WindingFill");
        break;
    }

    Element pathCubic;
    pathCubic.type = "PathCubic";
    int control = 1;
    for (int i = 0; i < path.elementCount(); i++) {
        const auto point = path.elementAt(i);
        const auto x = point.x * horizontalScale;
        const auto y = point.y * verticalScale;
        switch (point.type) {
        case QPainterPath::MoveToElement: {
            Element pathMove;
            pathMove.type = "PathMove";
            pathMove.properties.insert("x", x);
            pathMove.properties.insert("y", y);
            element->children.append(pathMove);
            break; }
        case QPainterPath::LineToElement: {
            Element pathLine;
            pathLine.type = "PathLine";
            pathLine.properties.insert("x", x);
            pathLine.properties.insert("y", y);
            element->children.append(pathLine);
            break; }
        case QPainterPath::CurveToElement: {
            pathCubic.properties.insert("control1X", x);
            pathCubic.properties.insert("control1Y", y);
            control = 1;
            break; }
        case QPainterPath::CurveToDataElement:
            switch (control) {
            case 1:
                pathCubic.properties.insert("control2X", x);
                pathCubic.properties.insert("control2Y", y);
                control--;
                break;
            case 0:
                pathCubic.properties.insert("x", x);
                pathCubic.properties.insert("y", y);
                element->children.append(pathCubic);
                break;
            }
            break;
        }
    }
    return true;
}

bool QPsdExporterFlutterPlugin::outputPositioned(const QPsdAbstractLayerItem *item, Element *element) const
{
    QRect rect = item->rect();
    if (makeCompact) {
        rect = rectMap.value(item);
    }
    if (item->exportHint().type == QPsdAbstractLayerItem::ExportHint::Merge) {
        auto parent = mergeMap.key(item);
        while (parent) {
            rect.translate(-parent->rect().topLeft());
            parent = parent->parent();
        }
    }
    if (rect.isEmpty()) {
        return false;
    } else {
        element->type = "Positioned";
        outputRectProp(rect, element, false, true);
    }

    //TODO QPsdSofiEffect support

    //TODO vectorMask support

    //TODO dropShadow support

    return true;
}

bool QPsdExporterFlutterPlugin::outputPositionedTextBounds(const QPsdTextLayerItem *item, Element *element) const
{
    QRect rect = item->bounds().toRect();
    if (item->exportHint().type == QPsdAbstractLayerItem::ExportHint::Merge) {
        auto parent = mergeMap.key(item);
        while (parent) {
            rect.translate(-parent->rect().topLeft());
            parent = parent->parent();
        }
    }
    if (rect.isEmpty()) {
        return false;
    } else {
        element->type = "Positioned";
        outputRectProp(rect, element, false, true);
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

bool QPsdExporterFlutterPlugin::outputTextElement(const QPsdTextLayerItem::Run run, const QString &text, Element *element) const
{
    element->type = "Text";
    element->noNamedParam = u"\"%1\""_s.arg(text.trimmed());

    Element textStyleElement;
    textStyleElement.type = "TextStyle";
    textStyleElement.properties.insert("fontFamily",  u"\"%1\""_s.arg(run.font.family()));
    textStyleElement.properties.insert("fontSize", run.font.pointSizeF() * fontScaleFactor / 1.5);
    textStyleElement.properties.insert("height", 1.0);
    int weight = run.font.bold() ? 800 : 600;
    textStyleElement.properties.insert("fontVariations", u"[FontVariation.weight(%1)]"_s.arg(weight));
    //TODO italic
    textStyleElement.properties.insert("color", colorValue(run.color));

    //TODO alignment horizontal / vertical

    element->properties.insert("style", QVariant::fromValue(textStyleElement));
    return true;
}

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

bool QPsdExporterFlutterPlugin::outputGradient(const QGradient *gradient, const QRectF &rect, Element *element) const
{
    switch (gradient->type()) {
    case QGradient::LinearGradient: {
        element->type = "LinearGradient";

        const QLinearGradient *linear = reinterpret_cast<const QLinearGradient*>(gradient);

        element->properties.insert("begin", u"Alignment(%1, %2)"_s.arg((linear->start().x() * 2 - rect.width()) / rect.width()).arg((linear->start().y() * 2 - rect.height()) / rect.height()));
        element->properties.insert("end", u"Alignment(%1, %2)"_s.arg((linear->finalStop().x() * 2 - rect.width()) / rect.width()).arg((linear->finalStop().y() * 2 - rect.height()) / rect.height()));

        break;
    }
    default:
        qFatal() << "Unsupported gradient type"_L1 << gradient->type();
    }

    QVariantList stops;
    QVariantList colors;
    for (const auto &stop : gradient->stops()) {
        stops.append(stop.first);
        colors.append(colorValue(stop.second));
    }
    element->properties.insert("stops", stops);
    element->properties.insert("colors", colors);

    return true;
}

bool QPsdExporterFlutterPlugin::outputShape(const QPsdShapeLayerItem *shape, Element *element, ImportData *imports, ExportData *exports) const
{
    const auto hint = shape->exportHint();
    const auto path = shape->pathInfo();
    switch (path.type) {
    case QPsdAbstractLayerItem::PathInfo::Rectangle:
    case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
        Element containerElement;
        switch (hint.baseElement) {
        case QPsdAbstractLayerItem::ExportHint::NativeComponent::Container:
        default:
            //TODO other NativeComponet are not supported yet
            containerElement.type = "Container";
            break;
        case QPsdAbstractLayerItem::ExportHint::NativeComponent::TouchArea:
            containerElement.type = "Ink";
            break;
        }

        Element decorationElement;
        decorationElement.type = "BoxDecoration";
        if (shape->pen().style() != Qt::NoPen) {
            qreal dw = std::max(1.0, shape->pen().width() * unitScale);
            outputRectProp(path.rect.adjusted(-dw, -dw, dw, dw), &containerElement);
            Element borderElement;
            borderElement.type = "Border.all";
            borderElement.properties.insert("width", dw);
            borderElement.properties.insert("color", colorValue(shape->pen().color()));
            decorationElement.properties.insert("border", QVariant::fromValue(borderElement));
        }
        if (path.type == QPsdAbstractLayerItem::PathInfo::RoundedRectangle) {
            decorationElement.properties.insert("borderRadius", u"BorderRadius.circular(%1)"_s.arg(path.radius * unitScale));
        }
        if (shape->gradient()) {
            Element gradientElement;
            outputGradient(shape->gradient(), shape->rect(), &gradientElement);
            decorationElement.properties.insert("gradient", QVariant::fromValue(gradientElement));
        } else {
            decorationElement.properties.insert("color", colorValue(shape->brush().color()));
        }

        containerElement.properties.insert("decoration", QVariant::fromValue(decorationElement));

        if (hint.baseElement == QPsdAbstractLayerItem::ExportHint::NativeComponent::TouchArea) {
            PropertyInfo prop {
                "void Function()?"_L1, "on_%1_Tap"_L1, hint.id
            };
            exports->insert(prop.name(), prop);

            Element inkWell;
            inkWell.type = "InkWell";
            inkWell.properties.insert("onTap", prop.name());
            if (path.type == QPsdAbstractLayerItem::PathInfo::RoundedRectangle) {
                inkWell.properties.insert("borderRadius", u"BorderRadius.circular(%1)"_s.arg(path.radius * unitScale));
            }

            if (mergeMap.contains(shape)) {
                Element stackElement;
                stackElement.type = "Stack";
                for (const auto *i : mergeMap.values(shape)) {
                    traverseTree(i, &stackElement, imports, exports, QPsdAbstractLayerItem::ExportHint::Embed);
                }

                inkWell.properties.insert("child", QVariant::fromValue(stackElement));
            }

            containerElement.properties.insert("child", QVariant::fromValue(inkWell));

            element->type = "Material";
            element->properties.insert("type", "MaterialType.transparency");
            element->properties.insert("child", QVariant::fromValue(containerElement));
        } else {
            *element = containerElement;
        }
        break;
    }
    case QPsdAbstractLayerItem::PathInfo::Path: {
        //TODO Path support
        element->type = "Container";
        break; }
    }
    return true;
}

bool QPsdExporterFlutterPlugin::outputImage(const QPsdImageLayerItem *image, Element *element) const
{
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

    element->type = "Image.asset";
    element->noNamedParam = u"\"%1\""_s.arg(imagePath(name));
    outputRectProp(image->rect(), element);
    element->properties.insert("fit", "BoxFit.contain");

    return true;
}

void QPsdExporterFlutterPlugin::findChildren(const QPsdAbstractLayerItem *item, QRect *rect) const
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
}

void QPsdExporterFlutterPlugin::generateRectMap(const QPsdAbstractLayerItem *item, const QPoint &topLeft) const
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
}

bool QPsdExporterFlutterPlugin::generateMergeData(const QPsdAbstractLayerItem *item) const
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
