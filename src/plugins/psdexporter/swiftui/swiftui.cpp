// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/qpsdexporterplugin.h>
#include <QtPsdExporter/qpsdimagestore.h>

#include <QtCore/QCborMap>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include <QtGui/QBrush>
#include <QtGui/QPen>

#include <QtPsdGui/QPsdBorder>

QT_BEGIN_NAMESPACE

class QPsdExporterSwiftUIPlugin : public QPsdExporterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdExporterFactoryInterface" FILE "swiftui.json")
public:
    int priority() const override { return 50; }
    QIcon icon() const override {
        return QIcon(":/swiftui/swiftui.png");
    }
    QString name() const override {
        return tr("&SwiftUI");
    }
    ExportType exportType() const override { return QPsdExporterPlugin::Directory; }

    bool exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const override;

private:
    struct Element {
        QString type;
        QString id;
        QVariantHash properties;
        QList<Element> children;
        QStringList modifiers;
    };

    mutable qreal horizontalScale = 1.0;
    mutable qreal verticalScale = 1.0;
    mutable qreal unitScale = 1.0;
    mutable qreal fontScaleFactor = 1.0;
    mutable bool makeCompact = false;
    mutable bool imageScaling = false;
    mutable QDir dir;
    mutable QDir assetsDir;
    mutable QString licenseText;

    using ImportData = QSet<QString>;
    using ExportData = QSet<QString>;

    static QByteArray indentString(int level);
    static QString colorValue(const QColor &color);
    static QString sanitizeImageName(const QString &name, const QByteArray &uniqueId = {});

    bool saveImageAsset(const QString &name, const QImage &image) const;

    bool outputRect(const QRectF &rect, Element *element, bool skipEmpty = false) const;
    bool outputBase(const QModelIndex &index, Element *element, ImportData *imports, QRect rectBounds = {}) const;
    bool outputFolder(const QModelIndex &folderIndex, Element *element, ImportData *imports, ExportData *exports) const;
    bool outputText(const QModelIndex &textIndex, Element *element, ImportData *imports) const;
    bool outputShape(const QModelIndex &shapeIndex, Element *element, ImportData *imports) const;
    bool outputImage(const QModelIndex &imageIndex, Element *element, ImportData *imports) const;
    bool outputPath(const QPainterPath &path, Element *element) const;
    bool outputGradient(const QGradient *gradient, const QRectF &rect, Element *element) const;

    bool traverseTree(const QModelIndex &index, Element *parent, ImportData *imports, ExportData *exports, QPsdExporterTreeItemModel::ExportHint::Type hintOverload) const;

    bool traverseElement(QTextStream &out, const Element *element, int level) const;
    bool saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const;
};

QByteArray QPsdExporterSwiftUIPlugin::indentString(int level)
{
    return QByteArray(level * 4, ' ');
}

QString QPsdExporterSwiftUIPlugin::colorValue(const QColor &color)
{
    if (color.alpha() == 255) {
        return u"Color(red: %1, green: %2, blue: %3)"_s
            .arg(color.redF(), 0, 'f', 3)
            .arg(color.greenF(), 0, 'f', 3)
            .arg(color.blueF(), 0, 'f', 3);
    } else {
        return u"Color(red: %1, green: %2, blue: %3, opacity: %4)"_s
            .arg(color.redF(), 0, 'f', 3)
            .arg(color.greenF(), 0, 'f', 3)
            .arg(color.blueF(), 0, 'f', 3)
            .arg(color.alphaF(), 0, 'f', 3);
    }
}

QString QPsdExporterSwiftUIPlugin::sanitizeImageName(const QString &name, const QByteArray &uniqueId)
{
    QString sanitized = name;
    sanitized.replace(QRegularExpression("[^a-zA-Z0-9_]"), "_");
    if (!sanitized.isEmpty() && sanitized[0].isDigit()) {
        sanitized.prepend('_');
    }
    if (sanitized != name) {
        QByteArray hashInput = name.toUtf8();
        if (!uniqueId.isEmpty())
            hashInput += uniqueId;
        sanitized = QString::fromLatin1(QCryptographicHash::hash(hashInput, QCryptographicHash::Sha256).toHex().left(16));
    }
    return sanitized;
}

