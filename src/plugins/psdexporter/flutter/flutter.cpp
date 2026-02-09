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

    bool exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const override;

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
    mutable QDir dir;
    mutable QPsdImageStore imageStore;
    mutable QString licenseText;

    static QByteArray indentString(int level);
    static QString valueAsText(QVariant value);
    static QString imagePath(const QString &name);
    static QString colorValue(const QColor &color);
    bool saveStaticFile(const QString &name) const;

    bool traverseVariant(QTextStream &out, const QVariant &value, int level, bool skipFirstIndent) const;
    bool traverseElement(QTextStream &out, const Element *element, int level, bool skipFirstIndent) const;
    bool saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const;

    bool outputRectProp(const QRectF &rect, Element *element, bool skipEmpty = false, bool outputPos = false) const;
    bool outputPositioned(const QModelIndex &index, Element *element) const;
    bool outputPositionedTextBounds(const QModelIndex &index, Element *element) const;
    bool outputFolder(const QModelIndex &folderIndex, Element *element, ImportData *imports, ExportData *exports) const;
    bool outputTextElement(const QPsdTextLayerItem::Run run, const QString &text, Element *element) const;
    bool outputText(const QModelIndex &textIndex, Element *element) const;
    bool outputGradient(const QGradient *gradient, const QRectF &rect, Element *element) const;
    bool outputPathProp(const QPainterPath &path, Element *element, ImportData *imports, ExportData *exports) const;
    bool outputShape(const QModelIndex &shapeIndex, Element *element, ImportData *imports, ExportData *exports) const;
    bool outputImage(const QModelIndex &imageIndex, Element *element) const;

    bool traverseTree(const QModelIndex &index, Element *parent, ImportData *imports, ExportData *exports, QPsdExporterTreeItemModel::ExportHint::Type hintOverload) const;
};

Q_DECLARE_METATYPE(QPsdExporterFlutterPlugin::Element)

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

bool QPsdExporterFlutterPlugin::saveStaticFile(const QString &name) const
{
    QFile src(":/flutter/%1"_L1.arg(name));
    return src.copy(dir.absoluteFilePath(name));
}

