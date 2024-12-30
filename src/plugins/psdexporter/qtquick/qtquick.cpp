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

class QPsdExporterQtQuickPlugin : public QPsdExporterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdExporterFactoryInterface" FILE "qtquick.json")
    Q_PROPERTY(bool noGPU READ isNoGpu WRITE setNoGpu NOTIFY noGpuChanged FINAL)
public:
    int priority() const override { return 10; }
    QIcon icon() const override {
        return QIcon(":/qtquick/qtquick.png");
    }
    QString name() const override {
        return tr("&Qt Quick");
    }
    ExportType exportType() const override { return QPsdExporterPlugin::Directory; }

    bool isNoGpu() const { return m_noGpu; }
public slots:
    void setNoGpu(bool noGpu) {
        if (m_noGpu == noGpu) return;
        m_noGpu = noGpu;
        emit noGpuChanged(noGpu);
    }
signals:
    void noGpuChanged(bool noGpu);

private:
    bool exportTo(const QPsdFolderLayerItem *tree, const QString &to, const QVariantMap &hint) const override;

    struct Element {
        QString type;
        QString id;
        QString name;
        QVariantHash properties;
        QList<Element> children;
        QList<Element> layers;
    };

    bool m_noGpu = false;
};

bool QPsdExporterQtQuickPlugin::exportTo(const QPsdFolderLayerItem *tree, const QString &to, const QVariantMap &hint) const
{
    using ImportData = QSet<QString>;
    using ExportData = QSet<QString>;
    QDir dir(to);
    QPsdImageStore imageStore(dir, "images"_L1);

    const QSize originalSize = tree->rect().size();
    const QSize targetSize = hint.value("resolution", originalSize).toSize();
    const qreal horizontalScale = targetSize.width() / qreal(originalSize.width());
    const qreal verticalScale = targetSize.height() / qreal(originalSize.height());
    const qreal unitScale = std::min(horizontalScale, verticalScale);
    const qreal fontScaleFactor = hint.value("fontScaleFactor", 1.0).toReal() * verticalScale;

    std::function<void(const QPsdAbstractLayerItem *, QRect *)> findChildren;
    QHash<const QPsdAbstractLayerItem *, QRect> rectMap;
    std::function<void(const QPsdAbstractLayerItem *, const QPoint &)> generateRectMap;
    QMultiMap<const QPsdAbstractLayerItem *, const QPsdAbstractLayerItem *> mergeMap;
    std::function<bool(const QPsdAbstractLayerItem *)> generateMergeData;
    std::function<bool(const QPsdAbstractLayerItem *, Element *, ImportData *, ExportData *, QPsdAbstractLayerItem::ExportHint::Type)> traverseTree;

    auto saveTo = [&dir](const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) -> bool {
        QFile file(dir.absoluteFilePath(baseName + ".qml"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        QTextStream out(&file);
        auto imporList = QList<QString>(imports.begin(), imports.end());
        std::sort(imporList.begin(), imporList.end(), [](const QString &a, const QString &b) {
            const auto ia = a.indexOf(".");
            const auto ib = b.indexOf(".");
            if (ia != ib)
                return ia < ib;
            return a < b;
        });
        for (const auto &import : imporList) {
            out << "import " << import << "\n";
        }

        out << "\n";

        std::function<bool(Element *, int)> traverseElement;
        traverseElement = [&](Element *element, int level) -> bool {
            // apply layers recursively
            auto layers = element->layers;
            std::function<void(Element *)> apply;
            apply = [&](Element *parent) {
                if (layers.isEmpty())
                    return;
                auto layer = layers.takeFirst();
                layer.type = "layer.effect: " + layer.type;
                parent->properties.insert("layer.enabled", true);
                apply(&layer);
                parent->children.append(layer);

            };
            // Special case
            if (
                (element->type == "Button" || element->type == "Button_Highlighted")
                && (!element->layers.isEmpty() && element->layers.first().type.endsWith("Gradient"))
                ) {
                auto gradient = element->layers.takeFirst();
                gradient.type = "background: " + gradient.type;
                element->children.append(gradient);
            }
            apply(element);

            out << QByteArray(level * 4, ' ') << element->type << " {\n";
            level++;
            if (!element->id.isEmpty())
                out << QByteArray(level * 4, ' ') << "id: " << element->id << "\n";

            auto keys = element->properties.keys();
            std::sort(keys.begin(), keys.end(), std::less<QString>());
            for (const auto &key : keys) {
                QString valueAsText;
                const auto value = element->properties.value(key);
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
                case QMetaType::QPointF:
                    valueAsText = QString("Qt.point(%1, %2)").arg(value.toPointF().x()).arg(value.toPointF().y());
                    break;
                default:
                    qFatal() << value.typeName() << "is not supported";
                }
                out << QByteArray(level * 4, ' ') << key << ": " << valueAsText << "\n";
            }

            if (level == 1) {
                auto keys = exports.values();
                std::sort(keys.begin(), keys.end(), std::less<QString>());
                for (const auto &key : keys) {
                    out << QByteArray(level * 4, ' ') << "property alias " << key << ": " << key << "\n";
                }
            }

            for (auto &child : element->children) {
                traverseElement(&child, level);
            }
            level--;
            out << QByteArray(level * 4, ' ') << "}\n";
            return true;
        };
        return traverseElement(element, 0);
    };

    auto outputRect = [&](const QRectF &rect, Element *element, bool skipEmpty = false) -> bool {
        element->properties.insert("x", rect.x() * horizontalScale);
        element->properties.insert("y", rect.y() * verticalScale);
        if (rect.isEmpty() && skipEmpty)
            return true;
        element->properties.insert("width", rect.width() * horizontalScale);
        element->properties.insert("height", rect.height() * verticalScale);
        return true;
    };

    auto outputPath = [&](const QPainterPath &path, Element *element) -> bool {
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
    };

    auto outputBase = [&](const QPsdAbstractLayerItem *item, Element *element, ImportData *imports, QRect rectBounds = {}) -> bool {
        QRect rect;
        if (rectBounds.isEmpty()) {
            rect = item->rect();
            if (hint.value("makeCompact", false).toBool()) {
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
        if (rect.isEmpty()) {
            element->properties.insert("anchors.fill", "parent");
        } else {
            outputRect(rect, element);
        }

        if (item->opacity() < 1.0) {
            element->properties.insert("opacity", item->opacity());
        }

        if (!isNoGpu()) {
            for (const auto &effect : item->effects()) {
                if (effect.canConvert<QPsdSofiEffect>()) {
                    const auto sofi = effect.value<QPsdSofiEffect>();
                    QColor color(sofi.nativeColor());
                    // color.setAlphaF(sofi.opacity());
                    switch (sofi.blendMode()) {
                    case QPsdBlend::Mode::Normal: {
                        // override pixels in the image with the color and opacity
                        imports->insert("Qt5Compat.GraphicalEffects as GE");
                        Element colorize;
                        colorize.type = "GE.ColorOverlay";
                        colorize.properties.insert("color", u"\"%1\""_s.arg(color.name()));
                        colorize.properties.insert("opacity", sofi.opacity());
                        element->layers.append(colorize);
                        break; }
                    default:
                        qWarning() << sofi.blendMode() << "not supported blend mode";
                        break;
                    }
                }
            }

            const auto mask = item->vectorMask();
            if (mask.type != QPsdAbstractLayerItem::PathInfo::None) {
                imports->insert("Qt5Compat.GraphicalEffects as GE");
                // layer->properties.insert("layer.enabled", true);
                Element effect;
                effect.type = "GE.OpacityMask";
                switch (mask.type) {
                case QPsdAbstractLayerItem::PathInfo::Rectangle:
                case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
                    bool filled = (mask.rect.topLeft() == QPointF(0, 0) && mask.rect.size() == item->rect().size());
                    Element maskSource;
                    Element rectangle;
                    if (!filled) {
                        maskSource.type = "maskSource: Item";
                        maskSource.properties.insert("width", item->rect().width() * horizontalScale);
                        maskSource.properties.insert("height", item->rect().height() * verticalScale);
                        rectangle.type = "Rectangle";
                    } else {
                        rectangle.type = "maskSource: Rectangle";
                    }
                    if (mask.radius > 0)
                        rectangle.properties.insert("radius", mask.radius * unitScale);
                    outputRect(mask.rect, &rectangle);
                    if (!filled) {
                        maskSource.children.append(rectangle);
                        effect.children.append(maskSource);
                    } else {
                        effect.children.append(rectangle);
                    }
                    break; }
                case QPsdAbstractLayerItem::PathInfo::Path: {
                    imports->insert("QtQuick.Shapes");
                    Element maskSource;
                    maskSource.type = "maskSource: Shape";
                    Element shapePath;
                    shapePath.type = "ShapePath";
                    if (!outputPath(mask.path, &shapePath))
                        return false;
                    maskSource.children.append(shapePath);
                    effect.children.append(maskSource);
                    break; }
                }
                element->layers.append(effect);
            }

            const auto dropShadow = item->dropShadow();
            if (!dropShadow.isEmpty()) {
                imports->insert("Qt5Compat.GraphicalEffects as GE");
                // layer->properties.insert("layer.enabled", true);
                Element effect;
                effect.type = "GE.DropShadow";

                QColor color(dropShadow.value("color").toString());
                color.setAlphaF(dropShadow.value("opacity").toDouble());
                effect.properties.insert("color", u"\"%1\""_s.arg(color.name(QColor::HexArgb)));
                const auto angle = dropShadow.value("angle").toDouble() * M_PI / 180.0;
                const auto distance = dropShadow.value("distance").toDouble() * unitScale;
                effect.properties.insert("horizontalOffset", std::cos(angle) * distance);
                effect.properties.insert("verticalOffset", std::sin(angle) * distance);
                effect.properties.insert("spread", dropShadow.value("spread").toDouble() * unitScale);
                effect.properties.insert("radius", dropShadow.value("size").toDouble() * unitScale);
                element->layers.append(effect);
            }
        }

        return true;
    };

    auto outputFolder = [&](const QPsdFolderLayerItem *folder, Element *element, ImportData *imports, ExportData *exports) -> bool {
        element->type = "Item";
        if (!outputBase(folder, element, imports))
            return false;

        if (folder->artboardRect().isValid() && folder->artboardBackground() != Qt::transparent) {
            Element artboard;
            artboard.type = "Rectangle";
            outputRect(folder->artboardRect(), &artboard);
            artboard.properties.insert("color", u"\"%1\""_s.arg(folder->artboardBackground().name()));
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

    auto outputText = [&](const QPsdTextLayerItem *text, Element *element, ImportData *imports) -> bool {
        const auto runs = text->runs();
        if (runs.size() == 1) {
            const auto run = runs.first();
            element->type = "Text";
            if (!outputBase(text, element, imports, text->bounds().toRect()))
                return false;
            auto lines = run.text.split("\n");
            // replace empty line with a space
            for (auto &line : lines)
                if (line.isEmpty())
                    line = " ";
            element->properties.insert("text", u"\"%1\""_s.arg(lines.join("\\n")));
            element->properties.insert("font.family", u"\"%1\""_s.arg(run.font.family()));
            element->properties.insert("font.pointSize", run.font.pointSizeF() / 1.5 * fontScaleFactor);
            if (run.font.bold())
                element->properties.insert("font.bold", true);
            if (run.font.italic())
                element->properties.insert("font.italic", true);
            element->properties.insert("color", u"\"%1\""_s.arg(run.color.name()));
            element->properties.insert("horizontalAlignment", "Text.AlignHCenter");
            switch (run.alignment) {
            case Qt::AlignTop:
                element->properties.insert("verticalAlignment", "Text.AlignTop");
                break;
            case Qt::AlignBottom:
                element->properties.insert("verticalAlignment", "Text.AlignBottom");
                break;
            case Qt::AlignVCenter:
                element->properties.insert("verticalAlignment", "Text.AlignVCenter");
                break;
            }
        } else {
            element->type = "Item";
            if (!outputBase(text, element, imports, text->bounds().toRect()))
                return false;

            Element column;
            column.type = "Column";
            column.properties.insert("anchors.centerIn", "parent");

            imports->insert("QtQuick.Layouts");
            Element rowLayout;
            rowLayout.type = "RowLayout";
            rowLayout.properties.insert("spacing", 0);
            rowLayout.properties.insert("anchors.horizontalCenter", "parent.horizontalCenter");

            for (const auto &run : runs) {
                const auto texts = run.text.split("\n");
                bool first = true;
                for (const auto &text : texts) {
                    if (first) {
                        first = false;
                    } else {
                        column.children.append(rowLayout);
                        rowLayout.children.clear();
                    }
                    Element textElement;
                    textElement.type = "Text";
                    textElement.properties.insert("text", u"\"%1\""_s.arg(text.isEmpty() ? " " : text));
                    textElement.properties.insert("font.family", u"\"%1\""_s.arg(run.font.family()));
                    textElement.properties.insert("font.pointSize", run.font.pointSizeF() / 1.5 * fontScaleFactor);
                    if (run.font.bold())
                        textElement.properties.insert("font.bold", true);
                    if (run.font.italic())
                        textElement.properties.insert("font.italic", true);
                    textElement.properties.insert("color", u"\"%1\""_s.arg(run.color.name()));
                    textElement.properties.insert("Layout.fillHeight", true);
                    switch (run.alignment) {
                    case Qt::AlignTop:
                        textElement.properties.insert("verticalAlignment", "Text.AlignTop");
                        break;
                    case Qt::AlignBottom:
                        textElement.properties.insert("verticalAlignment", "Text.AlignBottom");
                        break;
                    case Qt::AlignVCenter:
                        textElement.properties.insert("verticalAlignment", "Text.AlignVCenter");
                        break;
                    }
                    rowLayout.children.append(textElement);
                }
            }
            column.children.append(rowLayout);
            element->children.append(column);
        }
        return true;
    };

    auto outputShape = [&](const QPsdShapeLayerItem *shape, Element *element, ImportData *imports) -> bool {
        const auto path = shape->pathInfo();
        switch (path.type) {
        case QPsdAbstractLayerItem::PathInfo::Rectangle: {
            bool filled = (path.rect.topLeft() == QPointF(0, 0) && path.rect.size() == shape->rect().size());
            if (!filled) {
                element->type = "Item";
                if (!outputBase(shape, element, imports))
                    return false;
            }

            if (shape->gradient()) {
                const auto g = shape->gradient();
                switch (g->type()) {
                case QGradient::LinearGradient: {
                    const auto linear = reinterpret_cast<const QLinearGradient *>(g);
                    bool simpleVertical = linear->start().x() == linear->finalStop().x();
                    Element gradient;
                    gradient.type = "gradient: Gradient";
                    for (const auto &stop : linear->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name()));
                        gradient.children.append(stopElement);
                    }

                    if (simpleVertical) {
                        element->type = "Rectangle";
                        element->children.append(gradient);
                    } else if (!isNoGpu()) {
                        imports->insert("Qt5Compat.GraphicalEffects as GE");
                        Element effect;
                        effect.type = "GE.LinearGradient";
                        if (filled) {
                            if (!outputBase(shape, &effect, imports))
                                return false;
                        } else {
                            outputRect(path.rect, &effect);
                        }
                        effect.properties.insert("start", QPointF(linear->start().x() * horizontalScale, linear->start().y() * verticalScale));
                        effect.properties.insert("end", QPointF(linear->finalStop().x() * horizontalScale, linear->finalStop().y() * verticalScale));
                        effect.children.append(gradient);
                        if (filled)
                            *element = effect;
                        else
                            element->children.prepend(effect);
                    }

                    break; }
                default:
                    qFatal() << "Unsupported gradient type" << g->type();
                }
            } else {
                Element rectElement;
                rectElement.type = "Rectangle";
                if (filled) {
                    if (!outputBase(shape, &rectElement, imports))
                        return false;
                } else {
                    outputRect(path.rect, &rectElement);
                }
                if (shape->pen().style() != Qt::NoPen) {
                    qreal dw = std::max(1.0, shape->pen().width() * unitScale);
                    outputRect(path.rect.adjusted(-dw, -dw, dw, dw), &rectElement);
                    rectElement.properties.insert("border.width", dw);
                    rectElement.properties.insert("border.color", u"\"%1\""_s.arg(shape->pen().color().name()));
                }
                if (shape->brush() != Qt::NoBrush)
                    rectElement.properties.insert("color", u"\"%1\""_s.arg(shape->brush().color().name()));
                if (filled)
                    *element = rectElement;
                else
                    element->children.append(rectElement);
            }
            break; }
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
            bool filled = (path.rect.topLeft() == QPointF(0, 0) && path.rect.size() == shape->rect().size());
            Element rectElement;
            rectElement.type = "Rectangle";
            if (filled) {
                if (!outputBase(shape, &rectElement, imports))
                    return false;
            } else {
                element->type = "Item";
                if (!outputBase(shape, element, imports))
                    return false;
                outputRect(path.rect, &rectElement);
            }
            rectElement.properties.insert("radius", path.radius * unitScale);

            if (shape->gradient()) {
                const auto g = shape->gradient();
                switch (g->type()) {
                case QGradient::LinearGradient: {
                    const auto linear = reinterpret_cast<const QLinearGradient *>(g);
                    bool simpleVertical = linear->start().x() == linear->finalStop().x();
                    Element gradient;
                    gradient.type = "gradient: Gradient";
                    for (const auto &stop : linear->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name()));
                        gradient.children.append(stopElement);
                    }

                    if (simpleVertical) {
                        rectElement.children.append(gradient);
                    } else if (!isNoGpu()) {
                        imports->insert("Qt5Compat.GraphicalEffects as GE");
                        Element effect;
                        effect.type = "GE.LinearGradient";
                        if (filled) {
                            if (!outputBase(shape, &effect, imports))
                                return false;
                        } else {
                            outputRect(path.rect, &effect);
                        }
                        effect.properties.insert("start", QPointF(linear->start().x() * horizontalScale, linear->start().y() * verticalScale));
                        effect.properties.insert("end", QPointF(linear->finalStop().x() * horizontalScale, linear->finalStop().y() * verticalScale));
                        effect.children.append(gradient);
                        rectElement.layers.append(effect);
                    }

                    break; }
                default:
                    qFatal() << "Unsupported gradient type" << g->type();
                }
            } else {
                if (shape->pen().style() != Qt::NoPen) {
                    qreal dw = std::max(1.0, shape->pen().width() * unitScale);
                    outputRect(path.rect.adjusted(-dw, -dw, dw, dw), &rectElement);
                    rectElement.properties.insert("border.width", dw);
                    rectElement.properties.insert("border.color", u"\"%1\""_s.arg(shape->pen().color().name()));
                }
                if (shape->brush() != Qt::NoBrush)
                    rectElement.properties.insert("color", u"\"%1\""_s.arg(shape->brush().color().name()));
            }
            if (filled)
                *element = rectElement;
            else
                element->children.append(rectElement);
            break; }
        case QPsdAbstractLayerItem::PathInfo::Path: {
            imports->insert("QtQuick.Shapes");
            element->type = "Shape";
            if (!outputBase(shape, element, imports))
                return false;
            Element shapePath;
            shapePath.type = "ShapePath";
            if (shape->gradient()) {
                shapePath.properties.insert("strokeWidth", 0);
                shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                const auto g = shape->gradient();
                switch (g->type()) {
                case QGradient::LinearGradient: {
                    const auto linear = reinterpret_cast<const QLinearGradient *>(g);
                    Element gradient;
                    gradient.type = "fillGradient: LinearGradient";
                    gradient.properties.insert("x1", linear->start().x() * horizontalScale);
                    gradient.properties.insert("y1", linear->start().y() * verticalScale);
                    gradient.properties.insert("x2", linear->finalStop().x() * horizontalScale);
                    gradient.properties.insert("y2", linear->finalStop().y() * verticalScale);
                    for (const auto &stop : linear->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name()));
                        gradient.children.append(stopElement);
                    }
                    shapePath.children.append(gradient);
                    qDebug() << gradient.children.length();
                    break; }
                default:
                    qFatal() << "Unsupported gradient type" << g->type();
                }
                element->children.append(shapePath);
            } else {
                shapePath.properties.insert("strokeWidth", shape->pen().width() * unitScale);
                if (shape->pen().style() == Qt::NoPen)
                    shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                else
                    shapePath.properties.insert("strokeColor", u"\"%1\""_s.arg(shape->pen().color().name()));
                if (shape->brush() != Qt::NoBrush)
                    shapePath.properties.insert("fillColor", u"\"%1\""_s.arg(shape->brush().color().name()));
                element->children.append(shapePath);            }
            if (!outputPath(path.path, &shapePath))
                return false;
            element->children.append(shapePath);
            break; }
        }
        return true;
    };

    auto outputImage = [&](const QPsdImageLayerItem *image, Element *element, ImportData *imports) -> bool {
        QString name;
        bool done = false;
        const auto linkedFile = image->linkedFile();
        if (!linkedFile.type.isEmpty()) {
            QImage qimage = image->linkedImage();
            if (!qimage.isNull()) {
                if (hint.value("imageScaling", false).toBool()) {
                    qimage = qimage.scaled(image->rect().width() * horizontalScale, image->rect().height() * verticalScale, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                QByteArray format = linkedFile.type.trimmed();
                name = imageStore.save(imageFileName(linkedFile.name, QString::fromLatin1(format.constData())), qimage, format.constData());
                done = !name.isEmpty();
            }
        }
        if (!done) {
            QImage qimage = image->image();
            if (hint.value("imageScaling", false).toBool()) {
                qimage = qimage.scaled(image->rect().width() * horizontalScale, image->rect().height() * verticalScale, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            name = imageStore.save(imageFileName(image->name(), "PNG"_L1), qimage, "PNG");
        }

        element->type = "Image";
        if (!outputBase(image, element, imports))
            return false;
        element->properties.insert("source", u"\"images/%1\""_s.arg(name));
        element->properties.insert("fillMode", "Image.PreserveAspectFit");
        return true;
    };

    findChildren = [&](const QPsdAbstractLayerItem *item, QRect *rect) {
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

    generateRectMap = [&](const QPsdAbstractLayerItem *item, const QPoint &topLeft) {
        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            const auto folder = reinterpret_cast<const QPsdFolderLayerItem *>(item);
            QRect contentRect;
            if (item->parent() == tree) {
                contentRect = tree->rect();
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

    generateMergeData = [&](const QPsdAbstractLayerItem *item) -> bool {
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

    traverseTree = [&](const QPsdAbstractLayerItem *item, Element *parent, ImportData *imports, ExportData *exports, QPsdAbstractLayerItem::ExportHint::Type hintOverload) -> bool
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
            element.id = id;
            if (!hint.visible)
                element.properties.insert("visible", false);
            if (!id.isEmpty())
                exports->insert(id);
            switch (item->type()) {
            case QPsdAbstractLayerItem::Folder: {
                auto folder = reinterpret_cast<const QPsdFolderLayerItem *>(item);
                outputFolder(folder, &element, imports, exports);
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

            if (!id.isEmpty()) {
                if (hint.baseElement == QPsdAbstractLayerItem::ExportHint::NativeComponent::TouchArea) {
                    Element touchArea { "MouseArea", id };
                    outputBase(item, &touchArea, imports);
                    touchArea.layers.clear();
                    if (!hint.visible)
                        touchArea.properties.insert("visible", "false");
                    element.id = QString();
                    element.properties.remove("x");
                    element.properties.remove("y");
                    element.properties.remove("visible");
                    touchArea.children.append(element);
                    parent->children.append(touchArea);
                } else {
                    parent->children.append(element);
                }
                exports->insert(id);
            } else {
                parent->children.append(element);
            }
            break; }
        case QPsdAbstractLayerItem::ExportHint::Native: {
            Element element;
            switch (hint.baseElement) {
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::Container:
                element.type = "Rectangle";
                break;
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::TouchArea:
                element.type = "MouseArea";
                break;
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button:
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button_Highlighted:
                imports->insert("QtQuick.Controls");
                element.type = "Button";
                element.properties.insert("highlighted", (hint.baseElement == QPsdAbstractLayerItem::ExportHint::NativeComponent::Button_Highlighted));
                if (mergeMap.contains(item)) {
                    for (const auto *i : mergeMap.values(item)) {
                        switch (i->type()) {
                        case QPsdAbstractLayerItem::Text: {
                            const auto *textItem = reinterpret_cast<const QPsdTextLayerItem *>(i);
                            const auto runs = textItem->runs();
                            bool first = true;
                            QString text;
                            for (const auto &run : runs) {
                                if (first) {
                                    element.properties.insert("font.family", u"\"%1\""_s.arg(run.font.family()));
                                    element.properties.insert("font.pointSize", run.font.pointSizeF() / 1.5);
                                    first = false;
                                }
                                text += run.text;
                            }
                            element.properties.insert("text", u"\"%1\""_s.arg(text));
                            break; }
                        default:
                            qWarning() << i->type() << "is not supported";
                        }
                    }
                }
                break;
            }
            element.id = id;
            if (!hint.visible)
                element.properties.insert("visible", false);
            if (!id.isEmpty())
                exports->insert(id);
            if (!outputBase(item, &element, imports))
                return false;
            parent->children.append(element);
            break; }
        case QPsdAbstractLayerItem::ExportHint::Custom: {
            ImportData i;
            i.insert("QtQuick");
            ExportData x;

            Element component;

            switch (item->type()) {
            case QPsdAbstractLayerItem::Folder: {
                auto folder = reinterpret_cast<const QPsdFolderLayerItem *>(item);
                outputFolder(folder, &component, &i, &x);
                break; }
            case QPsdAbstractLayerItem::Text: {
                const auto text = reinterpret_cast<const QPsdTextLayerItem *>(item);
                outputText(text, &component, &i);
                break; }
            case QPsdAbstractLayerItem::Shape: {
                const auto shape = reinterpret_cast<const QPsdShapeLayerItem *>(item);
                outputShape(shape, &component, &i);
                break; }
            case QPsdAbstractLayerItem::Image: {
                const auto image = reinterpret_cast<const QPsdImageLayerItem *>(item);
                outputImage(image, &component, &i);
                break; }
            }
            switch (hint.baseElement) {
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::Container:
                component.type = "Item";
                break;
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::TouchArea:
                component.type = "MouseArea";
                break;
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button:
            case QPsdAbstractLayerItem::ExportHint::NativeComponent::Button_Highlighted:
                i.insert("QtQuick.Controls");
                component.type = "Button";
                component.properties.insert("highlighted", (hint.baseElement == QPsdAbstractLayerItem::ExportHint::NativeComponent::Button_Highlighted));
                break;
            }
            saveTo(hint.componentName + ".ui", &component, i, x);

            Element element;
            element.type = hint.componentName;
            element.id = id;
            if (!id.isEmpty())
                exports->insert(id);
            outputBase(item, &element, imports);
            if (!hint.visible)
                element.properties.insert("visible", false);
            parent->children.append(element);
            break;
                                }
        case QPsdAbstractLayerItem::ExportHint::Merge:
        case QPsdAbstractLayerItem::ExportHint::Skip:
            return true;
        }

        return true;
    };


    auto children = tree->children();
    for (const auto *child : children) {
        generateRectMap(child, QPoint(0, 0));
        if (!generateMergeData(child))
            return false;
    }

    ImportData imports;
    imports.insert("QtQuick");
    ExportData exports;

    Element window;
    window.type = "Item";
    window.properties.insert("width", tree->rect().width() * horizontalScale);
    window.properties.insert("height", tree->rect().height() * verticalScale);

    std::reverse(children.begin(), children.end());
    for (const auto *child : children) {
        if (!traverseTree(child, &window, &imports, &exports, QPsdAbstractLayerItem::ExportHint::None))
            return false;
    }

    return saveTo("MainWindow.ui", &window, imports, exports);
}

QT_END_NAMESPACE

#include "qtquick.moc"