bool QPsdExporterSwiftUIPlugin::saveImageAsset(const QString &name, const QImage &image) const
{
    QString sanitizedName = sanitizeImageName(name);
    QString imagesetPath = assetsDir.absoluteFilePath(u"%1.imageset"_s.arg(sanitizedName));
    QDir imagesetDir(imagesetPath);
    if (!imagesetDir.exists()) {
        imagesetDir.mkpath(".");
    }

    // Save 1x image
    QString imagePath = imagesetDir.absoluteFilePath(u"%1.png"_s.arg(sanitizedName));
    if (!image.save(imagePath, "PNG")) {
        return false;
    }

    // Create Contents.json
    QJsonObject contents;
    QJsonArray images;

    QJsonObject image1x;
    image1x["filename"] = u"%1.png"_s.arg(sanitizedName);
    image1x["idiom"] = "universal";
    image1x["scale"] = "1x";
    images.append(image1x);

    QJsonObject image2x;
    image2x["idiom"] = "universal";
    image2x["scale"] = "2x";
    images.append(image2x);

    QJsonObject image3x;
    image3x["idiom"] = "universal";
    image3x["scale"] = "3x";
    images.append(image3x);

    contents["images"] = images;

    QJsonObject info;
    info["author"] = "xcode";
    info["version"] = 1;
    contents["info"] = info;

    QFile contentsFile(imagesetDir.absoluteFilePath("Contents.json"));
    if (!contentsFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    contentsFile.write(QJsonDocument(contents).toJson());
    contentsFile.close();

    return true;
}

bool QPsdExporterSwiftUIPlugin::outputRect(const QRectF &rect, Element *element, bool skipEmpty) const
{
    element->properties.insert("x", rect.x() * horizontalScale);
    element->properties.insert("y", rect.y() * verticalScale);
    if (rect.isEmpty() && skipEmpty)
        return true;
    element->properties.insert("width", rect.width() * horizontalScale);
    element->properties.insert("height", rect.height() * verticalScale);
    return true;
}

bool QPsdExporterSwiftUIPlugin::outputBase(const QModelIndex &index, Element *element, ImportData *imports, QRect rectBounds) const
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

    outputRect(rect, element, true);

    // Handle opacity and fillOpacity
    const qreal opacity = item->opacity();
    const qreal fillOpacity = item->fillOpacity();
    const bool hasEffects = !item->dropShadow().isEmpty() || !item->effects().isEmpty();

    if (hasEffects) {
        if (opacity < 1.0) {
            element->modifiers.append(u".opacity(%1)"_s.arg(opacity, 0, 'f', 2));
        }
    } else {
        const qreal combinedOpacity = opacity * fillOpacity;
        if (combinedOpacity < 1.0) {
            element->modifiers.append(u".opacity(%1)"_s.arg(combinedOpacity, 0, 'f', 2));
        }
    }

    return true;
}

bool QPsdExporterSwiftUIPlugin::outputFolder(const QModelIndex &folderIndex, Element *element, ImportData *imports, ExportData *exports) const
{
    const auto *folder = dynamic_cast<const QPsdFolderLayerItem *>(model()->layerItem(folderIndex));
    element->type = "Group";

    if (!outputBase(folderIndex, element, imports))
        return false;

    if (folder->artboardRect().isValid() && folder->artboardBackground() != Qt::transparent) {
        Element background;
        background.type = "Rectangle";
        background.modifiers.append(u".fill(%1)"_s.arg(colorValue(folder->artboardBackground())));
        outputRect(folder->artboardRect(), &background);
        // Calculate center position for SwiftUI
        qreal centerX = (folder->artboardRect().x() + folder->artboardRect().width() / 2.0) * horizontalScale;
        qreal centerY = (folder->artboardRect().y() + folder->artboardRect().height() / 2.0) * verticalScale;
        background.modifiers.append(u".frame(width: %1, height: %2)"_s
            .arg(folder->artboardRect().width() * horizontalScale, 0, 'f', 1)
            .arg(folder->artboardRect().height() * verticalScale, 0, 'f', 1));
        background.modifiers.append(u".position(x: %1, y: %2)"_s.arg(centerX, 0, 'f', 1).arg(centerY, 0, 'f', 1));
        element->children.append(background);
    }

    for (int i = model()->rowCount(folderIndex) - 1; i >= 0; i--) {
        QModelIndex childIndex = model()->index(i, 0, folderIndex);
        if (!traverseTree(childIndex, element, imports, exports, QPsdExporterTreeItemModel::ExportHint::None))
            return false;
    }

    return true;
}