bool QPsdExporterFlutterPlugin::traverseVariant(QTextStream &out, const QVariant &value, int level, bool skipFirstIndent) const
{
    if (!skipFirstIndent) {
        out << indentString(level);
    }

    switch (value.typeId()) {
    case QMetaType::QString:
        out << value.toString();
        break;
    case QMetaType::Int:
        out << QString::number(value.toInt());
        break;
    case QMetaType::Float:
        out << QString::number(value.toFloat());
        break;
    case QMetaType::Double:
        out << QString::number(value.toDouble());
        break;
    case QMetaType::Bool:
        out << (value.toBool() ? "true" : "false");
        break;
    case QMetaType::QStringList: {
        bool first = true;
        for (const auto &l : value.value<QStringList>()) {
            if (first) {
                first = false;
            } else {
                out << Qt::endl << indentString(level);
            }
            out << l;
        }
        break; }
    case QMetaType::QVariantList:
        out << "[\n";
        for (const auto &item : value.value<QVariantList>()) {
            traverseVariant(out, item, level + 1, false);
            out << ",\n";
        }
        out << indentString(level) << "]";
        break;
    default:
        if (value.typeId() == qMetaTypeId<Element>()) {
            Element elem = value.value<Element>();
            traverseElement(out, &elem, level, true);
            break;
        }
        qFatal() << value.typeName() << "is not supported";
    }

    return true;
}

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
    keys.removeAll("child"_L1);
    keys.removeAll("children"_L1);
    for (const auto &key : keys) {
        const auto value = element->properties.value(key);

        if (value.typeId() == qMetaTypeId<Element>()) {
            elementKeys.append(key);
        } else {
            out << indentString(level) << key << ": ";
            traverseVariant(out, value, level, true);
            out << ",\n";
        }
    }

    for (const auto &key : elementKeys) {
        const auto value = element->properties.value(key);

        Element elem = value.value<Element>();
        out << indentString(level) << key << ": ";

        traverseElement(out, &elem, level, true);
        out << ",\n";
    }

    if (element->properties.contains("child")) {
        out << indentString(level) << "child: ";

        const auto value = element->properties.value("child");
        Element elem = value.value<Element>();

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

    if (!licenseText.isEmpty()) {
        const QStringList lines = licenseText.split('\n');
        for (const QString &line : lines) {
            out << "// " << line << "\n";
        }
        out << "\n";
    }

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

bool QPsdExporterFlutterPlugin::outputPositioned(const QModelIndex &index, Element *element) const
{
    const auto *item = model()->layerItem(index);
    QRect rect = item->rect();
    if (makeCompact) {
        rect = indexRectMap.value(index);
    }
    if (model()->layerHint(index).type == QPsdExporterTreeItemModel::ExportHint::Merge) {
        auto parentIndex = indexMergeMap.key(index);
        while (parentIndex.isValid()) {
            const auto *parent = model()->layerItem(parentIndex);
            rect.translate(-parent->rect().topLeft());
            parentIndex = model()->parent(parentIndex);
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

bool QPsdExporterFlutterPlugin::outputPositionedTextBounds(const QModelIndex &index, Element *element) const
{
    const auto *item = dynamic_cast<const QPsdTextLayerItem *>(model()->layerItem(index));
    QRect rect;
    if (item->textType() == QPsdTextLayerItem::TextType::ParagraphText) {
        rect = item->bounds().toRect();
    } else {
        rect = item->bounds().toRect();
    }
    if (model()->layerHint(index).type == QPsdExporterTreeItemModel::ExportHint::Merge) {
        auto parentIndex = indexMergeMap.key(index);
        while (parentIndex.isValid()) {
            const auto *parent = model()->layerItem(parentIndex);
            rect.translate(-parent->rect().topLeft());
            parentIndex = model()->parent(parentIndex);
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

bool QPsdExporterFlutterPlugin::outputFolder(const QModelIndex &folderIndex, Element *element, ImportData *imports, ExportData *exports) const
{
    const auto *folder = dynamic_cast<const QPsdFolderLayerItem *>(model()->layerItem(folderIndex));
    element->type = "Container";
    if (folder->artboardRect().isValid() && folder->artboardBackground() != Qt::transparent) {
        element->properties.insert("color", colorValue(folder->artboardBackground()));
        if (!outputRectProp(folder->artboardRect(), element))
            return false;
    }

    //TODO don't need Stack widget if chidlen.size() == 1
    Element stackElement;
    stackElement.type = "Stack";
    for (int i = model()->rowCount(folderIndex) - 1; i >= 0; i--) {
        QModelIndex childIndex = model()->index(i, 0, folderIndex);
        if (!traverseTree(childIndex, &stackElement, imports, exports, QPsdExporterTreeItemModel::ExportHint::None))
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
    textStyleElement.properties.insert("fontSize", run.font.pointSizeF() * fontScaleFactor);
    textStyleElement.properties.insert("height", 1.0);
    int weight = run.font.bold() ? 800 : 600;
    textStyleElement.properties.insert("fontVariations", u"[FontVariation.weight(%1)]"_s.arg(weight));
    //TODO italic
    textStyleElement.properties.insert("color", colorValue(run.color));
    
    // Use proper horizontal alignment from PSD
    const Qt::Alignment horizontalAlignment = static_cast<Qt::Alignment>(run.alignment & Qt::AlignHorizontal_Mask);
    switch (horizontalAlignment) {
    case Qt::AlignLeft:
        element->properties.insert("textAlign"_L1, "TextAlign.left"_L1);
        break;
    case Qt::AlignRight:
        element->properties.insert("textAlign"_L1, "TextAlign.right"_L1);
        break;
    case Qt::AlignHCenter:
        element->properties.insert("textAlign"_L1, "TextAlign.center"_L1);
        break;
    case Qt::AlignJustify:
        element->properties.insert("textAlign"_L1, "TextAlign.justify"_L1);
        break;
    default:
        element->properties.insert("textAlign"_L1, "TextAlign.left"_L1);
        break;
    }

    //TODO alignment vertical

    element->properties.insert("style", QVariant::fromValue(textStyleElement));
    element->properties.insert("textScaler"_L1, "TextScaler.linear(1)"_L1);
    return true;
}

bool QPsdExporterFlutterPlugin::outputText(const QModelIndex &textIndex, Element *element) const
{
    const auto *text = dynamic_cast<const QPsdTextLayerItem *>(model()->layerItem(textIndex));
    const auto runs = text->runs();

    Element columnElem;
    columnElem.type = "Column";

    if (runs.size() == 1) {
        auto run = runs.first();
        outputTextElement(run, run.text.trimmed().replace("\n", "\\n"), element);
    } else {
        Element rowElem;
        rowElem.type = "Row";

        for (const auto &run : runs) {
            auto texts = run.text.trimmed().split("\n");
            bool first = true;
            for (const auto &textLine : texts) {
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
                outputTextElement(run, textLine, &textElement);
                rowElem.children.append(textElement);
            }
        }

        if (rowElem.children.size() == 1) {
            columnElem.children.append(rowElem.children.at(0));
        } else {
            columnElem.children.append(rowElem);
        }
        *element = columnElem;
    }

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
    case QGradient::RadialGradient: {
        element->type = "RadialGradient";

        const QRadialGradient *radial = reinterpret_cast<const QRadialGradient*>(gradient);

        element->properties.insert("center", u"Alignment(%1, %2)"_s.arg((radial->center().x() * 2 - rect.width()) / rect.width()).arg((radial->center().y() * 2 - rect.height()) / rect.height()));
        element->properties.insert("radius", radial->radius() / std::min(rect.width(), rect.height()));
        if (radial->focalPoint() != radial->center()) {
            element->properties.insert("focal", u"Alignment(%1, %2)"_s.arg((radial->focalPoint().x() * 2 - rect.width()) / rect.width()).arg((radial->focalPoint().y() * 2 - rect.height()) / rect.height()));
        }
        if (radial->focalRadius() > 0) {
            element->properties.insert("focalRadius", radial->focalRadius() / std::min(rect.width(), rect.height()));
        }

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

bool QPsdExporterFlutterPlugin::outputPathProp(const QPainterPath &path, Element *element, ImportData *imports, ExportData *exports) const
{
    saveStaticFile("qtpsd_path.dart"_L1);
    imports->insert(u"./qtpsd_path.dart"_s);

    QStringList list;
    list.append("Path()"_L1);

    switch (path.fillRule()) {
    case Qt::OddEvenFill:
        list.append("..fillType = PathFillType.evenOdd"_L1);
        break;
    case Qt::WindingFill:
        list.append("..fillType = PathFillType.nonZero"_L1);
        break;
    }

    qreal c1x, c1y, c2x, c2y;
    int control = 1;
    for (int i = 0; i < path.elementCount(); i++) {
        const auto point = path.elementAt(i);
        const auto x = point.x * horizontalScale;
        const auto y = point.y * verticalScale;
        switch (point.type) {
        case QPainterPath::MoveToElement:
            list.append(u"..moveTo(%1, %2)"_s.arg(x).arg(y));
            break;
        case QPainterPath::LineToElement:
            list.append(u"..lineTo(%1, %2)"_s.arg(x).arg(y));
            break;
        case QPainterPath::CurveToElement:
            c1x = x;
            c1y = y;
            control = 1;
            break;
        case QPainterPath::CurveToDataElement:
            switch (control) {
            case 1:
                c2x = x;
                c2y = y;
                control--;
                break;
            case 0:
                list.append(u"..cubicTo(%1, %2, %3, %4, %5, %6)"_s.arg(c1x).arg(c1y).arg(c2x).arg(c2y).arg(x).arg(y));
                break;
            }
            break;
        }
    }
    list.append("..close()"_L1);
    element->properties.insert("path", list);

    return true;
}

bool QPsdExporterFlutterPlugin::outputShape(const QModelIndex &shapeIndex, Element *element, ImportData *imports, ExportData *exports) const
{
    const auto *shape = dynamic_cast<const QPsdShapeLayerItem *>(model()->layerItem(shapeIndex));
    const auto hint = model()->layerHint(shapeIndex);
    const auto path = shape->pathInfo();
    const auto id = toLowerCamelCase(hint.id);

    Element containerElement;
    if (!id.isEmpty()) {
        switch (hint.baseElement) {
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
        default:
            //TODO other NativeComponet are not supported yet
            containerElement.type = "Container"_L1;
            break;
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea:
            containerElement.type = "Ink"_L1;
            break;
        }
    } else {
        containerElement.type = "Container"_L1;        
    }

    Element decorationElement;

    switch (path.type) {
    case QPsdAbstractLayerItem::PathInfo::Rectangle:
    case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
        decorationElement.type = "BoxDecoration"_L1;
        if (shape->pen().style() != Qt::NoPen) {
            qreal dw = std::max(1.0, shape->pen().width() * unitScale);
            // Flutter BoxDecoration Border draws inside the container bounds
            switch (shape->strokeAlignment()) {
            case QPsdShapeLayerItem::StrokeInside:
                // No rect adjustment needed - platform border is already inside
                break;
            case QPsdShapeLayerItem::StrokeCenter:
                outputRectProp(path.rect.adjusted(-dw / 2, -dw / 2, dw / 2, dw / 2), &containerElement);
                break;
            case QPsdShapeLayerItem::StrokeOutside:
                outputRectProp(path.rect.adjusted(-dw, -dw, dw, dw), &containerElement);
                break;
            }
            Element borderElement;
            borderElement.type = "Border.all"_L1;
            borderElement.properties.insert("width"_L1, dw);
            borderElement.properties.insert("color"_L1, colorValue(shape->pen().color()));
            decorationElement.properties.insert("border"_L1, QVariant::fromValue(borderElement));
        }
        if (path.type == QPsdAbstractLayerItem::PathInfo::RoundedRectangle) {
            decorationElement.properties.insert("borderRadius"_L1, u"BorderRadius.circular(%1)"_s.arg(path.radius * unitScale));
        }
        break;
    case QPsdAbstractLayerItem::PathInfo::Path:
        decorationElement.type = "ShapeDecoration"_L1;
        Element borderElement;
        borderElement.type = "QtPsdPathBorder"_L1;
        if (shape->pen().style() != Qt::NoPen) {
            qreal dw = std::max(1.0, shape->pen().width() * unitScale);
            borderElement.properties.insert("color"_L1, colorValue(shape->pen().color()));
            borderElement.properties.insert("width"_L1, dw);
        }
        outputPathProp(path.path, &borderElement, imports, exports);
        decorationElement.properties.insert("shape"_L1, QVariant::fromValue(borderElement));
        break;
    }

    if (shape->gradient()) {
        Element gradientElement;
        outputGradient(shape->gradient(), shape->rect(), &gradientElement);
        decorationElement.properties.insert("gradient"_L1, QVariant::fromValue(gradientElement));
    } else if (shape->brush().gradient()) {
        Element gradientElement;
        outputGradient(shape->brush().gradient(), shape->rect(), &gradientElement);
        decorationElement.properties.insert("gradient"_L1, QVariant::fromValue(gradientElement));
    } else {
        decorationElement.properties.insert("color"_L1, colorValue(shape->brush().color()));
    }

    QVariantList listDropShadow;
    const auto dropShadow = shape->dropShadow();
    if (!dropShadow.isEmpty()) {
        Element effect;
        effect.type = "BoxShadow"_L1;

        QColor color(dropShadow.value("color"_L1).toString());
        color.setAlphaF(dropShadow.value("opacity"_L1).toDouble());
        effect.properties.insert("color"_L1, colorValue(color));
        const auto angle = (180 - dropShadow.value("angle"_L1).toDouble()) * M_PI / 180.0;
        const auto distance = dropShadow.value("distance"_L1).toDouble() * unitScale;
        effect.properties.insert("offset"_L1, u"Offset.fromDirection(%1, %2)"_s.arg(angle).arg(distance));
        effect.properties.insert("spreadRadius"_L1, dropShadow.value("spread"_L1).toDouble() * unitScale);
        effect.properties.insert("blurRadius"_L1, dropShadow.value("size"_L1).toDouble() * unitScale);

        listDropShadow.append(QVariant::fromValue(effect));
    }

    if (!id.isEmpty() && hint.baseElement == QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea) {
        PropertyInfo prop {
            "void Function()?"_L1, "on_%1_Tap"_L1, hint.id
        };
        exports->insert(prop.name(), prop);

        Element inkWell;
        inkWell.type = "InkWell"_L1;
        inkWell.properties.insert("onTap"_L1, prop.name());
        if (path.type == QPsdAbstractLayerItem::PathInfo::RoundedRectangle) {
            inkWell.properties.insert("borderRadius"_L1, u"BorderRadius.circular(%1)"_s.arg(path.radius * unitScale));
        }

        if (indexMergeMap.contains(shapeIndex)) {
            Element stackElement;
            stackElement.type = "Stack"_L1;
            const auto &list = indexMergeMap.values(shapeIndex);
            for (auto it = list.constBegin(); it != list.constEnd(); it++) {
                traverseTree(*it, &stackElement, imports, exports, QPsdExporterTreeItemModel::ExportHint::Embed);
            }

            inkWell.properties.insert("child"_L1, QVariant::fromValue(stackElement));
        }

        containerElement.properties.insert("child"_L1, QVariant::fromValue(inkWell));
        containerElement.properties.insert("decoration"_L1, QVariant::fromValue(decorationElement));

        Element materialElement;
        materialElement.type = "Material"_L1;
        materialElement.properties.insert("type"_L1, "MaterialType.transparency"_L1);
        materialElement.properties.insert("child"_L1, QVariant::fromValue(containerElement));

        if (!listDropShadow.isEmpty()) {
            Element decorationElement;
            decorationElement.type = "BoxDecoration"_L1;

            decorationElement.properties.insert("boxShadow"_L1, listDropShadow);
            element->type = "Container"_L1;
            element->properties.insert("decoration"_L1, QVariant::fromValue(decorationElement));
            element->properties.insert("child"_L1, QVariant::fromValue(materialElement));
        } else {
            *element = materialElement;
        }
    } else {
        if (!listDropShadow.isEmpty()) {
            decorationElement.properties.insert("boxShadow"_L1, listDropShadow);
        }
        containerElement.properties.insert("decoration"_L1, QVariant::fromValue(decorationElement));

        *element = containerElement;
    }

    return true;
}

bool QPsdExporterFlutterPlugin::outputImage(const QModelIndex &imageIndex, Element *element) const
{
    const auto *image = dynamic_cast<const QPsdImageLayerItem *>(model()->layerItem(imageIndex));

    // Check if we need to apply fillOpacity to image content
    const qreal fillOpacity = image->fillOpacity();
    const bool hasEffects = !image->dropShadow().isEmpty() || !image->effects().isEmpty() || image->border();
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
            qimage = image->applyGradient(qimage);
            QByteArray format = linkedFile.type.trimmed();
            name = imageStore.save(imageFileName(linkedFile.name, QString::fromLatin1(format.constData()), linkedFile.uniqueId), qimage, format.constData());
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
        qimage = image->applyGradient(qimage);
        name = imageStore.save(imageFileName(image->name(), "PNG"_L1), qimage, "PNG");
    }

    element->type = "Image.asset";
    element->noNamedParam = u"\"%1\""_s.arg(imagePath(name));
    outputRectProp(image->rect(), element);
    element->properties.insert("fit", "BoxFit.contain");

    const auto *border = image->border();
    if (border && border->isEnable()) {
        Element containerElement;
        containerElement.type = "Container";
        outputRectProp(image->rect(), &containerElement);

        Element decorationElement;
        decorationElement.type = "BoxDecoration";
        Element borderElement;
        borderElement.type = "Border.all";
        borderElement.properties.insert("width", border->size() * unitScale);
        borderElement.properties.insert("color", colorValue(border->color()));
        decorationElement.properties.insert("border", QVariant::fromValue(borderElement));
        containerElement.properties.insert("decoration", QVariant::fromValue(decorationElement));
        containerElement.properties.insert("child", QVariant::fromValue(*element));
        *element = containerElement;
    }

    return true;
}

bool QPsdExporterFlutterPlugin::traverseTree(const QModelIndex &index, Element *parent, ImportData *imports, ExportData *exports, QPsdExporterTreeItemModel::ExportHint::Type hintOverload) const
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
        Element positionedElement;
        bool existsPositioned = false;

        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            outputFolder(index, &element, imports, exports);
            break; }
        case QPsdAbstractLayerItem::Text: {
            outputText(index, &element);
            existsPositioned = outputPositionedTextBounds(index, &positionedElement);
            break; }
        case QPsdAbstractLayerItem::Shape: {
            outputShape(index, &element, imports, exports);
            existsPositioned = outputPositioned(index, &positionedElement);
            break; }
        case QPsdAbstractLayerItem::Image: {
            outputImage(index, &element);
            existsPositioned = outputPositioned(index, &positionedElement);
            break; }
        default:
            break;
        }

        Element *pElement = &element;

        Element materialElement;
        if (!id.isEmpty() && item->type() != QPsdAbstractLayerItem::Shape
            && hint.baseElement == QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea) {
            PropertyInfo prop {
                "void Function()?"_L1, "on_%1_Tap"_L1, hint.id
            };
            exports->insert(prop.name(), prop);
    
            Element inkWell;
            inkWell.type = "InkWell"_L1;
            inkWell.properties.insert("onTap"_L1, prop.name());
            inkWell.properties.insert("child"_L1, QVariant::fromValue(*pElement));
                    
            materialElement.type = "Material";
            materialElement.properties.insert("type"_L1, "MaterialType.transparency"_L1);
            materialElement.properties.insert("child"_L1, QVariant::fromValue(inkWell));
            pElement = &materialElement;
        }

        Element visibilityElement;
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

        // Handle opacity and fillOpacity
        // In Photoshop: opacity affects everything (including effects), fillOpacity affects only content
        const qreal opacity = item->opacity();
        const qreal fillOpacity = item->fillOpacity();
        const bool hasEffects = !item->dropShadow().isEmpty() || !item->effects().isEmpty();
        qreal combinedOpacity = hasEffects ? opacity : (opacity * fillOpacity);

        Element opacityElement;
        if (combinedOpacity < 1.0) {
            opacityElement.type = "Opacity";
            opacityElement.properties.insert("opacity", combinedOpacity);
            opacityElement.properties.insert("child", QVariant::fromValue(*pElement));
            pElement = &opacityElement;
        }

        if (existsPositioned) {
            positionedElement.properties.insert("child"_L1, QVariant::fromValue(*pElement));
            pElement = &positionedElement;
        }

        parent->children.append(*pElement);
        break; }
    case QPsdExporterTreeItemModel::ExportHint::Native: {
        Element element;
        switch (hint.baseElement) {
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
            element.type = "Container"_L1;
            break;
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea: {
            PropertyInfo prop {
                "void Function()?"_L1, "on_%1_Tap"_L1, hint.id
            };
            exports->insert(prop.name(), prop);

            element.type = "GestureDetector"_L1;
            element.properties.insert("onTap"_L1, prop.name());
            element.properties.insert("behavior"_L1, "HitTestBehavior.opaque"_L1);
            break;
        }
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button:
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted: {
            PropertyInfo prop {
                "void Function()?"_L1, "on_%1_Pressed"_L1, hint.id
            };
            exports->insert(prop.name(), prop);
            if (hint.baseElement == QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button) {
                element.type = "ElevatedButton"_L1;
            } else {
                element.type = "FilledButton"_L1;
            }
            element.properties.insert("onPressed"_L1, prop.name());

            if (indexMergeMap.contains(index)) {
                for (auto it = indexMergeMap.constBegin(); it != indexMergeMap.constEnd(); it++) {
                    const auto *i = model()->layerItem(it.value());
                    switch (i->type()) {
                    case QPsdAbstractLayerItem::Text: {
                        Element textElem;
                        outputText(it.value(), &textElem);
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
        if (outputPositioned(index, &positionedElement)) {
            positionedElement.properties.insert("child", QVariant::fromValue(*pElement));
            parent->children.append(positionedElement);
        } else {
            parent->children.append(*pElement);
        }
        break;
    }
    case QPsdExporterTreeItemModel::ExportHint::Component: {
        ImportData i;
        i.insert("package:flutter/material.dart");
        ExportData x;

        Element component;
        Element positionedElement;
        bool existsPositioned = false;

        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            outputFolder(index, &component, &i, &x);
            break; }
        case QPsdAbstractLayerItem::Text: {
            outputText(index, &component);
            existsPositioned = outputPositionedTextBounds(index, &positionedElement);
            break; }
        case QPsdAbstractLayerItem::Shape: {
            outputShape(index, &component, &i, &x);
            existsPositioned = outputPositioned(index, &positionedElement);
            break; }
        case QPsdAbstractLayerItem::Image: {
            outputImage(index, &component);
            existsPositioned = outputPositioned(index, &positionedElement);
            break; }
        }

        Element base;
        bool isMaterial = false;
        switch (hint.baseElement) {
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
            base.type = "Container";
            break;
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea: {
            PropertyInfo prop {
                "void Function()?"_L1, "on_%1_Tap"_L1, {}
            };
            x.insert(prop.name(), prop);
            base.type = "InkWell";
            base.properties.insert("onTap", prop.name());
            isMaterial = true;
            break;
        }
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button: {
            PropertyInfo prop {
                "void Function()?"_L1, "on_%1_Pressed"_L1, {}
            };
            x.insert(prop.name(), prop);
            base.type = "ElevatedButton";
            base.properties.insert("onPressed", prop.name());
            break;
        }
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted: {
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
    case QPsdExporterTreeItemModel::ExportHint::Merge:
    case QPsdExporterTreeItemModel::ExportHint::Skip:
        return true;
    }

    return true;
}

bool QPsdExporterFlutterPlugin::exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const
{
    setModel(model);
    dir = { to };
    imageStore = { dir, "assets/images"_L1 };

    const QSize originalSize = model->size();
    const QSize targetSize = hint.value("resolution", originalSize).toSize();
    horizontalScale = targetSize.width() / qreal(originalSize.width());
    verticalScale = targetSize.height() / qreal(originalSize.height());
    unitScale = std::min(horizontalScale, verticalScale);
    fontScaleFactor = hint.value("fontScaleFactor", 1.0).toReal() * verticalScale;
    makeCompact = hint.value("makeCompact", false).toBool();
    imageScaling = hint.value("imageScaling", false).toBool();
    licenseText = hint.value("licenseText").toString();

    if (!generateMaps()) {
        return false;
    }

    ImportData imports;
    imports.insert("package:flutter/material.dart");
    ExportData exports;

    Element sizedBox;
    sizedBox.type = "SizedBox";
    outputRectProp(QRect { QPoint { 0, 0 }, model->size() }, &sizedBox);

    Element container;
    container.type = "Stack";

    for (int i = model->rowCount(QModelIndex {}) - 1; i >= 0; i--) {
        QModelIndex childIndex = model->index(i, 0, QModelIndex {});
        if (!traverseTree(childIndex, &container, &imports, &exports, QPsdExporterTreeItemModel::ExportHint::None))
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