bool QPsdExporterSwiftUIPlugin::outputText(const QModelIndex &textIndex, Element *element, ImportData *imports) const
{
    Q_UNUSED(imports);
    const auto *text = dynamic_cast<const QPsdTextLayerItem *>(model()->layerItem(textIndex));
    const auto runs = text->runs();

    QRect rect;
    if (text->textType() == QPsdTextLayerItem::TextType::ParagraphText) {
        rect = text->bounds().toRect();
    } else {
        rect = text->bounds().toRect();
    }

    if (runs.size() == 1) {
        const auto run = runs.first();
        element->type = "Text";
        element->properties.insert("content", run.text.trimmed().replace("\n", "\\n").replace("\"", "\\\""));

        // Font modifier
        element->modifiers.append(u".font(.custom(\"%1\", size: %2))"_s
            .arg(run.font.family())
            .arg(qRound(run.font.pointSizeF() * fontScaleFactor)));

        // Color modifier
        element->modifiers.append(u".foregroundStyle(%1)"_s.arg(colorValue(run.color)));

        // Alignment
        const Qt::Alignment horizontalAlignment = static_cast<Qt::Alignment>(run.alignment & Qt::AlignHorizontal_Mask);
        switch (horizontalAlignment) {
        case Qt::AlignLeft:
            element->modifiers.append(".multilineTextAlignment(.leading)");
            break;
        case Qt::AlignRight:
            element->modifiers.append(".multilineTextAlignment(.trailing)");
            break;
        case Qt::AlignHCenter:
            element->modifiers.append(".multilineTextAlignment(.center)");
            break;
        default:
            break;
        }

        if (run.font.bold()) {
            element->modifiers.append(".fontWeight(.bold)");
        }
        if (run.font.italic()) {
            element->modifiers.append(".italic()");
        }
    } else {
        // Multiple runs - use VStack with HStack
        element->type = "VStack";
        element->properties.insert("alignment", ".leading");
        element->properties.insert("spacing", 0);

        Element currentHStack;
        currentHStack.type = "HStack";
        currentHStack.properties.insert("spacing", 0);

        for (const auto &run : runs) {
            const auto texts = run.text.trimmed().split("\n");
            bool first = true;
            for (const auto &textLine : texts) {
                if (first) {
                    first = false;
                } else {
                    if (!currentHStack.children.isEmpty()) {
                        element->children.append(currentHStack);
                    }
                    currentHStack.children.clear();
                }

                Element textElement;
                textElement.type = "Text";
                textElement.properties.insert("content", QString(textLine).replace("\"", "\\\""));
                textElement.modifiers.append(u".font(.custom(\"%1\", size: %2))"_s
                    .arg(run.font.family())
                    .arg(qRound(run.font.pointSizeF() * fontScaleFactor)));
                textElement.modifiers.append(u".foregroundStyle(%1)"_s.arg(colorValue(run.color)));

                if (run.font.bold()) {
                    textElement.modifiers.append(".fontWeight(.bold)");
                }
                if (run.font.italic()) {
                    textElement.modifiers.append(".italic()");
                }

                currentHStack.children.append(textElement);
            }
        }
        if (!currentHStack.children.isEmpty()) {
            element->children.append(currentHStack);
        }
    }

    // Position using center coordinates
    qreal centerX = (rect.x() + rect.width() / 2.0) * horizontalScale;
    qreal centerY = (rect.y() + rect.height() / 2.0) * verticalScale;
    element->modifiers.append(u".frame(width: %1, height: %2)"_s
        .arg(rect.width() * horizontalScale, 0, 'f', 1)
        .arg(rect.height() * verticalScale, 0, 'f', 1));
    element->modifiers.append(u".position(x: %1, y: %2)"_s.arg(centerX, 0, 'f', 1).arg(centerY, 0, 'f', 1));

    if (!outputBase(textIndex, element, imports, rect))
        return false;

    return true;
}

bool QPsdExporterSwiftUIPlugin::outputGradient(const QGradient *gradient, const QRectF &rect, Element *element) const
{
    Q_UNUSED(rect);
    switch (gradient->type()) {
    case QGradient::LinearGradient: {
        const QLinearGradient *linear = static_cast<const QLinearGradient*>(gradient);

        QStringList colors;
        for (const auto &stop : linear->stops()) {
            colors.append(colorValue(stop.second));
        }

        // Calculate gradient direction
        QPointF start = linear->start();
        QPointF end = linear->finalStop();

        QString startPoint, endPoint;
        // Normalize to unit points
        if (qFuzzyCompare(start.y(), end.y())) {
            // Horizontal gradient
            if (start.x() < end.x()) {
                startPoint = ".leading";
                endPoint = ".trailing";
            } else {
                startPoint = ".trailing";
                endPoint = ".leading";
            }
        } else if (qFuzzyCompare(start.x(), end.x())) {
            // Vertical gradient
            if (start.y() < end.y()) {
                startPoint = ".top";
                endPoint = ".bottom";
            } else {
                startPoint = ".bottom";
                endPoint = ".top";
            }
        } else {
            // Diagonal - use UnitPoint
            startPoint = u"UnitPoint(x: %1, y: %2)"_s
                .arg(start.x() / rect.width(), 0, 'f', 2)
                .arg(start.y() / rect.height(), 0, 'f', 2);
            endPoint = u"UnitPoint(x: %1, y: %2)"_s
                .arg(end.x() / rect.width(), 0, 'f', 2)
                .arg(end.y() / rect.height(), 0, 'f', 2);
        }

        element->modifiers.append(u".fill(LinearGradient(colors: [%1], startPoint: %2, endPoint: %3))"_s
            .arg(colors.join(", "), startPoint, endPoint));
        break;
    }
    case QGradient::RadialGradient: {
        const QRadialGradient *radial = static_cast<const QRadialGradient*>(gradient);

        QStringList colors;
        for (const auto &stop : radial->stops()) {
            colors.append(colorValue(stop.second));
        }

        element->modifiers.append(u".fill(RadialGradient(colors: [%1], center: UnitPoint(x: %2, y: %3), startRadius: %4, endRadius: %5))"_s
            .arg(colors.join(", "))
            .arg(radial->center().x() / rect.width(), 0, 'f', 2)
            .arg(radial->center().y() / rect.height(), 0, 'f', 2)
            .arg(radial->focalRadius() * unitScale, 0, 'f', 1)
            .arg(radial->radius() * unitScale, 0, 'f', 1));
        break;
    }
    default:
        qWarning() << "Unsupported gradient type" << gradient->type();
        return false;
    }
    return true;
}

bool QPsdExporterSwiftUIPlugin::outputPath(const QPainterPath &path, Element *element) const
{
    QStringList pathCommands;

    qreal c1x, c1y, c2x, c2y;
    int control = 1;

    for (int i = 0; i < path.elementCount(); i++) {
        const auto point = path.elementAt(i);
        const auto x = point.x * horizontalScale;
        const auto y = point.y * verticalScale;

        switch (point.type) {
        case QPainterPath::MoveToElement:
            pathCommands.append(u"path.move(to: CGPoint(x: %1, y: %2))"_s
                .arg(x, 0, 'f', 1).arg(y, 0, 'f', 1));
            break;
        case QPainterPath::LineToElement:
            pathCommands.append(u"path.addLine(to: CGPoint(x: %1, y: %2))"_s
                .arg(x, 0, 'f', 1).arg(y, 0, 'f', 1));
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
                pathCommands.append(u"path.addCurve(to: CGPoint(x: %1, y: %2), control1: CGPoint(x: %3, y: %4), control2: CGPoint(x: %5, y: %6))"_s
                    .arg(x, 0, 'f', 1).arg(y, 0, 'f', 1)
                    .arg(c1x, 0, 'f', 1).arg(c1y, 0, 'f', 1)
                    .arg(c2x, 0, 'f', 1).arg(c2y, 0, 'f', 1));
                break;
            }
            break;
        }
    }
    pathCommands.append("path.closeSubpath()");

    element->properties.insert("pathCommands", pathCommands);
    return true;
}

bool QPsdExporterSwiftUIPlugin::outputShape(const QModelIndex &shapeIndex, Element *element, ImportData *imports) const
{
    Q_UNUSED(imports);
    const auto *shape = dynamic_cast<const QPsdShapeLayerItem *>(model()->layerItem(shapeIndex));
    const auto pathInfo = shape->pathInfo();

    QRect rect = shape->rect();
    qreal centerX = (rect.x() + rect.width() / 2.0) * horizontalScale;
    qreal centerY = (rect.y() + rect.height() / 2.0) * verticalScale;

    switch (pathInfo.type) {
    case QPsdAbstractLayerItem::PathInfo::Rectangle: {
        element->type = "Rectangle";

        // Fill
        const QGradient *g = shape->gradient();
        if (!g && shape->brush().gradient()) {
            g = shape->brush().gradient();
        }

        if (g) {
            outputGradient(g, rect, element);
        } else if (shape->brush() != Qt::NoBrush) {
            element->modifiers.append(u".fill(%1)"_s.arg(colorValue(shape->brush().color())));
        }

        // Stroke
        if (shape->pen().style() != Qt::NoPen) {
            qreal strokeWidth = std::max(1.0, shape->pen().width() * unitScale);
            element->modifiers.append(u".stroke(%1, lineWidth: %2)"_s
                .arg(colorValue(shape->pen().color()))
                .arg(strokeWidth, 0, 'f', 1));
        }

        element->modifiers.append(u".frame(width: %1, height: %2)"_s
            .arg(pathInfo.rect.width() * horizontalScale, 0, 'f', 1)
            .arg(pathInfo.rect.height() * verticalScale, 0, 'f', 1));
        element->modifiers.append(u".position(x: %1, y: %2)"_s.arg(centerX, 0, 'f', 1).arg(centerY, 0, 'f', 1));
        break;
    }
    case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
        element->type = "RoundedRectangle";
        element->properties.insert("cornerRadius", pathInfo.radius * unitScale);

        // Fill
        const QGradient *g = shape->gradient();
        if (!g && shape->brush().gradient()) {
            g = shape->brush().gradient();
        }

        if (g) {
            outputGradient(g, rect, element);
        } else if (shape->brush() != Qt::NoBrush) {
            element->modifiers.append(u".fill(%1)"_s.arg(colorValue(shape->brush().color())));
        }

        // Stroke
        if (shape->pen().style() != Qt::NoPen) {
            qreal strokeWidth = std::max(1.0, shape->pen().width() * unitScale);
            element->modifiers.append(u".stroke(%1, lineWidth: %2)"_s
                .arg(colorValue(shape->pen().color()))
                .arg(strokeWidth, 0, 'f', 1));
        }

        element->modifiers.append(u".frame(width: %1, height: %2)"_s
            .arg(pathInfo.rect.width() * horizontalScale, 0, 'f', 1)
            .arg(pathInfo.rect.height() * verticalScale, 0, 'f', 1));
        element->modifiers.append(u".position(x: %1, y: %2)"_s.arg(centerX, 0, 'f', 1).arg(centerY, 0, 'f', 1));
        break;
    }
    case QPsdAbstractLayerItem::PathInfo::Path: {
        element->type = "Path";

        if (!outputPath(pathInfo.path, element))
            return false;

        // Fill
        const QGradient *g = shape->gradient();
        if (!g && shape->brush().gradient()) {
            g = shape->brush().gradient();
        }

        if (g) {
            outputGradient(g, rect, element);
        } else if (shape->brush() != Qt::NoBrush) {
            element->modifiers.append(u".fill(%1)"_s.arg(colorValue(shape->brush().color())));
        }

        // Stroke
        if (shape->pen().style() != Qt::NoPen) {
            qreal strokeWidth = std::max(1.0, shape->pen().width() * unitScale);
            element->modifiers.append(u".stroke(%1, lineWidth: %2)"_s
                .arg(colorValue(shape->pen().color()))
                .arg(strokeWidth, 0, 'f', 1));
        }

        element->modifiers.append(u".position(x: %1, y: %2)"_s.arg(centerX, 0, 'f', 1).arg(centerY, 0, 'f', 1));
        break;
    }
    default:
        break;
    }

    if (!outputBase(shapeIndex, element, imports))
        return false;

    return true;
}

bool QPsdExporterSwiftUIPlugin::outputImage(const QModelIndex &imageIndex, Element *element, ImportData *imports) const
{
    Q_UNUSED(imports);
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
            name = sanitizeImageName(QFileInfo(linkedFile.name).baseName(), linkedFile.uniqueId);
            done = saveImageAsset(name, qimage);
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
        name = sanitizeImageName(image->name());
        saveImageAsset(name, qimage);
    }

    element->type = "Image";
    element->properties.insert("imageName", name);

    QRect rect = image->rect();
    qreal centerX = (rect.x() + rect.width() / 2.0) * horizontalScale;
    qreal centerY = (rect.y() + rect.height() / 2.0) * verticalScale;

    element->modifiers.append(".resizable()");
    element->modifiers.append(".aspectRatio(contentMode: .fit)");
    element->modifiers.append(u".frame(width: %1, height: %2)"_s
        .arg(rect.width() * horizontalScale, 0, 'f', 1)
        .arg(rect.height() * verticalScale, 0, 'f', 1));
    element->modifiers.append(u".position(x: %1, y: %2)"_s.arg(centerX, 0, 'f', 1).arg(centerY, 0, 'f', 1));

    const auto *border = image->border();
    if (border && border->isEnable()) {
        qreal strokeWidth = border->size() * unitScale;
        element->modifiers.append(u".overlay(Rectangle().stroke(%1, lineWidth: %2))"_s
            .arg(colorValue(border->color()))
            .arg(strokeWidth, 0, 'f', 1));
    }

    if (!outputBase(imageIndex, element, imports))
        return false;

    return true;
}

bool QPsdExporterSwiftUIPlugin::traverseTree(const QModelIndex &index, Element *parent, ImportData *imports, ExportData *exports, QPsdExporterTreeItemModel::ExportHint::Type hintOverload) const
{
    const QPsdAbstractLayerItem *item = model()->layerItem(index);
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
            element.modifiers.append(".hidden()");
        }

        if (!id.isEmpty()) {
            exports->insert(id);
        }

        parent->children.append(element);
        break;
    }
    case QPsdExporterTreeItemModel::ExportHint::Native: {
        Element element;
        element.id = id;

        switch (hint.baseElement) {
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
            element.type = "Group";
            break;
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea:
            element.type = "Button";
            break;
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button:
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted:
            element.type = "Button";
            break;
        }

        if (!outputBase(index, &element, imports))
            return false;

        if (!hint.visible) {
            element.modifiers.append(".hidden()");
        }

        if (!id.isEmpty()) {
            exports->insert(id);
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
        }

        saveTo(hint.componentName, &component, i, x);

        Element element;
        element.type = hint.componentName;
        element.id = id;

        if (!outputBase(index, &element, imports))
            return false;

        if (!hint.visible) {
            element.modifiers.append(".hidden()");
        }

        if (!id.isEmpty()) {
            exports->insert(id);
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

bool QPsdExporterSwiftUIPlugin::traverseElement(QTextStream &out, const Element *element, int level) const
{
    out << indentString(level);

    // Handle special element types
    if (element->type == "Text") {
        QString content = element->properties.value("content").toString();
        out << "Text(\"" << content << "\")";
    } else if (element->type == "Image") {
        QString imageName = element->properties.value("imageName").toString();
        out << "Image(\"" << imageName << "\")";
    } else if (element->type == "RoundedRectangle") {
        qreal cornerRadius = element->properties.value("cornerRadius").toDouble();
        out << "RoundedRectangle(cornerRadius: " << QString::number(cornerRadius, 'f', 1) << ")";
    } else if (element->type == "Path") {
        out << "Path { path in\n";
        level++;
        QStringList pathCommands = element->properties.value("pathCommands").toStringList();
        for (const QString &cmd : pathCommands) {
            out << indentString(level) << cmd << "\n";
        }
        level--;
        out << indentString(level) << "}";
    } else if (element->type == "ZStack" || element->type == "VStack" || element->type == "HStack" || element->type == "Group") {
        QString alignment = element->properties.value("alignment").toString();
        QString spacing = element->properties.value("spacing").toString();

        out << element->type;
        if (!alignment.isEmpty() || !spacing.isEmpty()) {
            out << "(";
            bool first = true;
            if (!alignment.isEmpty()) {
                out << "alignment: " << alignment;
                first = false;
            }
            if (!spacing.isEmpty()) {
                if (!first) out << ", ";
                out << "spacing: " << spacing;
            }
            out << ")";
        }

        out << " {\n";
        level++;
        for (const auto &child : element->children) {
            traverseElement(out, &child, level);
            out << "\n";
        }
        level--;
        out << indentString(level) << "}";
    } else {
        // Default handling
        out << element->type;
        if (!element->children.isEmpty()) {
            out << " {\n";
            level++;
            for (const auto &child : element->children) {
                traverseElement(out, &child, level);
                out << "\n";
            }
            level--;
            out << indentString(level) << "}";
        }
    }

    // Output modifiers
    for (const QString &modifier : element->modifiers) {
        out << "\n" << indentString(level + 1) << modifier;
    }

    return true;
}

bool QPsdExporterSwiftUIPlugin::saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const
{
    Q_UNUSED(exports);

    QString viewsPath = dir.absoluteFilePath("Views");
    QDir viewsDir(viewsPath);
    if (!viewsDir.exists()) {
        viewsDir.mkpath(".");
    }

    QFile file(viewsDir.absoluteFilePath(baseName + ".swift"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&file);

    // License text
    if (!licenseText.isEmpty()) {
        const QStringList lines = licenseText.split('\n');
        for (const QString &line : lines) {
            out << "// " << line << "\n";
        }
        out << "\n";
    }

    // Imports
    out << "import SwiftUI\n";
    auto importList = imports.values();
    std::sort(importList.begin(), importList.end());
    for (const auto &import : importList) {
        if (import != "SwiftUI") {
            out << "import " << import << "\n";
        }
    }
    out << "\n";

    // Struct definition
    out << "struct " << baseName << ": View {\n";
    out << indentString(1) << "var body: some View {\n";

    // Wrap content in ZStack
    out << indentString(2) << "ZStack {\n";

    for (const auto &child : element->children) {
        traverseElement(out, &child, 3);
        out << "\n";
    }

    out << indentString(2) << "}\n";

    // Add frame for root size
    qreal width = element->properties.value("width").toDouble();
    qreal height = element->properties.value("height").toDouble();
    if (width > 0 && height > 0) {
        out << indentString(2) << ".frame(width: " << QString::number(width, 'f', 1)
            << ", height: " << QString::number(height, 'f', 1) << ")\n";
    }

    out << indentString(1) << "}\n";
    out << "}\n";

    // Preview
    out << "\n";
    out << "#Preview {\n";
    out << indentString(1) << baseName << "()\n";
    out << "}\n";

    return true;
}

bool QPsdExporterSwiftUIPlugin::exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const
{
    setModel(model);
    dir = QDir(to);

    // Create Assets.xcassets/Images directory
    QString assetsPath = dir.absoluteFilePath("Assets.xcassets/Images");
    assetsDir = QDir(assetsPath);
    if (!assetsDir.exists()) {
        assetsDir.mkpath(".");
    }

    // Create Assets.xcassets/Contents.json
    QFile assetsContents(dir.absoluteFilePath("Assets.xcassets/Contents.json"));
    if (assetsContents.open(QIODevice::WriteOnly)) {
        QJsonObject contents;
        QJsonObject info;
        info["author"] = "xcode";
        info["version"] = 1;
        contents["info"] = info;
        assetsContents.write(QJsonDocument(contents).toJson());
        assetsContents.close();
    }

    // Create Images/Contents.json
    QFile imagesContents(assetsDir.absoluteFilePath("Contents.json"));
    if (imagesContents.open(QIODevice::WriteOnly)) {
        QJsonObject contents;
        QJsonObject info;
        info["author"] = "xcode";
        info["version"] = 1;
        contents["info"] = info;
        QJsonObject properties;
        properties["provides-namespace"] = true;
        contents["properties"] = properties;
        imagesContents.write(QJsonDocument(contents).toJson());
        imagesContents.close();
    }

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
    imports.insert("SwiftUI");
    ExportData exports;

    Element rootElement;
    rootElement.type = "ZStack";
    rootElement.properties.insert("width", model->size().width() * horizontalScale);
    rootElement.properties.insert("height", model->size().height() * verticalScale);

    for (int i = model->rowCount(QModelIndex {}) - 1; i >= 0; i--) {
        QModelIndex childIndex = model->index(i, 0, QModelIndex {});
        if (!traverseTree(childIndex, &rootElement, &imports, &exports, QPsdExporterTreeItemModel::ExportHint::None))
            return false;
    }

    return saveTo("MainView", &rootElement, imports, exports);
}

QT_END_NAMESPACE

#include "swiftui.moc"
