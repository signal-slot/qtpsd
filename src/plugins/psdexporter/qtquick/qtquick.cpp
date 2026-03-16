// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/qpsdexporterplugin.h>
#include <QtPsdExporter/qpsdimagestore.h>

#include <QtCore/QCborMap>
#include <QtCore/QDir>
#include <QtCore/QQueue>

#include <optional>

#include <QtGui/QBrush>
#include <QtGui/QFontMetrics>
#include <QtGui/QGuiApplication>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QScreen>

#include <QtPsdCore/qpsdblend.h>
#include <QtPsdCore/QPsdDescriptor>
#include <QtPsdCore/QPsdSofiEffect>
#include <QtPsdCore/QPsdUnitFloat>
#include <QtPsdGui/QPsdAdjustmentLayerItem>
#include <QtPsdGui/QPsdBorder>
#include <QtPsdGui/QPsdGuiLayerTreeItemModel>
#include <QtPsdGui/QPsdPatternFill>
#include <QtPsdGui/qpsdguiglobal.h>
#include <QtPsdGui/qpsdborder.h>

QT_BEGIN_NAMESPACE

static QString blendModeString(QPsdBlend::Mode mode)
{
    switch (mode) {
    case QPsdBlend::Normal:      return u"normal"_s;
    case QPsdBlend::Dissolve:    return u"dissolve"_s;
    case QPsdBlend::Darken:      return u"darken"_s;
    case QPsdBlend::Multiply:    return u"multiply"_s;
    case QPsdBlend::ColorBurn:   return u"colorBurn"_s;
    case QPsdBlend::LinearBurn:  return u"linearBurn"_s;
    case QPsdBlend::DarkerColor: return u"darkerColor"_s;
    case QPsdBlend::Lighten:     return u"lighten"_s;
    case QPsdBlend::Screen:      return u"screen"_s;
    case QPsdBlend::ColorDodge:  return u"colorDodge"_s;
    case QPsdBlend::LinearDodge: return u"linearDodge"_s;
    case QPsdBlend::LighterColor: return u"lighterColor"_s;
    case QPsdBlend::Overlay:     return u"overlay"_s;
    case QPsdBlend::SoftLight:   return u"softLight"_s;
    case QPsdBlend::HardLight:   return u"hardLight"_s;
    case QPsdBlend::VividLight:  return u"vividLight"_s;
    case QPsdBlend::LinearLight: return u"linearLight"_s;
    case QPsdBlend::PinLight:    return u"pinLight"_s;
    case QPsdBlend::HardMix:     return u"hardMix"_s;
    case QPsdBlend::Difference:  return u"difference"_s;
    case QPsdBlend::Exclusion:   return u"exclusion"_s;
    case QPsdBlend::Subtract:    return u"subtract"_s;
    case QPsdBlend::Divide:      return u"divide"_s;
    case QPsdBlend::Hue:         return u"hue"_s;
    case QPsdBlend::Saturation:  return u"saturation"_s;
    case QPsdBlend::Color:       return u"color"_s;
    case QPsdBlend::Luminosity:  return u"luminosity"_s;
    case QPsdBlend::PassThrough:
    case QPsdBlend::Invalid:
    default:                     return QString();
    }
}

class QPsdExporterQtQuickPlugin : public QPsdExporterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdExporterFactoryInterface" FILE "qtquick.json")
    Q_PROPERTY(EffectMode effectMode READ effectMode WRITE setEffectMode NOTIFY effectModeChanged FINAL)
public:
    enum EffectMode { NoGPU, Qt5Effects, EffectMaker };
    Q_ENUM(EffectMode)

    int priority() const override { return 10; }
    QIcon icon() const override {
        return QIcon(":/qtquick/qtquick.png");
    }
    QString name() const override {
        return tr("&Qt Quick");
    }
    ExportType exportType() const override { return QPsdExporterPlugin::Directory; }

    EffectMode effectMode() const { return m_effectMode; }
public slots:
    void setEffectMode(EffectMode effectMode) {
        if (m_effectMode == effectMode) return;
        m_effectMode = effectMode;
        emit effectModeChanged(effectMode);
    }
signals:
    void effectModeChanged(EffectMode effectMode);

private:
    using ImportData = QSet<QString>;
    using ExportData = QSet<QString>;

    bool exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const ExportConfig &config) const override;

    struct Element {
        QString type;
        QString id;
        QString name;
        QVariantHash properties;
        QList<Element> children;
        QList<Element> layers;
    };

    EffectMode m_effectMode = EffectMaker;
    mutable bool m_needsBlendShader = false;
    mutable int m_blendCounter = 0;
    mutable int m_maskCounter = 0;

    bool outputBase(const QModelIndex &index, Element *element, ImportData *imports, QRect rectBounds = {}) const;
    bool outputRect(const QRectF &rect, Element *element, bool skipEmpty = false) const;
    bool outputPath(const QPainterPath &path, Element *element) const;
    bool outputFolder(const QModelIndex &folderIndex, Element *element, ImportData *imports, ExportData *exports, QPsdBlend::Mode groupBlendMode = QPsdBlend::PassThrough) const;
    bool outputText(const QModelIndex &textIndex, Element *element, ImportData *imports) const;
    bool outputShape(const QModelIndex &shapeIndex, Element *element, ImportData *imports) const;
    bool outputImage(const QModelIndex &imageIndex, Element *element, ImportData *imports) const;

    bool traverseTree(const QModelIndex &index, Element *parent, ImportData *imports, ExportData *exports, std::optional<QPsdExporterTreeItemModel::ExportHint::Type> hintOverload = std::nullopt, QPsdBlend::Mode groupBlendMode = QPsdBlend::PassThrough) const;
    void applyBlendModes(Element *element, ImportData *imports) const;
    void applyAdjustmentLayer(const QPsdAbstractLayerItem *item, Element *parent, ImportData *imports) const;
    static void stripLayerEffects(Element &element);

    mutable bool m_needsAdjustmentShader = false;
    mutable int m_adjustmentCounter = 0;

    bool saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const;
};

bool QPsdExporterQtQuickPlugin::exportTo(const QPsdExporterTreeItemModel *model,  const QString &to, const ExportConfig &config) const
{
    if (!initializeExport(model, to, config, "images"_L1)) {
        return false;
    }

    m_blendCounter = 0;
    m_maskCounter = 0;
    m_adjustmentCounter = 0;
    m_needsBlendShader = false;
    m_needsAdjustmentShader = false;

    ImportData imports;
    imports.insert("QtQuick");
    ExportData exports;

    Element window;
    window.type = "Item";
    window.properties.insert("width", canvasSize().width() * horizontalScale);
    window.properties.insert("height", canvasSize().height() * verticalScale);

    // Helper: use the merged image from Image Data section (pre-composited by Photoshop)
    auto appendMergedImage = [&]() {
        const QImage merged = model->guiLayerTreeItemModel()->mergedImage();
        if (merged.isNull())
            return;
        const QImage output = imageScaling
            ? merged.scaled(canvasSize().width() * horizontalScale, canvasSize().height() * verticalScale, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            : merged;
        const QString name = imageStore.save("merged.png"_L1, output, "PNG");
        Element img;
        img.type = "Image";
        img.properties.insert("width", canvasSize().width() * horizontalScale);
        img.properties.insert("height", canvasSize().height() * verticalScale);
        img.properties.insert("x", 0);
        img.properties.insert("y", 0);
        img.properties.insert("source", u"\"images/%1\""_s.arg(name));
        img.properties.insert("fillMode", "Image.PreserveAspectFit");
        window.children.append(img);
    };

    // Detect NoGPU mode with blend modes — requires merged image fallback
    bool needsMergedFallback = false;
    if (effectMode() == NoGPU) {
        std::function<void(const QModelIndex &)> scanForFallback = [&](const QModelIndex &idx) {
            if (needsMergedFallback) return;
            if (idx.isValid()) {
                const auto *layer = model->layerItem(idx);
                if (layer) {
                    const auto mode = layer->record().blendMode();
                    if (mode != QPsdBlend::Normal && mode != QPsdBlend::PassThrough && mode != QPsdBlend::Invalid) {
                        needsMergedFallback = true;
                        return;
                    }
                }
            }
            for (int r = 0; r < model->rowCount(idx); r++)
                scanForFallback(model->index(r, 0, idx));
        };
        scanForFallback(QModelIndex());
    }

    if (needsMergedFallback) {
        appendMergedImage();
    } else {
        for (int i = model->rowCount(QModelIndex {}) - 1; i >= 0; i--) {
            QModelIndex childIndex = model->index(i, 0, QModelIndex {});
            if (!traverseTree(childIndex, &window, &imports, &exports, std::nullopt))
                return false;
        }

        // Apply blend modes for root-level layers (same logic as inside outputFolder)
        applyBlendModes(&window, &imports);

        // Flattened PSD fallback: if no layers were produced, use the merged image
        if (window.children.isEmpty())
            appendMergedImage();
    }

    // Expose artboard children as property aliases for external control (z, visible, etc.)
    // Only generate aliases for top-level artboard folders (identified by clip:true from outputFolder)
    for (const auto &child : std::as_const(window.children)) {
        if (!child.id.isEmpty() && child.properties.value("clip"_L1) == QVariant(true))
            window.properties.insert(u"property alias %1"_s.arg(child.id), child.id);
    }

    if (!saveTo("MainWindow.ui", &window, imports, exports))
        return false;

    // Copy blend shader to output directory if needed
    if (m_needsBlendShader) {
        QFile shader(":/qtquick/blend.frag.qsb");
        if (shader.open(QIODevice::ReadOnly)) {
            QFile output(dir.absoluteFilePath("blend.frag.qsb"));
            if (output.open(QIODevice::WriteOnly))
                output.write(shader.readAll());
        }
        m_needsBlendShader = false;
    }

    // Copy adjustment shader to output directory if needed
    if (m_needsAdjustmentShader) {
        QFile shader(":/qtquick/adjustment.frag.qsb");
        if (shader.open(QIODevice::ReadOnly)) {
            QFile output(dir.absoluteFilePath("adjustment.frag.qsb"));
            if (output.open(QIODevice::WriteOnly))
                output.write(shader.readAll());
        }
        m_needsAdjustmentShader = false;
    }

    return true;
}

bool QPsdExporterQtQuickPlugin::outputBase(const QModelIndex &index, Element *element, ImportData *imports, QRect rectBounds) const
{
    const QPsdAbstractLayerItem *item = model()->layerItem(index);
    QRect rect;
    if (rectBounds.isEmpty()) {
        rect = computeBaseRect(index);
    } else {
        rect = rectBounds;
        if (makeCompact) {
            // makeCompact の場合、位置は indexRectMap から取得（親に対する相対座標）
            // rectBounds の位置補正（テキストの baseline-ascent 等）を保持する
            QRect compactRect = indexRectMap.value(index);
            QPoint delta = rectBounds.topLeft() - item->rect().topLeft();
            rect.moveTopLeft(compactRect.topLeft() + delta);
        }
        rect = adjustRectForMerge(index, rect);
    }

    // Check anchor mode hint
    const auto anchorMode = model()->layerHint(index).anchorMode;
    using AM = QPsdExporterTreeItemModel::ExportHint;
    if (anchorMode != AM::AnchorNone) {
        QSizeF parentSize;
        if (index.parent().isValid())
            parentSize = model()->rect(index.parent()).size();
        else
            parentSize = canvasSize();

        // Horizontal anchor
        switch (anchorMode) {
        case AM::AnchorTopLeft: case AM::AnchorLeft: case AM::AnchorBottomLeft:
            element->properties.insert("anchors.left", "parent.left");
            if (rect.x() != 0)
                element->properties.insert("anchors.leftMargin", rect.x() * horizontalScale);
            break;
        case AM::AnchorTop: case AM::AnchorCenter: case AM::AnchorBottom: {
            element->properties.insert("anchors.horizontalCenter", "parent.horizontalCenter");
            qreal off = rect.x() + rect.width() / 2.0 - parentSize.width() / 2.0;
            if (!qFuzzyIsNull(off))
                element->properties.insert("anchors.horizontalCenterOffset", off * horizontalScale);
            break; }
        case AM::AnchorTopRight: case AM::AnchorRight: case AM::AnchorBottomRight: {
            element->properties.insert("anchors.right", "parent.right");
            qreal m = parentSize.width() - rect.x() - rect.width();
            if (!qFuzzyIsNull(m))
                element->properties.insert("anchors.rightMargin", m * horizontalScale);
            break; }
        default: break;
        }

        // Vertical anchor
        switch (anchorMode) {
        case AM::AnchorTopLeft: case AM::AnchorTop: case AM::AnchorTopRight:
            element->properties.insert("anchors.top", "parent.top");
            if (rect.y() != 0)
                element->properties.insert("anchors.topMargin", rect.y() * verticalScale);
            break;
        case AM::AnchorLeft: case AM::AnchorCenter: case AM::AnchorRight: {
            element->properties.insert("anchors.verticalCenter", "parent.verticalCenter");
            qreal off = rect.y() + rect.height() / 2.0 - parentSize.height() / 2.0;
            if (!qFuzzyIsNull(off))
                element->properties.insert("anchors.verticalCenterOffset", off * verticalScale);
            break; }
        case AM::AnchorBottomLeft: case AM::AnchorBottom: case AM::AnchorBottomRight: {
            element->properties.insert("anchors.bottom", "parent.bottom");
            qreal m = parentSize.height() - rect.y() - rect.height();
            if (!qFuzzyIsNull(m))
                element->properties.insert("anchors.bottomMargin", m * verticalScale);
            break; }
        default: break;
        }

        element->properties.insert("width", rect.width() * horizontalScale);
        element->properties.insert("height", rect.height() * verticalScale);
    } else if (rect.isEmpty()) {
        element->properties.insert("anchors.fill", "parent");
    } else {
        outputRect(rect, element);
    }

    if (auto opac = displayOpacity(item))
        element->properties.insert("opacity", *opac);

    if (effectMode() != NoGPU) {
        for (const auto &effect : item->effects()) {
            if (effect.canConvert<QPsdSofiEffect>()) {
                const auto sofi = effect.value<QPsdSofiEffect>();
                QColor color(sofi.nativeColor());
                // color.setAlphaF(sofi.opacity());
                switch (sofi.blendMode()) {
                case QPsdBlend::Mode::Normal: {
                    // override pixels in the image with the color and opacity
                    Element colorize;
                    if (effectMode() == Qt5Effects) {
                        imports->insert("Qt5Compat.GraphicalEffects as GE");
                        colorize.type = "GE.ColorOverlay";
                        colorize.properties.insert("color", u"\"%1\""_s.arg(color.name(QColor::HexArgb)));
                        colorize.properties.insert("opacity", sofi.opacity());
                    } else {
                        imports->insert("QtQuick.Effects");
                        colorize.type = "MultiEffect";
                        colorize.properties.insert("colorization", 1.0);
                        colorize.properties.insert("colorizationColor", u"\"%1\""_s.arg(color.name(QColor::HexArgb)));
                        colorize.properties.insert("opacity", sofi.opacity());
                    }
                    element->layers.append(colorize);
                    break; }
                default:
                    qWarning() << sofi.blendMode() << "not supported blend mode";
                    break;
                }
            }
        }

        // Shape layers: vmsk defines both pathInfo() and vectorMask() from the same data,
        // so the Shape element already clips to the path — skip the redundant mask.
        const bool isShapeLayer = dynamic_cast<const QPsdShapeLayerItem *>(item) != nullptr;
        const auto mask = item->vectorMask();
        if (mask.type != QPsdAbstractLayerItem::PathInfo::None && !isShapeLayer) {
            // Simple filled rectangle with no radius → use clip: true (lightweight)
            const bool filled = (mask.rect.topLeft() == QPointF(0, 0) && mask.rect.size() == item->rect().size());
            if (filled && mask.type == QPsdAbstractLayerItem::PathInfo::Rectangle && mask.radius <= 0) {
                element->properties.insert("clip"_L1, true);
            } else if (effectMode() == Qt5Effects) {
                imports->insert("Qt5Compat.GraphicalEffects as GE");
                Element effect;
                effect.type = "GE.OpacityMask";
                switch (mask.type) {
                case QPsdAbstractLayerItem::PathInfo::Rectangle:
                case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
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
            } else {
                // MultiEffect: maskSource must be a separate Item with
                // visible:false and layer.enabled:true to work correctly
                imports->insert("QtQuick.Effects");
                const QString maskId = u"_vmask_%1"_s.arg(m_maskCounter++);

                Element maskItem;
                maskItem.properties.insert(u"visible"_s, false);
                maskItem.properties.insert(u"layer.enabled"_s, true);

                switch (mask.type) {
                case QPsdAbstractLayerItem::PathInfo::Rectangle:
                case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
                    if (!filled) {
                        maskItem.type = "Item";
                        maskItem.id = maskId;
                        maskItem.properties.insert("width", item->rect().width() * horizontalScale);
                        maskItem.properties.insert("height", item->rect().height() * verticalScale);
                        Element rectangle;
                        rectangle.type = "Rectangle";
                        if (mask.radius > 0)
                            rectangle.properties.insert("radius", mask.radius * unitScale);
                        outputRect(mask.rect, &rectangle);
                        maskItem.children.append(rectangle);
                    } else {
                        maskItem.type = "Rectangle";
                        maskItem.id = maskId;
                        if (mask.radius > 0)
                            maskItem.properties.insert("radius", mask.radius * unitScale);
                        outputRect(mask.rect, &maskItem);
                    }
                    break; }
                case QPsdAbstractLayerItem::PathInfo::Path: {
                    imports->insert("QtQuick.Shapes");
                    maskItem.type = "Shape";
                    maskItem.id = maskId;
                    Element shapePath;
                    shapePath.type = "ShapePath";
                    if (!outputPath(mask.path, &shapePath))
                        return false;
                    maskItem.children.append(shapePath);
                    break; }
                }

                element->children.prepend(maskItem);

                Element effect;
                effect.type = "MultiEffect";
                effect.properties.insert("maskEnabled", true);
                effect.properties.insert("maskSource", maskId);
                element->layers.append(effect);
            }
        }

        const auto shadow = parseDropShadow(item->dropShadow());
        if (shadow) {
            Element effect;
            const auto offset = dropShadowOffset(*shadow) * unitScale;
            if (effectMode() == Qt5Effects) {
                imports->insert("Qt5Compat.GraphicalEffects as GE");
                effect.type = "GE.DropShadow";
                effect.properties.insert("color", u"\"%1\""_s.arg(shadow->color.name(QColor::HexArgb)));
                effect.properties.insert("horizontalOffset", offset.x());
                effect.properties.insert("verticalOffset", offset.y());
                effect.properties.insert("spread", shadow->spread * unitScale);
                effect.properties.insert("radius", shadow->blur * unitScale);
            } else {
                imports->insert("QtQuick.Effects");
                effect.type = "MultiEffect";
                effect.properties.insert("shadowEnabled", true);
                effect.properties.insert("shadowColor", u"\"%1\""_s.arg(shadow->color.name(QColor::HexArgb)));
                effect.properties.insert("shadowHorizontalOffset", offset.x());
                effect.properties.insert("shadowVerticalOffset", offset.y());
                effect.properties.insert("shadowBlur", shadow->blur * unitScale);
                effect.properties.insert("shadowOpacity", shadow->color.alphaF());
            }
            element->layers.append(effect);
        }
    }

    return true;
}

bool QPsdExporterQtQuickPlugin::outputRect(const QRectF &rect, Element *element, bool skipEmpty) const
{
    element->properties.insert("x", rect.x() * horizontalScale);
    element->properties.insert("y", rect.y() * verticalScale);
    if (rect.isEmpty() && skipEmpty)
        return true;
    element->properties.insert("width", rect.width() * horizontalScale);
    element->properties.insert("height", rect.height() * verticalScale);
    return true;
}

bool QPsdExporterQtQuickPlugin::outputPath(const QPainterPath &path, Element *element) const
{
    switch (path.fillRule()) {
    case Qt::OddEvenFill:
        element->properties.insert("fillRule", "ShapePath.OddEvenFill");
        break;
    case Qt::WindingFill:
        element->properties.insert("fillRule", "ShapePath.WindingFill");
        break;
    }

    const auto commands = pathToCommands(path, horizontalScale, verticalScale);
    for (const auto &cmd : commands) {
        switch (cmd.type) {
        case PathCommand::MoveTo: {
            Element pathMove;
            pathMove.type = "PathMove";
            pathMove.properties.insert("x", cmd.x);
            pathMove.properties.insert("y", cmd.y);
            element->children.append(pathMove);
            break; }
        case PathCommand::LineTo: {
            Element pathLine;
            pathLine.type = "PathLine";
            pathLine.properties.insert("x", cmd.x);
            pathLine.properties.insert("y", cmd.y);
            element->children.append(pathLine);
            break; }
        case PathCommand::CubicTo: {
            Element pathCubic;
            pathCubic.type = "PathCubic";
            pathCubic.properties.insert("control1X", cmd.c1x);
            pathCubic.properties.insert("control1Y", cmd.c1y);
            pathCubic.properties.insert("control2X", cmd.c2x);
            pathCubic.properties.insert("control2Y", cmd.c2y);
            pathCubic.properties.insert("x", cmd.x);
            pathCubic.properties.insert("y", cmd.y);
            element->children.append(pathCubic);
            break; }
        case PathCommand::Close:
            break;
        }
    }
    return true;
}

bool QPsdExporterQtQuickPlugin::outputFolder(const QModelIndex &folderIndex, Element *element, ImportData *imports, ExportData *exports, QPsdBlend::Mode groupBlendMode) const
{
    const QPsdAbstractLayerItem *item = model()->layerItem(folderIndex);
    const QPsdFolderLayerItem *folder = dynamic_cast<const QPsdFolderLayerItem *>(item);
    element->type = "Item";
    if (!outputBase(folderIndex, element, imports))
        return false;

    // Top-level artboard folders: use clip: true instead of MultiEffect mask.
    // MultiEffect mask + layer.enabled + blend ShaderEffects inside = blank rendering.
    // Artboard corner radii are Figma frame decorations, not visible in the exported app.
    if (!folderIndex.parent().isValid() && folder && folder->artboardRect().isValid()) {
        if (!element->layers.isEmpty()) {
            element->layers.clear();
            if (!element->properties.contains("clip"_L1))
                element->properties.insert("clip"_L1, true);
        }
        // Generate id from artboard name for property alias access
        if (element->id.isEmpty()) {
            const auto artboardId = toLowerCamelCase(item->name());
            if (!artboardId.isEmpty())
                element->id = artboardId;
        }
    }

    // Determine the blend mode to propagate to children
    const auto folderBlendMode = item->record().blendMode();
    QPsdBlend::Mode nextGroupBlendMode = groupBlendMode;
    if (folderBlendMode != QPsdBlend::PassThrough && folderBlendMode != QPsdBlend::Normal && folderBlendMode != QPsdBlend::Invalid) {
        nextGroupBlendMode = folderBlendMode;
        // Enable layer compositing so the group renders as a unit
        element->properties.insert("layer.enabled", true);
        const auto modeStr = blendModeString(folderBlendMode);
        if (!modeStr.isEmpty())
            element->properties.insert("property string blendMode", u"\"%1\""_s.arg(modeStr));
    }

    if (folder->artboardRect().isValid() && folder->artboardBackground() != Qt::transparent) {
        Element artboard;
        artboard.type = "Rectangle";
        QRect bgRect(QPoint(0, 0), folder->artboardRect().size());
        outputRect(bgRect, &artboard);
        artboard.properties.insert("color", u"\"%1\""_s.arg(folder->artboardBackground().name(QColor::HexArgb)));
        element->children.append(artboard);
    }
    // Pattern fill layers are folders with PtFl data but no children
    if (model()->rowCount(folderIndex) == 0) {
    const auto ali = item->record().additionalLayerInformation();
    if (ali.contains("PtFl")) {
        const auto ptfl = ali.value("PtFl").value<QPsdDescriptor>().data();
        const auto ptrn = ptfl.value("Ptrn").value<QPsdDescriptor>().data();
        const QString patternId = ptrn.value("Idnt").toString();
        qreal patternScale = 1.0;
        const auto scl = ptfl.value("Scl ").value<QPsdUnitFloat>();
        if (scl.unit() == QPsdUnitFloat::Percent)
            patternScale = scl.value() / 100.0;
        qreal patternAngle = 0;
        const auto angl = ptfl.value("Angl").value<QPsdUnitFloat>();
        if (angl.unit() == QPsdUnitFloat::Angle)
            patternAngle = angl.value();

        auto *guiModel = static_cast<const QPsdExporterTreeItemModel *>(model())->guiLayerTreeItemModel();
        QImage patternImage = guiModel ? guiModel->patternImage(patternId) : QImage();
        if (!patternImage.isNull()) {
            if (patternScale != 1.0 && patternScale > 0) {
                patternImage = patternImage.scaled(
                    qRound(patternImage.width() * patternScale),
                    qRound(patternImage.height() * patternScale),
                    Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
            QBrush brush(patternImage);
            QTransform transform;
            if (patternAngle != 0)
                transform.rotate(patternAngle);
            if (!transform.isIdentity())
                brush.setTransform(transform);

            QImage rendered(item->rect().size(), QImage::Format_ARGB32);
            rendered.fill(Qt::transparent);
            QPainter painter(&rendered);
            painter.setPen(Qt::NoPen);
            painter.setBrush(brush);
            painter.drawRect(rendered.rect());
            painter.end();

            const QString name = imageStore.save(imageFileName(item->name(), "PNG"_L1), rendered, "PNG");
            if (!name.isEmpty()) {
                element->type = "Image";
                element->properties.insert("source", u"\"images/%1\""_s.arg(name));
                element->properties.insert("fillMode", "Image.PreserveAspectFit");
            }
        }
    }
    }

    for (int i = model()->rowCount(folderIndex) - 1; i >= 0; i--) {
        QModelIndex childIndex = model()->index(i, 0, folderIndex);
        if (!traverseTree(childIndex, element, imports, exports, std::nullopt, nextGroupBlendMode))
            return false;
    }
    if (effectMode() != NoGPU)
        applyBlendModes(element, imports);

    // Fix cross-group blend: Figma frame fills (_imgbg) are at the frame level,
    // but blend-mode layers may be inside a sub-frame. The blend background in
    // the sub-frame won't include the image because they're at different levels.
    // Move preceding image siblings into the sub-group's _blend_bg element.
    if (effectMode() != NoGPU) {
        // Find the first Image element in an element tree (nullptr if none)
        std::function<Element *(Element &)> findImage = [&](Element &el) -> Element * {
            if (el.type == "Image"_L1) return &el;
            for (auto &c : el.children)
                if (auto *found = findImage(c))
                    return found;
            return nullptr;
        };
        // Find the deepest _blend_bg_ that has no Image descendants
        std::function<Element *(Element &)> findEmptyBlendBg = [&](Element &el) -> Element * {
            for (auto &c : el.children) {
                if (auto *found = findEmptyBlendBg(c))
                    return found;
                if (c.id.startsWith("_blend_bg_"_L1) && !findImage(c))
                    return &c;
            }
            return nullptr;
        };
        // Apply fix recursively at all nesting levels
        std::function<void(Element &)> fixCrossGroupBlend = [&](Element &parent) {
            for (int i = 0; i < parent.children.size(); i++) {
                if (parent.children[i].type != "Item"_L1) continue;
                Element *blendBg = findEmptyBlendBg(parent.children[i]);
                if (!blendBg) continue;
                bool found = false;
                // First pass: move a non-blend Image sibling into the empty blend_bg
                for (int j = 0; j < i; j++) {
                    auto &candidate = parent.children[j];
                    if (findImage(candidate)
                        && !candidate.id.startsWith("_blend_bg_"_L1)
                        && !candidate.id.startsWith("_blend_fg_"_L1)) {
                        Element img = parent.children.takeAt(j);
                        i--;
                        // Re-fetch: takeAt shifted indices, invalidating the old pointer
                        blendBg = findEmptyBlendBg(parent.children[i]);
                        if (blendBg)
                            blendBg->children.prepend(img);
                        found = true;
                        break;
                    }
                }
                // Second pass: if no standalone image found, clone from a sibling blend_bg
                if (!found) {
                    for (int j = 0; j < i; j++) {
                        if (!parent.children[j].id.startsWith("_blend_bg_"_L1))
                            continue;
                        Element *srcImg = findImage(parent.children[j]);
                        if (srcImg) {
                            blendBg->children.prepend(*srcImg);
                            break;
                        }
                    }
                }
            }
            for (auto &child : parent.children)
                if (child.type == "Item"_L1)
                    fixCrossGroupBlend(child);
        };
        fixCrossGroupBlend(*element);
    }

    // When mask effect (layers) coexists with blend compositing (ShaderEffect children
    // from applyBlendModes) or explicit layer.enabled (from blend mode), wrap children
    // in an inner Item. The serialization always adds layer.enabled when layers exist,
    // and layer.enabled + layer.effect: MultiEffect { maskEnabled } on the same Item
    // that contains ShaderEffect blend children causes blank rendering.
    if (!element->layers.isEmpty()) {
        bool needsWrapping = element->properties.contains("layer.enabled"_L1);
        if (!needsWrapping) {
            for (const auto &child : std::as_const(element->children)) {
                if (child.type == "ShaderEffect"_L1) {
                    needsWrapping = true;
                    break;
                }
            }
        }
        if (needsWrapping) {
            Element inner;
            inner.type = "Item"_L1;
            inner.properties.insert("anchors.fill"_L1, "parent"_L1);
            if (element->properties.contains("layer.enabled"_L1)) {
                inner.properties.insert("layer.enabled"_L1, true);
                element->properties.remove("layer.enabled"_L1);
            }
            if (element->properties.contains("property string blendMode"_L1))
                inner.properties.insert("property string blendMode"_L1, element->properties.take("property string blendMode"_L1));
            inner.children = element->children;
            element->children.clear();
            element->children.append(inner);
        }
    }

    return true;
}

// Strip layer effects (and associated layer.enabled) from elements inside blend wrappers.
// Qt Quick cannot render nested layer effects inside invisible layer-enabled parents;
// the parent's layer texture ends up empty/black.
void QPsdExporterQtQuickPlugin::stripLayerEffects(Element &element)
{
    // Layer effects are stored in element.layers during tree construction,
    // later converted to children in saveTo(). Clear the layers list and
    // remove the layer.enabled property that was only there to support them.
    if (!element.layers.isEmpty()) {
        element.layers.clear();
        element.properties.remove(u"layer.enabled"_s);
    }
    for (auto &child : element.children)
        stripLayerEffects(child);
}

void QPsdExporterQtQuickPlugin::applyBlendModes(Element *element, ImportData *imports) const
{
    // Check if any child has a blend mode
    bool hasBlend = false;
    for (const auto &child : std::as_const(element->children)) {
        if (child.properties.contains(u"property string blendMode"_s)) {
            hasBlend = true;
            break;
        }
    }
    if (!hasBlend) return;

    QList<Element> accumulated;

    for (auto &child : element->children) {
        if (child.properties.contains(u"property string blendMode"_s)) {
            QString mode = child.properties.take(u"property string blendMode"_s).toString();
            mode.remove(u'"');

            if (effectMode() == Qt5Effects) {
                imports->insert(u"Qt5Compat.GraphicalEffects as GE"_s);

                // Wrap accumulated children as background source
                Element bgItem;
                bgItem.type = u"Item"_s;
                bgItem.id = u"_blend_bg_%1"_s.arg(m_blendCounter);
                bgItem.properties.insert(u"anchors.fill"_s, u"parent"_s);
                bgItem.properties.insert(u"layer.enabled"_s, true);
                bgItem.properties.insert(u"visible"_s, true);
                bgItem.properties.insert(u"opacity"_s, 0);
                bgItem.children = accumulated;
                stripLayerEffects(bgItem);

                // Wrap blend-mode child as foreground source
                Element fgItem;
                fgItem.type = u"Item"_s;
                fgItem.id = u"_blend_fg_%1"_s.arg(m_blendCounter);
                fgItem.properties.insert(u"anchors.fill"_s, u"parent"_s);
                fgItem.properties.insert(u"layer.enabled"_s, true);
                fgItem.properties.insert(u"visible"_s, true);
                fgItem.properties.insert(u"opacity"_s, 0);
                fgItem.children.append(child);
                stripLayerEffects(fgItem);

                // Blend composites bg + fg using the specified mode
                Element blend;
                blend.type = u"GE.Blend"_s;
                blend.properties.insert(u"anchors.fill"_s, u"parent"_s);
                blend.properties.insert(u"source"_s, bgItem.id);
                blend.properties.insert(u"foregroundSource"_s, fgItem.id);
                blend.properties.insert(u"mode"_s, u"\"%1\""_s.arg(mode));

                accumulated.clear();
                accumulated.append(bgItem);
                accumulated.append(fgItem);
                accumulated.append(blend);
            } else {
                // EffectMaker mode: ShaderEffect with custom blend shader

                // Wrap accumulated children as background source
                Element bgItem;
                bgItem.type = u"Item"_s;
                bgItem.id = u"_blend_bg_%1"_s.arg(m_blendCounter);
                bgItem.properties.insert(u"anchors.fill"_s, u"parent"_s);
                bgItem.properties.insert(u"layer.enabled"_s, true);
                bgItem.properties.insert(u"visible"_s, true);
                bgItem.properties.insert(u"opacity"_s, 0);
                bgItem.children = accumulated;
                stripLayerEffects(bgItem);

                // Wrap blend-mode child as foreground source
                Element fgItem;
                fgItem.type = u"Item"_s;
                fgItem.id = u"_blend_fg_%1"_s.arg(m_blendCounter);
                fgItem.properties.insert(u"anchors.fill"_s, u"parent"_s);
                fgItem.properties.insert(u"layer.enabled"_s, true);
                fgItem.properties.insert(u"visible"_s, true);
                fgItem.properties.insert(u"opacity"_s, 0);
                fgItem.children.append(child);
                stripLayerEffects(fgItem);

                // Map blend mode string to shader constant
                int blendModeInt = -1; // fallback: normal
                if (mode == "multiply"_L1)         blendModeInt = 0;
                else if (mode == "screen"_L1)      blendModeInt = 1;
                else if (mode == "overlay"_L1)     blendModeInt = 2;
                else if (mode == "darken"_L1)      blendModeInt = 3;
                else if (mode == "lighten"_L1)     blendModeInt = 4;
                else if (mode == "colorDodge"_L1)  blendModeInt = 5;
                else if (mode == "colorBurn"_L1)   blendModeInt = 6;
                else if (mode == "hardLight"_L1)   blendModeInt = 7;
                else if (mode == "softLight"_L1)   blendModeInt = 8;
                else if (mode == "difference"_L1)  blendModeInt = 9;
                else if (mode == "exclusion"_L1)   blendModeInt = 10;
                else if (mode == "linearBurn"_L1)  blendModeInt = 11;
                else if (mode == "linearDodge"_L1) blendModeInt = 12;
                else if (mode == "vividLight"_L1)  blendModeInt = 13;
                else if (mode == "linearLight"_L1) blendModeInt = 14;
                else if (mode == "pinLight"_L1)    blendModeInt = 15;
                else if (mode == "hardMix"_L1)     blendModeInt = 16;
                else if (mode == "subtract"_L1)    blendModeInt = 17;
                else if (mode == "divide"_L1)      blendModeInt = 18;
                else if (mode == "darkerColor"_L1) blendModeInt = 19;
                else if (mode == "lighterColor"_L1) blendModeInt = 20;
                else if (mode == "hue"_L1)         blendModeInt = 21;
                else if (mode == "saturation"_L1)  blendModeInt = 22;
                else if (mode == "color"_L1)       blendModeInt = 23;
                else if (mode == "luminosity"_L1)  blendModeInt = 24;

                // ShaderEffect composites bg + fg using blend mode
                Element shader;
                shader.type = u"ShaderEffect"_s;
                shader.properties.insert(u"anchors.fill"_s, u"parent"_s);
                shader.properties.insert(u"property var source"_s, fgItem.id);
                shader.properties.insert(u"property var background"_s, bgItem.id);
                shader.properties.insert(u"property int blendMode"_s, blendModeInt);
                shader.properties.insert(u"fragmentShader"_s, u"\"blend.frag.qsb\""_s);

                accumulated.clear();
                accumulated.append(bgItem);
                accumulated.append(fgItem);
                accumulated.append(shader);

                m_needsBlendShader = true;
            }

            m_blendCounter++;
        } else {
            accumulated.append(child);
        }
    }

    element->children = accumulated;
}

void QPsdExporterQtQuickPlugin::applyAdjustmentLayer(const QPsdAbstractLayerItem *item, Element *parent, ImportData * /*imports*/) const
{
    const auto *adj = dynamic_cast<const QPsdAdjustmentLayerItem *>(item);
    if (!adj || parent->children.isEmpty())
        return;

    const QByteArray key = adj->adjustmentKey();
    const auto ali = item->record().additionalLayerInformation();
    // ALI data may be a QVariantMap (custom plugins) or QPsdDescriptor (v16descriptor plugin)
    QVariantMap data = ali.value(key).toMap();
    if (data.isEmpty() && ali.value(key).canConvert<QPsdDescriptor>()) {
        const auto desc = ali.value(key).value<QPsdDescriptor>();
        const auto hash = desc.data();
        for (auto it = hash.cbegin(); it != hash.cend(); ++it)
            data.insert(QString::fromLatin1(it.key()), it.value());
    }

    // Wrap all existing children into a source Item
    const QString sourceId = u"_adj_src_%1"_s.arg(m_adjustmentCounter++);
    Element wrapper;
    wrapper.type = "Item";
    wrapper.id = sourceId;
    wrapper.properties.insert("anchors.fill", "parent");
    wrapper.properties.insert("layer.enabled", true);
    wrapper.children = parent->children;
    parent->children.clear();

    // Build ShaderEffect as layer.effect
    Element effect;
    effect.type = "ShaderEffect";
    effect.properties.insert("fragmentShader", u"\"adjustment.frag.qsb\""_s);

    // Helper to set/override a float property
    auto setFloat = [&](const QString &name, qreal value) {
        effect.properties.insert(u"property real %1"_s.arg(name), value);
    };

    // Declare ALL shader uniforms with defaults to avoid "does not have a matching property" warnings
    effect.properties.insert("property int adjustmentType", -1);
    setFloat("brightness", 0);
    setFloat("contrast", 0);
    // Levels defaults (identity: shadow=0, highlight=1, midtone=1)
    for (const auto &prefix : {u"lvl"_s, u"lvlR"_s, u"lvlG"_s, u"lvlB"_s}) {
        setFloat(prefix + "_shadowIn"_L1, 0);
        setFloat(prefix + "_highlightIn"_L1, 1);
        setFloat(prefix + "_shadowOut"_L1, 0);
        setFloat(prefix + "_highlightOut"_L1, 1);
        setFloat(prefix + "_midtone"_L1, 1);
    }
    setFloat("exposure", 0); setFloat("offset", 0); setFloat("gamma", 1);
    setFloat("hueShift", 0); setFloat("saturationShift", 0); setFloat("lightnessShift", 0);
    setFloat("bal_shadow_cr", 0); setFloat("bal_shadow_mg", 0); setFloat("bal_shadow_yb", 0);
    setFloat("bal_mid_cr", 0); setFloat("bal_mid_mg", 0); setFloat("bal_mid_yb", 0);
    setFloat("bal_hi_cr", 0); setFloat("bal_hi_mg", 0); setFloat("bal_hi_yb", 0);
    setFloat("bal_preserveLum", 0);
    setFloat("phfl_r", 1); setFloat("phfl_g", 1); setFloat("phfl_b", 1);
    setFloat("phfl_density", 0); setFloat("phfl_preserveLum", 0);
    setFloat("posterizeLevels", 4); setFloat("thresholdLevel", 0.5);
    setFloat("vibrance", 0); setFloat("vibranceSat", 0);
    setFloat("mixr_outR_r", 100); setFloat("mixr_outR_g", 0); setFloat("mixr_outR_b", 0); setFloat("mixr_outR_c", 0);
    setFloat("mixr_outG_r", 0); setFloat("mixr_outG_g", 100); setFloat("mixr_outG_b", 0); setFloat("mixr_outG_c", 0);
    setFloat("mixr_outB_r", 0); setFloat("mixr_outB_g", 0); setFloat("mixr_outB_b", 100); setFloat("mixr_outB_c", 0);
    setFloat("mixr_mono", 0);
    setFloat("bw_red", 40); setFloat("bw_yellow", 60); setFloat("bw_green", 40);
    setFloat("bw_cyan", 60); setFloat("bw_blue", 20); setFloat("bw_magenta", 80);

    // LUT image for curves/gradient map — will be overridden if needed
    QString lutImageId;

    // Override with actual adjustment values
    if (key == "brit") {
        effect.properties.insert("property int adjustmentType", 0);
        setFloat("brightness", data.value(u"brightness"_s).toDouble() / 150.0);
        setFloat("contrast", data.value(u"contrast"_s).toDouble() / 100.0);
    } else if (key == "levl") {
        effect.properties.insert("property int adjustmentType", 1);
        auto setLevels = [&](const QString &prefix, const QVariantMap &ch) {
            setFloat(prefix + "_shadowIn"_L1, ch.value(u"shadowInput"_s).toDouble() / 255.0);
            setFloat(prefix + "_highlightIn"_L1, ch.value(u"highlightInput"_s).toDouble() / 255.0);
            setFloat(prefix + "_shadowOut"_L1, ch.value(u"shadowOutput"_s).toDouble() / 255.0);
            setFloat(prefix + "_highlightOut"_L1, ch.value(u"highlightOutput"_s).toDouble() / 255.0);
            double mid = ch.value(u"midtoneInput"_s, 100).toDouble();
            setFloat(prefix + "_midtone"_L1, mid / 100.0);
        };
        setLevels("lvl"_L1, data.value(u"rgb"_s).toMap());
        setLevels("lvlR"_L1, data.value(u"red"_s).toMap());
        setLevels("lvlG"_L1, data.value(u"green"_s).toMap());
        setLevels("lvlB"_L1, data.value(u"blue"_s).toMap());
    } else if (key == "curv") {
        effect.properties.insert("property int adjustmentType", 2);
        QImage lut(256, 1, QImage::Format_RGBA8888);
        auto buildLUT = [](const QVariantList &points) -> QVector<quint8> {
            QVector<quint8> table(256);
            if (points.isEmpty()) {
                for (int i = 0; i < 256; i++) table[i] = i;
                return table;
            }
            QVector<QPair<int, int>> pts;
            for (const auto &p : points) {
                auto m = p.toMap();
                pts.append({m.value(u"input"_s).toInt(), m.value(u"output"_s).toInt()});
            }
            if (pts.first().first != 0)
                pts.prepend({0, pts.first().second});
            if (pts.last().first != 255)
                pts.append({255, pts.last().second});
            int seg = 0;
            for (int i = 0; i < 256; i++) {
                while (seg < pts.size() - 2 && i > pts[seg + 1].first)
                    seg++;
                int x0 = pts[seg].first, y0 = pts[seg].second;
                int x1 = pts[seg + 1].first, y1 = pts[seg + 1].second;
                double t = (x1 != x0) ? double(i - x0) / (x1 - x0) : 0.0;
                table[i] = qBound(0, qRound(y0 + t * (y1 - y0)), 255);
            }
            return table;
        };
        auto rgbCurve = buildLUT(data.value(u"rgb"_s).toList());
        auto redCurve = buildLUT(data.value(u"red"_s).toList());
        auto greenCurve = buildLUT(data.value(u"green"_s).toList());
        auto blueCurve = buildLUT(data.value(u"blue"_s).toList());
        for (int i = 0; i < 256; i++) {
            auto *pixel = reinterpret_cast<quint8 *>(lut.scanLine(0)) + i * 4;
            pixel[0] = redCurve[i];
            pixel[1] = greenCurve[i];
            pixel[2] = blueCurve[i];
            pixel[3] = rgbCurve[i];
        }
        const QString lutName = imageStore.save(imageFileName(item->name() + "_lut"_L1, "PNG"_L1), lut, "PNG");
        lutImageId = u"_adj_lut_%1"_s.arg(m_adjustmentCounter - 1);
        Element lutItem;
        lutItem.type = "Image";
        lutItem.id = lutImageId;
        lutItem.properties.insert("source", u"\"images/%1\""_s.arg(lutName));
        lutItem.properties.insert("visible", false);
        parent->children.append(lutItem);
    } else if (key == "expA") {
        effect.properties.insert("property int adjustmentType", 3);
        setFloat("exposure", data.value(u"exposure"_s).toDouble());
        setFloat("offset", data.value(u"offset"_s).toDouble());
        setFloat("gamma", data.value(u"gamma"_s, 1.0).toDouble());
    } else if (key == "hue2") {
        effect.properties.insert("property int adjustmentType", 4);
        auto master = data.value(u"master"_s).toMap();
        setFloat("hueShift", master.value(u"hue"_s).toDouble());
        setFloat("saturationShift", master.value(u"saturation"_s).toDouble());
        setFloat("lightnessShift", master.value(u"lightness"_s).toDouble());
    } else if (key == "blnc") {
        effect.properties.insert("property int adjustmentType", 5);
        auto shadows = data.value(u"shadows"_s).toMap();
        auto midtones = data.value(u"midtones"_s).toMap();
        auto highlights = data.value(u"highlights"_s).toMap();
        setFloat("bal_shadow_cr", shadows.value(u"cyanRed"_s).toDouble());
        setFloat("bal_shadow_mg", shadows.value(u"magentaGreen"_s).toDouble());
        setFloat("bal_shadow_yb", shadows.value(u"yellowBlue"_s).toDouble());
        setFloat("bal_mid_cr", midtones.value(u"cyanRed"_s).toDouble());
        setFloat("bal_mid_mg", midtones.value(u"magentaGreen"_s).toDouble());
        setFloat("bal_mid_yb", midtones.value(u"yellowBlue"_s).toDouble());
        setFloat("bal_hi_cr", highlights.value(u"cyanRed"_s).toDouble());
        setFloat("bal_hi_mg", highlights.value(u"magentaGreen"_s).toDouble());
        setFloat("bal_hi_yb", highlights.value(u"yellowBlue"_s).toDouble());
        setFloat("bal_preserveLum", data.value(u"preserveLuminosity"_s).toBool() ? 1.0 : 0.0);
    } else if (key == "phfl") {
        effect.properties.insert("property int adjustmentType", 6);
        QColor c = data.value(u"color"_s).value<QColor>();
        if (!c.isValid()) c = QColor(255, 147, 0);
        setFloat("phfl_r", c.redF());
        setFloat("phfl_g", c.greenF());
        setFloat("phfl_b", c.blueF());
        setFloat("phfl_density", data.value(u"density"_s).toDouble() / 100.0);
        setFloat("phfl_preserveLum", data.value(u"preserveLuminosity"_s).toBool() ? 1.0 : 0.0);
    } else if (key == "nvrt") {
        effect.properties.insert("property int adjustmentType", 7);
    } else if (key == "post") {
        effect.properties.insert("property int adjustmentType", 8);
        setFloat("posterizeLevels", 4.0);
    } else if (key == "thrs") {
        effect.properties.insert("property int adjustmentType", 9);
        setFloat("thresholdLevel", 128.0 / 255.0);
    } else if (key == "mixr") {
        effect.properties.insert("property int adjustmentType", 11);
        auto mono = data.value(u"monochrome"_s).toBool();
        setFloat("mixr_mono", mono ? 1.0 : 0.0);
        auto red = data.value(u"red"_s).toMap();
        auto green = data.value(u"green"_s).toMap();
        auto blue = data.value(u"blue"_s).toMap();
        auto src = mono ? data.value(u"gray"_s).toMap() : red;
        setFloat("mixr_outR_r", src.value(u"red"_s).toDouble());
        setFloat("mixr_outR_g", src.value(u"green"_s).toDouble());
        setFloat("mixr_outR_b", src.value(u"blue"_s).toDouble());
        setFloat("mixr_outR_c", src.value(u"constant"_s).toDouble());
        if (!mono) {
            setFloat("mixr_outG_r", green.value(u"red"_s).toDouble());
            setFloat("mixr_outG_g", green.value(u"green"_s).toDouble());
            setFloat("mixr_outG_b", green.value(u"blue"_s).toDouble());
            setFloat("mixr_outG_c", green.value(u"constant"_s).toDouble());
            setFloat("mixr_outB_r", blue.value(u"red"_s).toDouble());
            setFloat("mixr_outB_g", blue.value(u"green"_s).toDouble());
            setFloat("mixr_outB_b", blue.value(u"blue"_s).toDouble());
            setFloat("mixr_outB_c", blue.value(u"constant"_s).toDouble());
        }
    } else if (key == "grdm") {
        effect.properties.insert("property int adjustmentType", 13);
        QImage lut(256, 1, QImage::Format_RGBA8888);
        lut.fill(Qt::black);
        auto stops = data.value(u"colorStops"_s).toList();
        if (!stops.isEmpty()) {
            struct Stop { double pos; QColor color; };
            QVector<Stop> gradStops;
            for (const auto &s : stops) {
                auto m = s.toMap();
                double loc = m.value(u"location"_s).toDouble() / 4096.0;
                QColor c = m.value(u"color"_s).value<QColor>();
                if (c.isValid())
                    gradStops.append({loc, c});
            }
            if (!gradStops.isEmpty()) {
                for (int i = 0; i < 256; i++) {
                    double t = i / 255.0;
                    int idx = 0;
                    while (idx < gradStops.size() - 1 && gradStops[idx + 1].pos < t)
                        idx++;
                    QColor c;
                    if (idx >= gradStops.size() - 1) {
                        c = gradStops.last().color;
                    } else {
                        double t0 = gradStops[idx].pos, t1 = gradStops[idx + 1].pos;
                        double f = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0;
                        const auto &c0 = gradStops[idx].color;
                        const auto &c1 = gradStops[idx + 1].color;
                        c = QColor::fromRgbF(
                            c0.redF() + f * (c1.redF() - c0.redF()),
                            c0.greenF() + f * (c1.greenF() - c0.greenF()),
                            c0.blueF() + f * (c1.blueF() - c0.blueF()));
                    }
                    auto *pixel = reinterpret_cast<quint8 *>(lut.scanLine(0)) + i * 4;
                    pixel[0] = c.red();
                    pixel[1] = c.green();
                    pixel[2] = c.blue();
                    pixel[3] = 255;
                }
            }
        }
        const QString lutName = imageStore.save(imageFileName(item->name() + "_lut"_L1, "PNG"_L1), lut, "PNG");
        lutImageId = u"_adj_lut_%1"_s.arg(m_adjustmentCounter - 1);
        Element lutItem;
        lutItem.type = "Image";
        lutItem.id = lutImageId;
        lutItem.properties.insert("source", u"\"images/%1\""_s.arg(lutName));
        lutItem.properties.insert("visible", false);
        parent->children.append(lutItem);
    } else if (key == "blwh") {
        effect.properties.insert("property int adjustmentType", 12);
        // Descriptor keys use 4-char codes: "Rd  ", "Yllw", "Grn ", "Cyn ", "Bl  ", "Mgnt"
        setFloat("bw_red", data.value(u"Rd  "_s).toDouble());
        setFloat("bw_yellow", data.value(u"Yllw"_s).toDouble());
        setFloat("bw_green", data.value(u"Grn "_s).toDouble());
        setFloat("bw_cyan", data.value(u"Cyn "_s).toDouble());
        setFloat("bw_blue", data.value(u"Bl  "_s).toDouble());
        setFloat("bw_magenta", data.value(u"Mgnt"_s).toDouble());
    } else if (key == "vibA") {
        effect.properties.insert("property int adjustmentType", 10);
        setFloat("vibrance", data.value(u"vibrance"_s).toDouble() / 100.0);
        setFloat("vibranceSat", data.value(u"Strt"_s).toDouble());
    } else {
        // Unknown adjustment type — skip
        parent->children = wrapper.children;
        return;
    }

    // Bind curvesLUT to the LUT image if one was created
    if (!lutImageId.isEmpty())
        effect.properties.insert("property var curvesLUT", lutImageId);

    wrapper.layers.append(effect);
    parent->children.append(wrapper);
    m_needsAdjustmentShader = true;
}

bool QPsdExporterQtQuickPlugin::outputText(const QModelIndex &textIndex, Element *element, ImportData *imports) const
{
    const QPsdTextLayerItem *text = dynamic_cast<const QPsdTextLayerItem *>(model()->layerItem(textIndex));
    if (!text) {
        qWarning() << "Invalid text layer item for index" << textIndex;
        return false;
    }
    const auto runs = text->runs();
    if (runs.isEmpty()) {
        element->type = "Text";
        if (!outputBase(textIndex, element, imports, text->bounds().toRect()))
            return false;
        element->properties.insert("text", "\"\"");
        element->properties.insert("color", "\"#000000\"");
        return true;
    }
    if (runs.size() == 1) {
        const auto run = runs.first();
        element->type = "Text";
        QRect rect = computeTextBounds(text);
        if (text->textType() == QPsdTextLayerItem::TextType::ParagraphText) {
            element->properties.insert("wrapMode"_L1, "Text.Wrap"_L1);
        }

        if (!outputBase(textIndex, element, imports, rect))
            return false;
        element->properties.insert("text", u"\"%1\""_s.arg(run.text.trimmed().replace("\n", "\\n")));
        element->properties.insert("font.family", u"\"%1\""_s.arg(run.font.family()));
        element->properties.insert("font.pixelSize", std::round(run.font.pointSizeF() * fontScaleFactor));
        {
            const int weight = run.font.weight();
            if (weight == QFont::Bold || run.fauxBold)
                element->properties.insert("font.bold", true);
            else if (weight != QFont::Normal && weight != 0)
                element->properties.insert("font.weight", weight);
        }
        if (run.font.italic() || run.fauxItalic)
            element->properties.insert("font.italic", true);
        if (run.underline)
            element->properties.insert("font.underline", true);
        if (run.strikethrough)
            element->properties.insert("font.strikeout", true);
        if (run.fontCaps == 1)
            element->properties.insert("font.capitalization", "Font.SmallCaps"_L1);
        else if (run.fontCaps == 2)
            element->properties.insert("font.capitalization", "Font.AllUppercase"_L1);
        if (run.font.letterSpacingType() == QFont::AbsoluteSpacing) {
            const qreal ls = run.font.letterSpacing();
            if (qAbs(ls) > 0.01)
                element->properties.insert("font.letterSpacing", ls * fontScaleFactor);
        } else if (run.font.letterSpacingType() == QFont::PercentageSpacing) {
            const qreal ls = run.font.letterSpacing();
            // Figma importer stores 100+value; Qt default is 0. Skip both "no change" cases.
            if (ls > 0.01 && qAbs(ls - 100.0) > 0.01)
                element->properties.insert("font.letterSpacing", ls - 100.0);
        }
        if (run.lineHeight > 0) {
            qreal ratio = run.lineHeight / run.font.pointSizeF();
            // Figma lineHeight is pre-multiplied by dpiScale for rendering; undo for QML export
            if (model()->fileInfo().suffix().compare("psd"_L1, Qt::CaseInsensitive)) {
                const qreal dpiScale = QGuiApplication::primaryScreen()
                    ? QGuiApplication::primaryScreen()->logicalDotsPerInchY() / 72.0 : 1.0;
                ratio /= dpiScale;
            }
            element->properties.insert("lineHeight", ratio);
        }
        element->properties.insert("color", u"\"%1\""_s.arg(run.color.name(QColor::HexArgb)));
        element->properties.insert("horizontalAlignment",
            horizontalAlignmentString(run.alignment, {"Text.AlignLeft"_L1, "Text.AlignRight"_L1, "Text.AlignHCenter"_L1, "Text.AlignJustify"_L1}));
        {
            const auto vAlign = verticalAlignmentString(run.alignment, {"Text.AlignTop"_L1, "Text.AlignBottom"_L1, "Text.AlignVCenter"_L1});
            if (!vAlign.isEmpty())
                element->properties.insert("verticalAlignment", vAlign);
        }
        element->properties.insert("clip", true);
    } else {
        element->type = "Item";
        QRect multiRect = computeTextBounds(text);
        if (!outputBase(textIndex, element, imports, multiRect))
            return false;
        element->properties.insert("clip", true);

        const bool isParagraph = text->textType() == QPsdTextLayerItem::TextType::ParagraphText;

        Element column;
        column.type = "Column";
        if (isParagraph) {
            column.properties.insert("anchors.fill", "parent");
        } else {
            column.properties.insert("anchors.centerIn", "parent");
        }

        imports->insert("QtQuick.Layouts");
        Element rowLayout;
        rowLayout.type = "RowLayout";
        rowLayout.properties.insert("spacing", 0);
        if (isParagraph) {
            rowLayout.properties.insert("width", "parent.width");
        } else {
            rowLayout.properties.insert("anchors.horizontalCenter", "parent.horizontalCenter");
        }

        for (const auto &run : runs) {
            const auto texts = run.text.trimmed().split("\n");
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
                textElement.properties.insert("text", u"\"%1\""_s.arg(text));
                textElement.properties.insert("font.family", u"\"%1\""_s.arg(run.font.family()));
                textElement.properties.insert("font.pixelSize", std::round(run.font.pointSizeF() * fontScaleFactor));
                {
                    const int weight = run.font.weight();
                    if (weight != QFont::Normal && weight != 0)
                        textElement.properties.insert("font.weight", weight);
                }
                if (run.fauxBold)
                    textElement.properties.insert("font.bold", true);
                if (run.font.italic() || run.fauxItalic)
                    textElement.properties.insert("font.italic", true);
                if (run.underline)
                    textElement.properties.insert("font.underline", true);
                if (run.strikethrough)
                    textElement.properties.insert("font.strikeout", true);
                if (run.fontCaps == 1)
                    textElement.properties.insert("font.capitalization", "Font.SmallCaps"_L1);
                else if (run.fontCaps == 2)
                    textElement.properties.insert("font.capitalization", "Font.AllUppercase"_L1);
                if (run.font.letterSpacingType() == QFont::AbsoluteSpacing) {
                    const qreal ls = run.font.letterSpacing();
                    if (qAbs(ls) > 0.01)
                        textElement.properties.insert("font.letterSpacing", ls * fontScaleFactor);
                } else if (run.font.letterSpacingType() == QFont::PercentageSpacing) {
                    const qreal ls = run.font.letterSpacing();
                    if (ls > 0.01 && qAbs(ls - 100.0) > 0.01)
                        textElement.properties.insert("font.letterSpacing", ls - 100.0);
                }
                if (run.lineHeight > 0) {
                    qreal ratio = run.lineHeight / run.font.pointSizeF();
                    if (model()->fileInfo().suffix().compare("psd"_L1, Qt::CaseInsensitive)) {
                        const qreal dpiScale = QGuiApplication::primaryScreen()
                            ? QGuiApplication::primaryScreen()->logicalDotsPerInchY() / 72.0 : 1.0;
                        ratio /= dpiScale;
                    }
                    textElement.properties.insert("lineHeight", ratio);
                }
                textElement.properties.insert("color", u"\"%1\""_s.arg(run.color.name(QColor::HexArgb)));
                textElement.properties.insert("Layout.fillHeight", true);
                if (isParagraph) {
                    textElement.properties.insert("Layout.fillWidth", true);
                    textElement.properties.insert("wrapMode"_L1, "Text.Wrap"_L1);
                }
                textElement.properties.insert("horizontalAlignment",
                    horizontalAlignmentString(run.alignment, {"Text.AlignLeft"_L1, "Text.AlignRight"_L1, "Text.AlignHCenter"_L1, "Text.AlignJustify"_L1}));
                {
                    const auto vAlign = verticalAlignmentString(run.alignment, {"Text.AlignTop"_L1, "Text.AlignBottom"_L1, "Text.AlignVCenter"_L1});
                    if (!vAlign.isEmpty())
                        textElement.properties.insert("verticalAlignment", vAlign);
                }
                rowLayout.children.append(textElement);
            }
        }
        column.children.append(rowLayout);
        element->children.append(column);
    }
    return true;
}

bool QPsdExporterQtQuickPlugin::outputShape(const QModelIndex &shapeIndex, Element *element, ImportData *imports) const
{
    const QPsdShapeLayerItem *shape = dynamic_cast<const QPsdShapeLayerItem *>(model()->layerItem(shapeIndex));
    const auto path = shape->pathInfo();
    switch (path.type) {
    case QPsdAbstractLayerItem::PathInfo::None: {
        // Fill layers (GdFl, SoCo without vector mask) cover the entire layer rect
        const QGradient *g = effectiveGradient(shape);
        if (g) {
            switch (g->type()) {
            case QGradient::LinearGradient: {
                const auto linear = reinterpret_cast<const QLinearGradient *>(g);
                const QTransform brushTransform = shape->brush().transform();
                const QPointF gradStart = brushTransform.map(linear->start());
                const QPointF gradEnd = brushTransform.map(linear->finalStop());
                const bool simpleVertical = qFuzzyCompare(gradStart.x(), gradEnd.x());
                const bool simpleHorizontal = qFuzzyCompare(gradStart.y(), gradEnd.y());

                if (simpleVertical || simpleHorizontal) {
                    // Gradient position 0=top/left, 1=bottom/right; invert if PSD direction is reversed
                    const bool reversed = simpleHorizontal
                        ? (gradStart.x() > gradEnd.x())
                        : (gradStart.y() > gradEnd.y());
                    Element gradient;
                    gradient.type = "gradient: Gradient";
                    if (simpleHorizontal)
                        gradient.properties.insert("orientation", "Gradient.Horizontal"_L1);
                    for (const auto &stop : linear->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", reversed ? 1.0 - stop.first : stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                        gradient.children.append(stopElement);
                    }
                    element->type = "Rectangle";
                    if (!outputBase(shapeIndex, element, imports))
                        return false;
                    element->children.append(gradient);
                } else if (effectMode() == Qt5Effects) {
                    imports->insert("Qt5Compat.GraphicalEffects as GE");
                    Element effect;
                    effect.type = "GE.LinearGradient";
                    if (!outputBase(shapeIndex, &effect, imports))
                        return false;
                    effect.properties.insert("start", QPointF(gradStart.x() * horizontalScale, gradStart.y() * verticalScale));
                    effect.properties.insert("end", QPointF(gradEnd.x() * horizontalScale, gradEnd.y() * verticalScale));
                    Element effectGradient;
                    effectGradient.type = "gradient: Gradient";
                    for (const auto &stop : linear->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                        effectGradient.children.append(stopElement);
                    }
                    effect.children.append(effectGradient);
                    *element = effect;
                } else {
                    imports->insert("QtQuick.Shapes");
                    Element shapeElem;
                    shapeElem.type = "Shape";
                    if (!outputBase(shapeIndex, &shapeElem, imports))
                        return false;
                    Element shapePath;
                    shapePath.type = "ShapePath";
                    shapePath.properties.insert("strokeWidth", 0);
                    shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                    Element fillGrad;
                    fillGrad.type = "fillGradient: LinearGradient";
                    fillGrad.properties.insert("x1", gradStart.x() * horizontalScale);
                    fillGrad.properties.insert("y1", gradStart.y() * verticalScale);
                    fillGrad.properties.insert("x2", gradEnd.x() * horizontalScale);
                    fillGrad.properties.insert("y2", gradEnd.y() * verticalScale);
                    for (const auto &stop : linear->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                        fillGrad.children.append(stopElement);
                    }
                    shapePath.children.append(fillGrad);
                    const QRect shapeRect = computeBaseRect(shapeIndex);
                    const qreal w = shapeRect.width() * horizontalScale;
                    const qreal h = shapeRect.height() * verticalScale;
                    Element startPath;
                    startPath.type = "PathMove";
                    startPath.properties.insert("x", 0);
                    startPath.properties.insert("y", 0);
                    shapePath.children.append(startPath);
                    Element line1; line1.type = "PathLine"; line1.properties.insert("x", w); line1.properties.insert("y", 0);
                    Element line2; line2.type = "PathLine"; line2.properties.insert("x", w); line2.properties.insert("y", h);
                    Element line3; line3.type = "PathLine"; line3.properties.insert("x", 0); line3.properties.insert("y", h);
                    Element line4; line4.type = "PathLine"; line4.properties.insert("x", 0); line4.properties.insert("y", 0);
                    shapePath.children.append(line1);
                    shapePath.children.append(line2);
                    shapePath.children.append(line3);
                    shapePath.children.append(line4);
                    shapeElem.children.append(shapePath);
                    *element = shapeElem;
                }
                break; }
            case QGradient::RadialGradient: {
                const auto radial = reinterpret_cast<const QRadialGradient *>(g);
                if (effectMode() == Qt5Effects) {
                    imports->insert("Qt5Compat.GraphicalEffects as GE");
                    Element effect;
                    effect.type = "GE.RadialGradient";
                    if (!outputBase(shapeIndex, &effect, imports))
                        return false;
                    const QRect shapeRect = computeBaseRect(shapeIndex);
                    const qreal elemW = shapeRect.width() * horizontalScale;
                    const qreal elemH = shapeRect.height() * verticalScale;
                    effect.properties.insert("horizontalOffset", radial->center().x() * horizontalScale - elemW / 2);
                    effect.properties.insert("verticalOffset", radial->center().y() * verticalScale - elemH / 2);
                    effect.properties.insert("horizontalRadius", radial->radius() * horizontalScale);
                    effect.properties.insert("verticalRadius", radial->radius() * verticalScale);
                    Element gradient;
                    gradient.type = "gradient: Gradient";
                    for (const auto &stop : radial->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                        gradient.children.append(stopElement);
                    }
                    effect.children.append(gradient);
                    *element = effect;
                } else {
                    imports->insert("QtQuick.Shapes");
                    Element shapeElem;
                    shapeElem.type = "Shape";
                    if (!outputBase(shapeIndex, &shapeElem, imports))
                        return false;
                    Element shapePath;
                    shapePath.type = "ShapePath";
                    shapePath.properties.insert("strokeWidth", 0);
                    shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                    Element fillGrad;
                    fillGrad.type = "fillGradient: RadialGradient";
                    fillGrad.properties.insert("centerX", radial->center().x() * horizontalScale);
                    fillGrad.properties.insert("centerY", radial->center().y() * verticalScale);
                    fillGrad.properties.insert("centerRadius", radial->radius() * std::max(horizontalScale, verticalScale));
                    fillGrad.properties.insert("focalX", radial->focalPoint().x() * horizontalScale);
                    fillGrad.properties.insert("focalY", radial->focalPoint().y() * verticalScale);
                    fillGrad.properties.insert("focalRadius", radial->focalRadius() * std::max(horizontalScale, verticalScale));
                    for (const auto &stop : radial->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                        fillGrad.children.append(stopElement);
                    }
                    shapePath.children.append(fillGrad);
                    const QRect shapeRect = computeBaseRect(shapeIndex);
                    const qreal w = shapeRect.width() * horizontalScale;
                    const qreal h = shapeRect.height() * verticalScale;
                    Element startPath;
                    startPath.type = "PathMove";
                    startPath.properties.insert("x", 0);
                    startPath.properties.insert("y", 0);
                    shapePath.children.append(startPath);
                    Element line1; line1.type = "PathLine"; line1.properties.insert("x", w); line1.properties.insert("y", 0);
                    Element line2; line2.type = "PathLine"; line2.properties.insert("x", w); line2.properties.insert("y", h);
                    Element line3; line3.type = "PathLine"; line3.properties.insert("x", 0); line3.properties.insert("y", h);
                    Element line4; line4.type = "PathLine"; line4.properties.insert("x", 0); line4.properties.insert("y", 0);
                    shapePath.children.append(line1);
                    shapePath.children.append(line2);
                    shapePath.children.append(line3);
                    shapePath.children.append(line4);
                    shapeElem.children.append(shapePath);
                    *element = shapeElem;
                }
                break; }
            case QGradient::ConicalGradient: {
                const auto conical = reinterpret_cast<const QConicalGradient *>(g);
                imports->insert("QtQuick.Shapes");
                Element shapeElem;
                shapeElem.type = "Shape";
                if (!outputBase(shapeIndex, &shapeElem, imports))
                    return false;
                Element shapePath;
                shapePath.type = "ShapePath";
                shapePath.properties.insert("strokeWidth", 0);
                shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                Element fillGrad;
                fillGrad.type = "fillGradient: ConicalGradient";
                fillGrad.properties.insert("centerX", conical->center().x() * horizontalScale);
                fillGrad.properties.insert("centerY", conical->center().y() * verticalScale);
                fillGrad.properties.insert("angle", conical->angle());
                for (const auto &stop : conical->stops()) {
                    Element stopElement;
                    stopElement.type = "GradientStop";
                    stopElement.properties.insert("position", stop.first);
                    stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                    fillGrad.children.append(stopElement);
                }
                shapePath.children.append(fillGrad);
                const QRect shapeRect = computeBaseRect(shapeIndex);
                const qreal w = shapeRect.width() * horizontalScale;
                const qreal h = shapeRect.height() * verticalScale;
                Element startPath;
                startPath.type = "PathMove";
                startPath.properties.insert("x", 0);
                startPath.properties.insert("y", 0);
                shapePath.children.append(startPath);
                Element line1; line1.type = "PathLine"; line1.properties.insert("x", w); line1.properties.insert("y", 0);
                Element line2; line2.type = "PathLine"; line2.properties.insert("x", w); line2.properties.insert("y", h);
                Element line3; line3.type = "PathLine"; line3.properties.insert("x", 0); line3.properties.insert("y", h);
                Element line4; line4.type = "PathLine"; line4.properties.insert("x", 0); line4.properties.insert("y", 0);
                shapePath.children.append(line1);
                shapePath.children.append(line2);
                shapePath.children.append(line3);
                shapePath.children.append(line4);
                shapeElem.children.append(shapePath);
                *element = shapeElem;
                break; }
            default: {
                // Unsupported gradient type (diamond, reflected, etc.) — rasterize to image
                const QString name = saveLayerImage(shape);
                if (!name.isEmpty()) {
                    element->type = "Image";
                    if (!outputBase(shapeIndex, element, imports))
                        return false;
                    element->properties.insert("source", u"\"images/%1\""_s.arg(name));
                    element->properties.insert("fillMode", "Image.PreserveAspectFit");
                }
                break; }
            }
        } else if (shape->patternFill() || !shape->vscgPatternId().isEmpty()) {
            // Pattern fill — render to image
            auto *guiModel = static_cast<const QPsdExporterTreeItemModel *>(model())->guiLayerTreeItemModel();
            const auto *pf = shape->patternFill();
            const QString patternId = pf ? pf->patternID() : shape->vscgPatternId();
            QImage patternImage = guiModel ? guiModel->patternImage(patternId) : QImage();
            if (!patternImage.isNull()) {
                const qreal sc = pf ? pf->scale() / 100.0 : shape->vscgPatternScale();
                if (sc != 1.0 && sc > 0) {
                    patternImage = patternImage.scaled(
                        qRound(patternImage.width() * sc),
                        qRound(patternImage.height() * sc),
                        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                }
                QBrush brush(patternImage);
                QTransform transform;
                const qreal angle = pf ? pf->angle() : shape->vscgPatternAngle();
                if (angle != 0)
                    transform.rotate(angle);
                if (pf) {
                    const QPointF phase = pf->phase();
                    if (!phase.isNull())
                        transform.translate(phase.x(), phase.y());
                }
                if (!transform.isIdentity())
                    brush.setTransform(transform);

                const QRect shapeRect = computeBaseRect(shapeIndex);
                QImage rendered(shapeRect.size(), QImage::Format_ARGB32);
                rendered.fill(Qt::transparent);
                QPainter painter(&rendered);
                painter.setPen(Qt::NoPen);
                painter.setBrush(brush);
                painter.drawRect(rendered.rect());
                painter.end();

                const QString name = imageStore.save(imageFileName(shape->name(), "PNG"_L1), rendered, "PNG");
                if (!name.isEmpty()) {
                    element->type = "Image";
                    if (!outputBase(shapeIndex, element, imports))
                        return false;
                    element->properties.insert("source", u"\"images/%1\""_s.arg(name));
                    element->properties.insert("fillMode", "Image.PreserveAspectFit");
                }
            }
        } else if (shape->brush() != Qt::NoBrush) {
            element->type = "Rectangle";
            if (!outputBase(shapeIndex, element, imports))
                return false;
            element->properties.insert("color", u"\"%1\""_s.arg(shape->brush().color().name(QColor::HexArgb)));
        }
        break; }
    case QPsdAbstractLayerItem::PathInfo::Rectangle: {
        bool filled = isFilledRect(path, shape);
        if (!filled) {
            element->type = "Item";
            if (!outputBase(shapeIndex, element, imports))
                return false;
        }

        const QGradient *g = effectiveGradient(shape);
        if (g) {
            switch (g->type()) {
            case QGradient::LinearGradient: {
                const auto linear = reinterpret_cast<const QLinearGradient *>(g);
                const QTransform brushTransform = shape->brush().transform();
                const QPointF gradStart = brushTransform.map(linear->start());
                const QPointF gradEnd = brushTransform.map(linear->finalStop());
                const bool simpleVertical = qFuzzyCompare(gradStart.x(), gradEnd.x());
                const bool simpleHorizontal = qFuzzyCompare(gradStart.y(), gradEnd.y());

                if (simpleVertical || simpleHorizontal) {
                    const bool reversed = simpleHorizontal
                        ? (gradStart.x() > gradEnd.x())
                        : (gradStart.y() > gradEnd.y());
                    Element gradient;
                    gradient.type = "gradient: Gradient";
                    if (simpleHorizontal)
                        gradient.properties.insert("orientation", "Gradient.Horizontal"_L1);
                    for (const auto &stop : linear->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", reversed ? 1.0 - stop.first : stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                        gradient.children.append(stopElement);
                    }
                    element->type = "Rectangle";
                    element->children.append(gradient);
                } else if (effectMode() == Qt5Effects) {
                    imports->insert("Qt5Compat.GraphicalEffects as GE");
                    Element effect;
                    effect.type = "GE.LinearGradient";
                    if (filled) {
                        if (!outputBase(shapeIndex, &effect, imports))
                            return false;
                    } else {
                        outputRect(path.rect, &effect);
                    }
                    effect.properties.insert("start", QPointF(gradStart.x() * horizontalScale, gradStart.y() * verticalScale));
                    effect.properties.insert("end", QPointF(gradEnd.x() * horizontalScale, gradEnd.y() * verticalScale));
                    Element effectGradient;
                    effectGradient.type = "gradient: Gradient";
                    for (const auto &stop : linear->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                        effectGradient.children.append(stopElement);
                    }
                    effect.children.append(effectGradient);
                    if (filled)
                        *element = effect;
                    else
                        element->children.prepend(effect);
                } else {
                    imports->insert("QtQuick.Shapes");
                    Element shapeElem;
                    shapeElem.type = "Shape";
                    if (filled) {
                        if (!outputBase(shapeIndex, &shapeElem, imports))
                            return false;
                    } else {
                        outputRect(path.rect, &shapeElem);
                    }
                    Element shapePath;
                    shapePath.type = "ShapePath";
                    shapePath.properties.insert("strokeWidth", 0);
                    shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                    Element fillGrad;
                    fillGrad.type = "fillGradient: LinearGradient";
                    fillGrad.properties.insert("x1", gradStart.x() * horizontalScale);
                    fillGrad.properties.insert("y1", gradStart.y() * verticalScale);
                    fillGrad.properties.insert("x2", gradEnd.x() * horizontalScale);
                    fillGrad.properties.insert("y2", gradEnd.y() * verticalScale);
                    for (const auto &stop : linear->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                        fillGrad.children.append(stopElement);
                    }
                    shapePath.children.append(fillGrad);
                    // Rectangle path for the shape
                    const qreal w = path.rect.width() * horizontalScale;
                    const qreal h = path.rect.height() * verticalScale;
                    Element startPath;
                    startPath.type = "PathMove";
                    startPath.properties.insert("x", 0);
                    startPath.properties.insert("y", 0);
                    shapePath.children.append(startPath);
                    Element line1; line1.type = "PathLine"; line1.properties.insert("x", w); line1.properties.insert("y", 0);
                    Element line2; line2.type = "PathLine"; line2.properties.insert("x", w); line2.properties.insert("y", h);
                    Element line3; line3.type = "PathLine"; line3.properties.insert("x", 0); line3.properties.insert("y", h);
                    Element line4; line4.type = "PathLine"; line4.properties.insert("x", 0); line4.properties.insert("y", 0);
                    shapePath.children.append(line1);
                    shapePath.children.append(line2);
                    shapePath.children.append(line3);
                    shapePath.children.append(line4);
                    shapeElem.children.append(shapePath);
                    if (filled)
                        *element = shapeElem;
                    else
                        element->children.prepend(shapeElem);
                }

                break; }
            case QGradient::RadialGradient: {
                const auto radial = reinterpret_cast<const QRadialGradient *>(g);
                if (effectMode() == Qt5Effects) {
                    imports->insert("Qt5Compat.GraphicalEffects as GE");
                    Element effect;
                    effect.type = "GE.RadialGradient";
                    if (filled) {
                        if (!outputBase(shapeIndex, &effect, imports))
                            return false;
                    } else {
                        outputRect(path.rect, &effect);
                    }
                    const qreal elemW = path.rect.width() * horizontalScale;
                    const qreal elemH = path.rect.height() * verticalScale;
                    effect.properties.insert("horizontalOffset", radial->center().x() * horizontalScale - elemW / 2);
                    effect.properties.insert("verticalOffset", radial->center().y() * verticalScale - elemH / 2);
                    effect.properties.insert("horizontalRadius", radial->radius() * horizontalScale);
                    effect.properties.insert("verticalRadius", radial->radius() * verticalScale);
                    Element gradient;
                    gradient.type = "gradient: Gradient";
                    for (const auto &stop : radial->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                        gradient.children.append(stopElement);
                    }
                    effect.children.append(gradient);
                    if (filled)
                        *element = effect;
                    else
                        element->children.prepend(effect);
                } else {
                    imports->insert("QtQuick.Shapes");
                    Element shapeElem;
                    shapeElem.type = "Shape";
                    if (filled) {
                        if (!outputBase(shapeIndex, &shapeElem, imports))
                            return false;
                    } else {
                        outputRect(path.rect, &shapeElem);
                    }
                    Element shapePath;
                    shapePath.type = "ShapePath";
                    shapePath.properties.insert("strokeWidth", 0);
                    shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                    Element fillGrad;
                    fillGrad.type = "fillGradient: RadialGradient";
                    fillGrad.properties.insert("centerX", radial->center().x() * horizontalScale);
                    fillGrad.properties.insert("centerY", radial->center().y() * verticalScale);
                    fillGrad.properties.insert("centerRadius", radial->radius() * std::max(horizontalScale, verticalScale));
                    fillGrad.properties.insert("focalX", radial->focalPoint().x() * horizontalScale);
                    fillGrad.properties.insert("focalY", radial->focalPoint().y() * verticalScale);
                    fillGrad.properties.insert("focalRadius", radial->focalRadius() * std::max(horizontalScale, verticalScale));
                    for (const auto &stop : radial->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                        fillGrad.children.append(stopElement);
                    }
                    shapePath.children.append(fillGrad);
                    // Rectangle path for the shape
                    const qreal w = path.rect.width() * horizontalScale;
                    const qreal h = path.rect.height() * verticalScale;
                    Element startPath;
                    startPath.type = "PathMove";
                    startPath.properties.insert("x", 0);
                    startPath.properties.insert("y", 0);
                    shapePath.children.append(startPath);
                    Element line1; line1.type = "PathLine"; line1.properties.insert("x", w); line1.properties.insert("y", 0);
                    Element line2; line2.type = "PathLine"; line2.properties.insert("x", w); line2.properties.insert("y", h);
                    Element line3; line3.type = "PathLine"; line3.properties.insert("x", 0); line3.properties.insert("y", h);
                    Element line4; line4.type = "PathLine"; line4.properties.insert("x", 0); line4.properties.insert("y", 0);
                    shapePath.children.append(line1);
                    shapePath.children.append(line2);
                    shapePath.children.append(line3);
                    shapePath.children.append(line4);
                    shapeElem.children.append(shapePath);
                    if (filled)
                        *element = shapeElem;
                    else
                        element->children.prepend(shapeElem);
                }
                break; }
            case QGradient::ConicalGradient: {
                const auto conical = reinterpret_cast<const QConicalGradient *>(g);
                imports->insert("QtQuick.Shapes");
                Element shapeElem;
                shapeElem.type = "Shape";
                if (filled) {
                    if (!outputBase(shapeIndex, &shapeElem, imports))
                        return false;
                } else {
                    outputRect(path.rect, &shapeElem);
                }
                Element shapePath;
                shapePath.type = "ShapePath";
                shapePath.properties.insert("strokeWidth", 0);
                shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                Element fillGrad;
                fillGrad.type = "fillGradient: ConicalGradient";
                fillGrad.properties.insert("centerX", conical->center().x() * horizontalScale);
                fillGrad.properties.insert("centerY", conical->center().y() * verticalScale);
                fillGrad.properties.insert("angle", conical->angle());
                for (const auto &stop : conical->stops()) {
                    Element stopElement;
                    stopElement.type = "GradientStop";
                    stopElement.properties.insert("position", stop.first);
                    stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                    fillGrad.children.append(stopElement);
                }
                shapePath.children.append(fillGrad);
                const qreal w = path.rect.width() * horizontalScale;
                const qreal h = path.rect.height() * verticalScale;
                Element startPath;
                startPath.type = "PathMove";
                startPath.properties.insert("x", 0);
                startPath.properties.insert("y", 0);
                shapePath.children.append(startPath);
                Element line1; line1.type = "PathLine"; line1.properties.insert("x", w); line1.properties.insert("y", 0);
                Element line2; line2.type = "PathLine"; line2.properties.insert("x", w); line2.properties.insert("y", h);
                Element line3; line3.type = "PathLine"; line3.properties.insert("x", 0); line3.properties.insert("y", h);
                Element line4; line4.type = "PathLine"; line4.properties.insert("x", 0); line4.properties.insert("y", 0);
                shapePath.children.append(line1);
                shapePath.children.append(line2);
                shapePath.children.append(line3);
                shapePath.children.append(line4);
                shapeElem.children.append(shapePath);
                if (filled)
                    *element = shapeElem;
                else
                    element->children.prepend(shapeElem);
                break; }
            default:
                qWarning() << "Unsupported gradient type"_L1 << g->type();
                break;
            }
        } else {
            const auto &pen = shape->pen();
            bool needsShapePath = pen.style() != Qt::NoPen
                && (pen.style() != Qt::SolidLine || pen.brush().gradient());
            if (needsShapePath) {
                // Dashed or gradient stroke: use Shape+ShapePath (Rectangle.border can't do these)
                imports->insert("QtQuick.Shapes");
                Element shapeElem;
                shapeElem.type = "Shape";
                qreal dw = computeStrokeWidth(pen, unitScale);
                QRectF strokeRect = adjustRectForStroke(path.rect, shape->strokeAlignment(), dw);
                if (filled) {
                    if (!outputBase(shapeIndex, &shapeElem, imports))
                        return false;
                } else {
                    outputRect(strokeRect, &shapeElem);
                }
                Element shapePath;
                shapePath.type = "ShapePath";
                shapePath.properties.insert("strokeWidth", dw);
                if (pen.brush().gradient()) {
                    const auto stops = pen.brush().gradient()->stops();
                    if (!stops.isEmpty())
                        shapePath.properties.insert("strokeColor", u"\"%1\""_s.arg(stops.first().second.name(QColor::HexArgb)));
                    else
                        shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                } else {
                    shapePath.properties.insert("strokeColor", u"\"%1\""_s.arg(pen.color().name(QColor::HexArgb)));
                }
                if (!pen.dashPattern().isEmpty() && pen.style() != Qt::SolidLine) {
                    QStringList dashValues;
                    for (qreal d : pen.dashPattern())
                        dashValues.append(QString::number(d));
                    shapePath.properties.insert("dashPattern", u"[%1]"_s.arg(dashValues.join(u", "_s)));
                }
                if (pen.capStyle() == Qt::RoundCap)
                    shapePath.properties.insert("capStyle", u"ShapePath.RoundCap"_s);
                else if (pen.capStyle() == Qt::SquareCap)
                    shapePath.properties.insert("capStyle", u"ShapePath.SquareCap"_s);
                if (pen.joinStyle() == Qt::RoundJoin)
                    shapePath.properties.insert("joinStyle", u"ShapePath.RoundJoin"_s);
                else if (pen.joinStyle() == Qt::BevelJoin)
                    shapePath.properties.insert("joinStyle", u"ShapePath.BevelJoin"_s);
                if (shape->brush() != Qt::NoBrush)
                    shapePath.properties.insert("fillColor", u"\"%1\""_s.arg(shape->brush().color().name(QColor::HexArgb)));
                else
                    shapePath.properties.insert("fillColor", u"\"transparent\""_s);
                const qreal w = strokeRect.width() * horizontalScale;
                const qreal h = strokeRect.height() * verticalScale;
                Element startPath; startPath.type = "PathMove";
                startPath.properties.insert("x", 0); startPath.properties.insert("y", 0);
                shapePath.children.append(startPath);
                Element line1; line1.type = "PathLine"; line1.properties.insert("x", w); line1.properties.insert("y", 0);
                Element line2; line2.type = "PathLine"; line2.properties.insert("x", w); line2.properties.insert("y", h);
                Element line3; line3.type = "PathLine"; line3.properties.insert("x", 0); line3.properties.insert("y", h);
                Element line4; line4.type = "PathLine"; line4.properties.insert("x", 0); line4.properties.insert("y", 0);
                shapePath.children.append(line1);
                shapePath.children.append(line2);
                shapePath.children.append(line3);
                shapePath.children.append(line4);
                shapeElem.children.append(shapePath);
                if (filled)
                    *element = shapeElem;
                else
                    element->children.prepend(shapeElem);
            } else if (shape->brush().style() == Qt::TexturePattern) {
                // Composited fill (e.g. solid + gradient): save as image
                QImage texImage = shape->brush().textureImage();
                if (imageScaling)
                    texImage = texImage.scaled(path.rect.width() * horizontalScale, path.rect.height() * verticalScale, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QString name = imageStore.save(imageFileName(shape->name(), "PNG"_L1), texImage, "PNG");
                Element imgElement;
                imgElement.type = "Image";
                if (filled) {
                    if (!outputBase(shapeIndex, &imgElement, imports))
                        return false;
                } else {
                    outputRect(path.rect, &imgElement);
                }
                imgElement.properties.insert("source", u"\"images/%1\""_s.arg(name));
                imgElement.properties.insert("fillMode", "Image.Stretch");
                if (filled)
                    *element = imgElement;
                else
                    element->children.append(imgElement);
            } else {
                Element rectElement;
                rectElement.type = "Rectangle";
                if (filled) {
                    if (!outputBase(shapeIndex, &rectElement, imports))
                        return false;
                } else {
                    outputRect(path.rect, &rectElement);
                }
                if (pen.style() != Qt::NoPen) {
                    qreal dw = computeStrokeWidth(pen, unitScale);
                    outputRect(adjustRectForStroke(path.rect, shape->strokeAlignment(), dw), &rectElement);
                    rectElement.properties.insert("border.width", dw);
                    rectElement.properties.insert("border.color", u"\"%1\""_s.arg(pen.color().name(QColor::HexArgb)));
                }
                if (shape->brush() != Qt::NoBrush)
                    rectElement.properties.insert("color", u"\"%1\""_s.arg(shape->brush().color().name(QColor::HexArgb)));
                else
                    rectElement.properties.insert("color", "\"transparent\"");
                if (filled)
                    *element = rectElement;
                else
                    element->children.append(rectElement);
            }
        }
        break; }
    case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
        // Ellipse detection: if radius > half the smaller dimension and non-square,
        // QML Rectangle+radius produces a stadium shape. Use Shape+PathAngleArc instead.
        {
            // No qFuzzyCompare() here, which is too strict to test visual distinction.
            constexpr qreal epsilon = 0.1;
            const qreal minDim = qMin(path.rect.width(), path.rect.height());
            if (path.radius * 2 > minDim + epsilon
                && qAbs(path.rect.width() - path.rect.height()) > epsilon) {
                imports->insert("QtQuick.Shapes");
                element->type = "Shape";
                if (!outputBase(shapeIndex, element, imports))
                    return false;
                Element shapePath;
                shapePath.type = "ShapePath";
                // Fill
                const QGradient *eg = effectiveGradient(shape);
                if (eg) {
                    // TODO: gradient fill for ellipses
                    shapePath.properties.insert("fillColor", u"\"%1\""_s.arg(shape->brush().color().name(QColor::HexArgb)));
                } else if (shape->brush() != Qt::NoBrush) {
                    shapePath.properties.insert("fillColor", u"\"%1\""_s.arg(shape->brush().color().name(QColor::HexArgb)));
                }
                // Stroke
                const auto &pen = shape->pen();
                if (pen.style() == Qt::NoPen) {
                    shapePath.properties.insert("strokeWidth", 0);
                    shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                } else {
                    shapePath.properties.insert("strokeWidth", pen.widthF() * unitScale);
                    shapePath.properties.insert("strokeColor", u"\"%1\""_s.arg(pen.color().name(QColor::HexArgb)));
                }
                // Ellipse arc
                const qreal cx = path.rect.center().x() * horizontalScale;
                const qreal cy = path.rect.center().y() * verticalScale;
                const qreal rx = path.rect.width() * horizontalScale / 2.0;
                const qreal ry = path.rect.height() * verticalScale / 2.0;
                Element arc;
                arc.type = "PathAngleArc";
                arc.properties.insert("centerX", cx);
                arc.properties.insert("centerY", cy);
                arc.properties.insert("radiusX", rx);
                arc.properties.insert("radiusY", ry);
                arc.properties.insert("startAngle", 0);
                arc.properties.insert("sweepAngle", 360);
                shapePath.children.append(arc);
                element->children.append(shapePath);
                return true;
            }
        }
        bool filled = isFilledRect(path, shape);
        bool gradientHandled = false;
        Element rectElement;
        rectElement.type = "Rectangle";
        if (filled) {
            if (!outputBase(shapeIndex, &rectElement, imports))
                return false;
        } else {
            element->type = "Item";
            if (!outputBase(shapeIndex, element, imports))
                return false;
            outputRect(path.rect, &rectElement);
        }
        rectElement.properties.insert("radius", path.radius * unitScale);

        const QGradient *g = effectiveGradient(shape);
        if (g) {
            switch (g->type()) {
            case QGradient::LinearGradient: {
                const auto linear = reinterpret_cast<const QLinearGradient *>(g);
                const QTransform brushTransform = shape->brush().transform();
                const QPointF gradStart = brushTransform.map(linear->start());
                const QPointF gradEnd = brushTransform.map(linear->finalStop());
                bool simpleVertical = qFuzzyCompare(gradStart.x(), gradEnd.x());
                Element gradient;
                gradient.type = "gradient: Gradient";
                for (const auto &stop : linear->stops()) {
                    Element stopElement;
                    stopElement.type = "GradientStop";
                    stopElement.properties.insert("position", stop.first);
                    stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                    gradient.children.append(stopElement);
                }

                if (simpleVertical) {
                    rectElement.children.append(gradient);
                } else if (effectMode() == Qt5Effects) {
                    imports->insert("Qt5Compat.GraphicalEffects as GE");
                    Element effect;
                    effect.type = "GE.LinearGradient";
                    if (filled) {
                        if (!outputBase(shapeIndex, &effect, imports))
                            return false;
                    } else {
                        outputRect(path.rect, &effect);
                    }
                    effect.properties.insert("start", QPointF(gradStart.x() * horizontalScale, gradStart.y() * verticalScale));
                    effect.properties.insert("end", QPointF(gradEnd.x() * horizontalScale, gradEnd.y() * verticalScale));
                    effect.children.append(gradient);
                    rectElement.layers.append(effect);
                } else {
                    imports->insert("QtQuick.Shapes");
                    const qreal w = path.rect.width() * horizontalScale;
                    const qreal h = path.rect.height() * verticalScale;
                    const qreal r = path.radius * unitScale;
                    Element shapeElem;
                    shapeElem.type = "Shape";
                    if (filled) {
                        if (!outputBase(shapeIndex, &shapeElem, imports))
                            return false;
                    } else {
                        outputRect(path.rect, &shapeElem);
                    }
                    Element shapePath;
                    shapePath.type = "ShapePath";
                    shapePath.properties.insert("strokeWidth", 0);
                    shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                    Element fillGrad;
                    fillGrad.type = "fillGradient: LinearGradient";
                    fillGrad.properties.insert("x1", gradStart.x() * horizontalScale);
                    fillGrad.properties.insert("y1", gradStart.y() * verticalScale);
                    fillGrad.properties.insert("x2", gradEnd.x() * horizontalScale);
                    fillGrad.properties.insert("y2", gradEnd.y() * verticalScale);
                    for (const auto &stop : linear->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                        fillGrad.children.append(stopElement);
                    }
                    shapePath.children.append(fillGrad);
                    Element svgPath;
                    svgPath.type = "PathSvg";
                    svgPath.properties.insert("path", u"\"M %1 0 L %2 0 Q %3 0 %3 %4 L %3 %5 Q %3 %6 %2 %6 L %1 %6 Q 0 %6 0 %5 L 0 %4 Q 0 0 %1 0 Z\""_s
                        .arg(r).arg(w - r).arg(w).arg(r).arg(h - r).arg(h));
                    shapePath.children.append(svgPath);
                    shapeElem.children.append(shapePath);
                    if (filled)
                        *element = shapeElem;
                    else
                        element->children.prepend(shapeElem);
                    gradientHandled = true;
                }

                break; }
            case QGradient::RadialGradient: {
                const auto radial = reinterpret_cast<const QRadialGradient *>(g);
                if (effectMode() == Qt5Effects) {
                    imports->insert("Qt5Compat.GraphicalEffects as GE");
                    Element effect;
                    effect.type = "GE.RadialGradient";
                    if (filled) {
                        if (!outputBase(shapeIndex, &effect, imports))
                            return false;
                    } else {
                        outputRect(path.rect, &effect);
                    }
                    const qreal elemW = path.rect.width() * horizontalScale;
                    const qreal elemH = path.rect.height() * verticalScale;
                    effect.properties.insert("horizontalOffset", radial->center().x() * horizontalScale - elemW / 2);
                    effect.properties.insert("verticalOffset", radial->center().y() * verticalScale - elemH / 2);
                    effect.properties.insert("horizontalRadius", radial->radius() * horizontalScale);
                    effect.properties.insert("verticalRadius", radial->radius() * verticalScale);
                    Element gradient;
                    gradient.type = "gradient: Gradient";
                    for (const auto &stop : radial->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                        gradient.children.append(stopElement);
                    }
                    effect.children.append(gradient);
                    rectElement.layers.append(effect);
                } else {
                    imports->insert("QtQuick.Shapes");
                    const qreal w = path.rect.width() * horizontalScale;
                    const qreal h = path.rect.height() * verticalScale;
                    const qreal r = path.radius * unitScale;
                    Element shapeElem;
                    shapeElem.type = "Shape";
                    if (filled) {
                        if (!outputBase(shapeIndex, &shapeElem, imports))
                            return false;
                    } else {
                        outputRect(path.rect, &shapeElem);
                    }
                    Element shapePath;
                    shapePath.type = "ShapePath";
                    shapePath.properties.insert("strokeWidth", 0);
                    shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                    Element fillGrad;
                    fillGrad.type = "fillGradient: RadialGradient";
                    fillGrad.properties.insert("centerX", radial->center().x() * horizontalScale);
                    fillGrad.properties.insert("centerY", radial->center().y() * verticalScale);
                    fillGrad.properties.insert("centerRadius", radial->radius() * std::max(horizontalScale, verticalScale));
                    fillGrad.properties.insert("focalX", radial->focalPoint().x() * horizontalScale);
                    fillGrad.properties.insert("focalY", radial->focalPoint().y() * verticalScale);
                    fillGrad.properties.insert("focalRadius", radial->focalRadius() * std::max(horizontalScale, verticalScale));
                    for (const auto &stop : radial->stops()) {
                        Element stopElement;
                        stopElement.type = "GradientStop";
                        stopElement.properties.insert("position", stop.first);
                        stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                        fillGrad.children.append(stopElement);
                    }
                    shapePath.children.append(fillGrad);
                    Element svgPath;
                    svgPath.type = "PathSvg";
                    svgPath.properties.insert("path", u"\"M %1 0 L %2 0 Q %3 0 %3 %4 L %3 %5 Q %3 %6 %2 %6 L %1 %6 Q 0 %6 0 %5 L 0 %4 Q 0 0 %1 0 Z\""_s
                        .arg(r).arg(w - r).arg(w).arg(r).arg(h - r).arg(h));
                    shapePath.children.append(svgPath);
                    shapeElem.children.append(shapePath);
                    if (filled)
                        *element = shapeElem;
                    else
                        element->children.prepend(shapeElem);
                    gradientHandled = true;
                }
                break; }
            case QGradient::ConicalGradient: {
                const auto conical = reinterpret_cast<const QConicalGradient *>(g);
                imports->insert("QtQuick.Shapes");
                const qreal w = path.rect.width() * horizontalScale;
                const qreal h = path.rect.height() * verticalScale;
                const qreal r = path.radius * unitScale;
                Element shapeElem;
                shapeElem.type = "Shape";
                if (filled) {
                    if (!outputBase(shapeIndex, &shapeElem, imports))
                        return false;
                } else {
                    element->type = "Item";
                    if (!outputBase(shapeIndex, element, imports))
                        return false;
                    outputRect(path.rect, &shapeElem);
                }
                Element shapePath;
                shapePath.type = "ShapePath";
                shapePath.properties.insert("strokeWidth", 0);
                shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                Element fillGrad;
                fillGrad.type = "fillGradient: ConicalGradient";
                fillGrad.properties.insert("centerX", conical->center().x() * horizontalScale);
                fillGrad.properties.insert("centerY", conical->center().y() * verticalScale);
                fillGrad.properties.insert("angle", conical->angle());
                for (const auto &stop : conical->stops()) {
                    Element stopElement;
                    stopElement.type = "GradientStop";
                    stopElement.properties.insert("position", stop.first);
                    stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                    fillGrad.children.append(stopElement);
                }
                shapePath.children.append(fillGrad);
                Element svgPath;
                svgPath.type = "PathSvg";
                svgPath.properties.insert("path", u"\"M %1 0 L %2 0 Q %3 0 %3 %4 L %3 %5 Q %3 %6 %2 %6 L %1 %6 Q 0 %6 0 %5 L 0 %4 Q 0 0 %1 0 Z\""_s
                    .arg(r).arg(w - r).arg(w).arg(r).arg(h - r).arg(h));
                shapePath.children.append(svgPath);
                shapeElem.children.append(shapePath);
                if (filled)
                    *element = shapeElem;
                else
                    element->children.prepend(shapeElem);
                gradientHandled = true;
                break; }
            default:
                qWarning() << "Unsupported gradient type"_L1 << g->type();
                break;
            }
        } else {
            const auto &pen = shape->pen();
            bool needsShapePath = pen.style() != Qt::NoPen
                && (pen.style() != Qt::SolidLine || pen.brush().gradient());
            if (needsShapePath) {
                // Dashed or gradient stroke: use Shape+ShapePath (Rectangle.border can't do these)
                imports->insert("QtQuick.Shapes");
                qreal dw = computeStrokeWidth(pen, unitScale);
                QRectF strokeRect = adjustRectForStroke(path.rect, shape->strokeAlignment(), dw);
                const qreal w = strokeRect.width() * horizontalScale;
                const qreal h = strokeRect.height() * verticalScale;
                const qreal r = path.radius * unitScale;
                Element shapeElem;
                shapeElem.type = "Shape";
                if (filled) {
                    if (!outputBase(shapeIndex, &shapeElem, imports))
                        return false;
                } else {
                    outputRect(strokeRect, &shapeElem);
                }
                Element shapePath;
                shapePath.type = "ShapePath";
                shapePath.properties.insert("strokeWidth", dw);
                if (pen.brush().gradient()) {
                    const auto stops = pen.brush().gradient()->stops();
                    if (!stops.isEmpty())
                        shapePath.properties.insert("strokeColor", u"\"%1\""_s.arg(stops.first().second.name(QColor::HexArgb)));
                    else
                        shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
                } else {
                    shapePath.properties.insert("strokeColor", u"\"%1\""_s.arg(pen.color().name(QColor::HexArgb)));
                }
                if (!pen.dashPattern().isEmpty() && pen.style() != Qt::SolidLine) {
                    QStringList dashValues;
                    for (qreal d : pen.dashPattern())
                        dashValues.append(QString::number(d));
                    shapePath.properties.insert("dashPattern", u"[%1]"_s.arg(dashValues.join(u", "_s)));
                }
                if (pen.capStyle() == Qt::RoundCap)
                    shapePath.properties.insert("capStyle", u"ShapePath.RoundCap"_s);
                else if (pen.capStyle() == Qt::SquareCap)
                    shapePath.properties.insert("capStyle", u"ShapePath.SquareCap"_s);
                if (pen.joinStyle() == Qt::RoundJoin)
                    shapePath.properties.insert("joinStyle", u"ShapePath.RoundJoin"_s);
                else if (pen.joinStyle() == Qt::BevelJoin)
                    shapePath.properties.insert("joinStyle", u"ShapePath.BevelJoin"_s);
                if (shape->brush() != Qt::NoBrush)
                    shapePath.properties.insert("fillColor", u"\"%1\""_s.arg(shape->brush().color().name(QColor::HexArgb)));
                else
                    shapePath.properties.insert("fillColor", u"\"transparent\""_s);
                Element svgPath;
                svgPath.type = "PathSvg";
                svgPath.properties.insert("path", u"\"M %1 0 L %2 0 Q %3 0 %3 %4 L %3 %5 Q %3 %6 %2 %6 L %1 %6 Q 0 %6 0 %5 L 0 %4 Q 0 0 %1 0 Z\""_s
                    .arg(r).arg(w - r).arg(w).arg(r).arg(h - r).arg(h));
                shapePath.children.append(svgPath);
                shapeElem.children.append(shapePath);
                if (filled)
                    *element = shapeElem;
                else
                    element->children.prepend(shapeElem);
            } else if (shape->brush().style() == Qt::TexturePattern) {
                // Composited fill with rounded corners: clip an Image inside a transparent Rectangle
                QImage texImage = shape->brush().textureImage();
                if (imageScaling)
                    texImage = texImage.scaled(path.rect.width() * horizontalScale, path.rect.height() * verticalScale, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QString name = imageStore.save(imageFileName(shape->name(), "PNG"_L1), texImage, "PNG");
                rectElement.properties.insert("color", "\"transparent\"");
                rectElement.properties.insert("clip", true);
                Element imgElement;
                imgElement.type = "Image";
                imgElement.properties.insert("anchors.fill", "parent");
                imgElement.properties.insert("source", u"\"images/%1\""_s.arg(name));
                imgElement.properties.insert("fillMode", "Image.Stretch");
                rectElement.children.append(imgElement);
                gradientHandled = true;
                if (filled)
                    *element = rectElement;
                else
                    element->children.append(rectElement);
            } else {
                if (pen.style() != Qt::NoPen) {
                    qreal dw = computeStrokeWidth(pen, unitScale);
                    outputRect(adjustRectForStroke(path.rect, shape->strokeAlignment(), dw), &rectElement);
                    rectElement.properties.insert("border.width", dw);
                    rectElement.properties.insert("border.color", u"\"%1\""_s.arg(pen.color().name(QColor::HexArgb)));
                }
                if (shape->brush() != Qt::NoBrush)
                    rectElement.properties.insert("color", u"\"%1\""_s.arg(shape->brush().color().name(QColor::HexArgb)));
                else
                    rectElement.properties.insert("color", "\"transparent\"");
            }
        }
        if (!gradientHandled) {
            if (filled)
                *element = rectElement;
            else
                element->children.append(rectElement);
        }
        break; }
    case QPsdAbstractLayerItem::PathInfo::Path: {
        // Circle optimization: emit Rectangle { radius: width/2 } instead of Shape+PathAngleArc
        // No qFuzzyCompare() here, which is too strict to test visual distinction.
        constexpr qreal epsilon = 0.1;
        if (!path.rect.isEmpty()
            && qAbs(path.rect.width() - path.rect.height()) > epsilon) {
            QPainterPath refCircle;
            refCircle.addEllipse(path.rect);
            refCircle.setFillRule(path.path.fillRule());
            if (path.path == refCircle) {
                const QGradient *g = effectiveGradient(shape);
                if (!g && shape->brush().style() != Qt::TexturePattern) {
                    bool filled = isFilledRect(path, shape);
                    Element rectElement;
                    rectElement.type = "Rectangle";
                    if (filled) {
                        if (!outputBase(shapeIndex, &rectElement, imports))
                            return false;
                    } else {
                        element->type = "Item";
                        if (!outputBase(shapeIndex, element, imports))
                            return false;
                        outputRect(path.rect, &rectElement);
                    }
                    rectElement.properties.insert("radius", path.rect.width() * horizontalScale / 2.0);
                    const auto &pen = shape->pen();
                    if (pen.style() != Qt::NoPen) {
                        qreal dw = computeStrokeWidth(pen, unitScale);
                        rectElement.properties.insert("border.width", dw);
                        rectElement.properties.insert("border.color", u"\"%1\""_s.arg(pen.color().name(QColor::HexArgb)));
                    }
                    if (shape->brush() != Qt::NoBrush)
                        rectElement.properties.insert("color", u"\"%1\""_s.arg(shape->brush().color().name(QColor::HexArgb)));
                    else
                        rectElement.properties.insert("color", "\"transparent\"");
                    if (filled)
                        *element = rectElement;
                    else
                        element->children.append(rectElement);
                    return true;
                }
            }
        }
        // Fall through to Shape for non-circles and complex fills (gradients, textures)
        imports->insert("QtQuick.Shapes");
        element->type = "Shape";
        if (!outputBase(shapeIndex, element, imports))
            return false;
        Element shapePath;
        shapePath.type = "ShapePath";

        // Stroke properties (always output from pen)
        const auto &pen = shape->pen();
        if (pen.style() == Qt::NoPen) {
            shapePath.properties.insert("strokeWidth", 0);
            shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
        } else {
            shapePath.properties.insert("strokeWidth", pen.widthF() * unitScale);
            if (pen.brush().gradient()) {
                // Gradient stroke: use first stop color as approximation
                const auto stops = pen.brush().gradient()->stops();
                if (!stops.isEmpty())
                    shapePath.properties.insert("strokeColor", u"\"%1\""_s.arg(stops.first().second.name(QColor::HexArgb)));
                else
                    shapePath.properties.insert("strokeColor", u"\"transparent\""_s);
            } else {
                shapePath.properties.insert("strokeColor", u"\"%1\""_s.arg(pen.color().name(QColor::HexArgb)));
            }
            // Dash pattern
            if (pen.style() != Qt::SolidLine && !pen.dashPattern().isEmpty()) {
                QStringList dashValues;
                for (qreal d : pen.dashPattern())
                    dashValues.append(QString::number(d));
                shapePath.properties.insert("dashPattern", u"[%1]"_s.arg(dashValues.join(u", "_s)));
            }
            // Cap style
            if (pen.capStyle() == Qt::RoundCap)
                shapePath.properties.insert("capStyle", u"ShapePath.RoundCap"_s);
            else if (pen.capStyle() == Qt::SquareCap)
                shapePath.properties.insert("capStyle", u"ShapePath.SquareCap"_s);
            // Join style
            if (pen.joinStyle() == Qt::RoundJoin)
                shapePath.properties.insert("joinStyle", u"ShapePath.RoundJoin"_s);
            else if (pen.joinStyle() == Qt::BevelJoin)
                shapePath.properties.insert("joinStyle", u"ShapePath.BevelJoin"_s);
        }

        // Fill properties
        const QGradient *g = effectiveGradient(shape);
        if (g) {
            switch (g->type()) {
            case QGradient::LinearGradient: {
                const auto linear = reinterpret_cast<const QLinearGradient *>(g);
                const QTransform brushTransform = shape->brush().transform();
                const QPointF gradStart = brushTransform.map(linear->start());
                const QPointF gradEnd = brushTransform.map(linear->finalStop());
                Element gradient;
                gradient.type = "fillGradient: LinearGradient";
                gradient.properties.insert("x1", gradStart.x() * horizontalScale);
                gradient.properties.insert("y1", gradStart.y() * verticalScale);
                gradient.properties.insert("x2", gradEnd.x() * horizontalScale);
                gradient.properties.insert("y2", gradEnd.y() * verticalScale);
                for (const auto &stop : linear->stops()) {
                    Element stopElement;
                    stopElement.type = "GradientStop";
                    stopElement.properties.insert("position", stop.first);
                    stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                    gradient.children.append(stopElement);
                }
                shapePath.children.append(gradient);
                break; }
            case QGradient::RadialGradient: {
                const auto radial = reinterpret_cast<const QRadialGradient *>(g);
                Element gradient;
                gradient.type = "fillGradient: RadialGradient";
                gradient.properties.insert("centerX", radial->center().x() * horizontalScale);
                gradient.properties.insert("centerY", radial->center().y() * verticalScale);
                gradient.properties.insert("centerRadius", radial->radius() * std::max(horizontalScale, verticalScale));
                gradient.properties.insert("focalX", radial->focalPoint().x() * horizontalScale);
                gradient.properties.insert("focalY", radial->focalPoint().y() * verticalScale);
                gradient.properties.insert("focalRadius", radial->focalRadius() * std::max(horizontalScale, verticalScale));
                for (const auto &stop : radial->stops()) {
                    Element stopElement;
                    stopElement.type = "GradientStop";
                    stopElement.properties.insert("position", stop.first);
                    stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                    gradient.children.append(stopElement);
                }
                shapePath.children.append(gradient);
                break; }
            case QGradient::ConicalGradient: {
                const auto conical = reinterpret_cast<const QConicalGradient *>(g);
                Element gradient;
                gradient.type = "fillGradient: ConicalGradient";
                gradient.properties.insert("centerX", conical->center().x() * horizontalScale);
                gradient.properties.insert("centerY", conical->center().y() * verticalScale);
                gradient.properties.insert("angle", conical->angle());
                for (const auto &stop : conical->stops()) {
                    Element stopElement;
                    stopElement.type = "GradientStop";
                    stopElement.properties.insert("position", stop.first);
                    stopElement.properties.insert("color", u"\"%1\""_s.arg(stop.second.name(QColor::HexArgb)));
                    gradient.children.append(stopElement);
                }
                shapePath.children.append(gradient);
                break; }
            default:
                qWarning() << "Unsupported gradient type"_L1 << g->type();
                break;
            }
        } else {
            if (shape->brush() != Qt::NoBrush)
                shapePath.properties.insert("fillColor", u"\"%1\""_s.arg(shape->brush().color().name(QColor::HexArgb)));
        }

        if (!outputPath(path.path, &shapePath))
            return false;
        element->children.append(shapePath);
        break; }
    }
    return true;
}

bool QPsdExporterQtQuickPlugin::outputImage(const QModelIndex &imageIndex, Element *element, ImportData *imports) const
{
    const QPsdImageLayerItem *image = dynamic_cast<const QPsdImageLayerItem *>(model()->layerItem(imageIndex));

    QString name = saveLayerImage(image);

    // Apply color overlay (SOFI) effect by baking it into the exported PNG
    for (const auto &effect : image->effects()) {
        if (effect.canConvert<QPsdSofiEffect>()) {
            const auto sofi = effect.value<QPsdSofiEffect>();
            const QString path = dir.absoluteFilePath("images/"_L1 + name);
            QImage img(path);
            if (img.isNull())
                break;
            img = img.convertToFormat(QImage::Format_ARGB32);
            const QColor overlayColor(sofi.nativeColor());
            const qreal opacity = sofi.opacity();

            // Create overlay image: SOFI color masked by original alpha
            QImage overlayImg(img.size(), QImage::Format_ARGB32);
            overlayImg.fill(Qt::transparent);
            for (int y = 0; y < img.height(); ++y) {
                const QRgb *srcLine = reinterpret_cast<const QRgb *>(img.constScanLine(y));
                QRgb *dstLine = reinterpret_cast<QRgb *>(overlayImg.scanLine(y));
                for (int x = 0; x < img.width(); ++x) {
                    const int a = qAlpha(srcLine[x]);
                    if (a > 0)
                        dstLine[x] = qRgba(overlayColor.red(), overlayColor.green(), overlayColor.blue(), a);
                }
            }

            if (QtPsdGui::isCustomBlendMode(sofi.blendMode())) {
                QtPsdGui::customBlend(img, overlayImg, sofi.blendMode(), opacity);
            } else {
                // Step 1: Apply blend at full strength
                QImage blended;
                if (sofi.blendMode() == QPsdBlend::Mode::Normal) {
                    blended = overlayImg;
                } else {
                    blended = img;
                    QPainter painter(&blended);
                    painter.setCompositionMode(QtPsdGui::compositionMode(sofi.blendMode()));
                    painter.drawImage(0, 0, overlayImg);
                    painter.end();
                }
                // Step 2: Mix blended result with original using SOFI opacity
                for (int y = 0; y < img.height(); ++y) {
                    const QRgb *bLine = reinterpret_cast<const QRgb *>(blended.constScanLine(y));
                    QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
                    for (int x = 0; x < img.width(); ++x) {
                        const int a = qAlpha(line[x]);
                        if (a == 0) continue;
                        const int r = qRound(qRed(line[x]) * (1.0 - opacity) + qRed(bLine[x]) * opacity);
                        const int g = qRound(qGreen(line[x]) * (1.0 - opacity) + qGreen(bLine[x]) * opacity);
                        const int b = qRound(qBlue(line[x]) * (1.0 - opacity) + qBlue(bLine[x]) * opacity);
                        line[x] = qRgba(r, g, b, a);
                    }
                }
            }
            img.save(path);
        }
    }

    element->type = "Image";
    if (!outputBase(imageIndex, element, imports))
        return false;
    element->properties.insert("source", u"\"images/%1\""_s.arg(name));
    element->properties.insert("fillMode", "Image.PreserveAspectFit");

    const auto *border = image->border();
    if (border && border->isEnable()) {
        Element wrapper;
        wrapper.type = "Rectangle";
        const qreal bw = border->size() * unitScale;
        // Qt Quick draws borders inside the rect; adjust for PSD stroke position
        qreal expand = 0;
        if (border->position() == QPsdBorder::Outer)
            expand = bw;
        else if (border->position() == QPsdBorder::Center)
            expand = bw / 2.0;
        if (expand > 0) {
            QRect baseRect = computeBaseRect(imageIndex);
            QRectF expanded(baseRect.x() - expand, baseRect.y() - expand,
                            baseRect.width() + expand * 2, baseRect.height() + expand * 2);
            if (!outputBase(imageIndex, &wrapper, imports, expanded.toRect()))
                return false;
            // Position image inside wrapper at the expand offset
            element->properties.remove("x");
            element->properties.remove("y");
            element->properties.insert("x", expand);
            element->properties.insert("y", expand);
        } else {
            if (!outputBase(imageIndex, &wrapper, imports))
                return false;
            element->properties.remove("x");
            element->properties.remove("y");
            element->properties.insert("anchors.fill", "parent");
        }
        wrapper.properties.insert("border.width", bw);
        wrapper.properties.insert("border.color", u"\"%1\""_s.arg(border->color().name(QColor::HexArgb)));
        wrapper.properties.insert("color", "\"transparent\"");
        wrapper.children.append(*element);
        *element = wrapper;
    }

    return true;
}

bool QPsdExporterQtQuickPlugin::traverseTree(const QModelIndex &index, Element *parent, ImportData *imports, ExportData *exports, std::optional<QPsdExporterTreeItemModel::ExportHint::Type> hintOverload, QPsdBlend::Mode groupBlendMode) const
{
    const QPsdAbstractLayerItem *item = model()->layerItem(index);
    const auto hint = model()->layerHint(index);;
    const auto id = toLowerCamelCase(hint.id);
    auto type = hint.type;
    if (hintOverload.has_value()) {
        type = *hintOverload;
    }

    switch (type) {
    case QPsdExporterTreeItemModel::ExportHint::Embed: {
        Element element;
        element.id = id;
        if (!hint.visible || (item && !item->isVisible()))
            element.properties.insert("visible", false);
        if (!id.isEmpty())
            exports->insert(id);
        // Clipping mask: NonBase layers are clipped to their base layer's alpha
        const auto *guiModel = static_cast<const QPsdExporterTreeItemModel *>(model())->guiLayerTreeItemModel();
        const QModelIndex sourceIndex = static_cast<const QPsdExporterTreeItemModel *>(model())->mapToSource(index);
        const QModelIndex clipBaseSourceIndex = guiModel->clippingMaskIndex(sourceIndex);
        if (clipBaseSourceIndex.isValid()) {
            const auto *baseItem = guiModel->layerItem(clipBaseSourceIndex);
            if (baseItem && item) {
                QImage clippedImg = item->image();
                QImage baseAlpha = baseItem->image();
                if (!clippedImg.isNull() && !baseAlpha.isNull()) {
                    // Composite: show clipped layer only where base layer has content
                    const QRect baseRect = baseItem->rect();
                    const QRect clippedRect = item->rect();

                    // Draw clipped layer, then mask with base alpha
                    QImage result(clippedRect.size(), QImage::Format_ARGB32);
                    result.fill(Qt::transparent);
                    QPainter painter(&result);
                    painter.drawImage(0, 0, clippedImg);
                    // DestinationIn: keep destination pixels only where source has alpha
                    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                    QImage baseMask(clippedRect.size(), QImage::Format_ARGB32);
                    baseMask.fill(Qt::transparent);
                    QPainter maskPainter(&baseMask);
                    maskPainter.drawImage(baseRect.topLeft() - clippedRect.topLeft(), baseAlpha);
                    maskPainter.end();
                    painter.drawImage(0, 0, baseMask);
                    painter.end();

                    const QString name = imageStore.save(imageFileName(item->name(), "PNG"_L1), result, "PNG");
                    if (!name.isEmpty()) {
                        element.type = "Image";
                        if (!outputBase(index, &element, imports))
                            return false;
                        element.properties.insert("source", u"\"images/%1\""_s.arg(name));
                        element.properties.insert("fillMode", "Image.PreserveAspectFit");
                        parent->children.append(element);
                    }
                }
            }
            break;
        }

        bool generated = false;
        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            generated = outputFolder(index, &element, imports, exports, groupBlendMode);
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
            // Adjustment layers modify all layers below them via ShaderEffect
            applyAdjustmentLayer(item, parent, imports);
            return true; }
        default:
            return true;
        }
        if (!generated)
            return false;

        // Apply raster layer mask via MultiEffect (all layer types)
        if (!item->layerMask().isNull()) {
            imports->insert("QtQuick.Effects");
            const QString maskId = u"_rmask_%1"_s.arg(m_maskCounter++);
            const QImage mask = item->layerMask();
            const QString maskName = imageStore.save(
                imageFileName(item->name() + "_mask"_L1, "PNG"_L1), mask, "PNG");
            if (!maskName.isEmpty()) {
                Element maskItem;
                maskItem.type = "Image";
                maskItem.id = maskId;
                maskItem.properties.insert("visible", false);
                maskItem.properties.insert("layer.enabled", true);
                maskItem.properties.insert("source", u"\"images/%1\""_s.arg(maskName));
                maskItem.properties.insert("fillMode", "Image.PreserveAspectFit");
                maskItem.properties.insert("width", item->rect().width() * horizontalScale);
                maskItem.properties.insert("height", item->rect().height() * verticalScale);
                element.children.prepend(maskItem);

                Element effect;
                effect.type = "MultiEffect";
                effect.properties.insert("maskEnabled", true);
                effect.properties.insert("maskSource", maskId);
                element.layers.append(effect);
            }
        }

        // Determine effective blend mode for leaf items
        if (item->type() != QPsdAbstractLayerItem::Folder) {
            QPsdBlend::Mode effectiveMode = QPsdBlend::Invalid;
            // Leaf's own blend mode takes priority
            const auto leafMode = item->record().blendMode();
            if (leafMode != QPsdBlend::Normal && leafMode != QPsdBlend::PassThrough && leafMode != QPsdBlend::Invalid) {
                effectiveMode = leafMode;
            } else if (groupBlendMode != QPsdBlend::PassThrough && groupBlendMode != QPsdBlend::Invalid) {
                effectiveMode = groupBlendMode;
            }
            if (effectiveMode != QPsdBlend::Invalid) {
                element.properties.insert("layer.enabled", true);
                const auto modeStr = blendModeString(effectiveMode);
                if (!modeStr.isEmpty())
                    element.properties.insert("property string blendMode", u"\"%1\""_s.arg(modeStr));
            }
        }

        const bool hasRenderableContent = !element.type.isEmpty()
                                          || !element.children.isEmpty()
                                          || !element.layers.isEmpty();
        if (!hasRenderableContent)
            return true;

        if (!id.isEmpty()) {
            if (hint.interactive || hint.baseElement == QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea) {
                Element touchArea { "MouseArea", id };
                outputBase(index, &touchArea, imports);
                touchArea.layers.clear();
                if (!hint.visible || (item && !item->isVisible()))
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
    case QPsdExporterTreeItemModel::ExportHint::Native: {
        Element element;
        switch (hint.baseElement) {
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
            element.type = "Rectangle";
            break;
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea:
            element.type = "MouseArea";
            break;
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button:
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted:
            imports->insert("QtQuick.Controls");
            element.type = "Button";
            element.properties.insert("highlighted", (hint.baseElement == QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted));
            if (indexMergeMap.contains(index)) {
                const auto &mergedLayers = indexMergeMap.values(index);
                for (const auto &mergedIndex : mergedLayers) {
                    const QPsdAbstractLayerItem *i = model()->layerItem(mergedIndex);
                    switch (i->type()) {
                    case QPsdAbstractLayerItem::Text: {
                        const auto *textItem = reinterpret_cast<const QPsdTextLayerItem *>(i);
                        const auto runs = textItem->runs();
                        bool first = true;
                        QString text;
                        for (const auto &run : runs) {
                            if (first) {
                                element.properties.insert("font.family", u"\"%1\""_s.arg(run.font.family()));
                                element.properties.insert("font.pixelSize", std::round(run.font.pointSizeF() * fontScaleFactor));
                                first = false;
                            }
                            text += run.text;
                        }
                        element.properties.insert("text", u"\"%1\""_s.arg(text));
                        break; }
                    case QPsdAbstractLayerItem::Image:
                    case QPsdAbstractLayerItem::Shape: {
                        const QString name = saveLayerImage(i);
                        element.properties.insert("icon.source", u"\"images/%1\""_s.arg(name));
                        break; }
                    default:
                        qWarning() << i->type() << "is not supported";
                    }
                }
            }
            break;
        }
        element.id = id;
        if (!hint.visible || (item && !item->isVisible()))
            element.properties.insert("visible", false);
        if (!id.isEmpty())
            exports->insert(id);
        if (!outputBase(index, &element, imports))
            return false;
        parent->children.append(element);
        break; }
    case QPsdExporterTreeItemModel::ExportHint::Component: {
        ImportData i;
        i.insert("QtQuick");
        ExportData x;

        Element component;
        bool generated = false;

        switch (item->type()) {
        case QPsdAbstractLayerItem::Folder: {
            generated = outputFolder(index, &component, &i, &x, groupBlendMode);
            break; }
        case QPsdAbstractLayerItem::Text: {
            generated = outputText(index, &component, &i);
            break; }
        case QPsdAbstractLayerItem::Shape: {
            generated = outputShape(index, &component, &i);
            break; }
        case QPsdAbstractLayerItem::Image: {
            generated = outputImage(index, &component, &i);
            break; }
        case QPsdAbstractLayerItem::Adjustment:
            // TODO: adjustment layers in Component mode
            return true;
        default:
            return true;
        }
        if (!generated)
            return false;
        switch (hint.baseElement) {
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
            component.type = "Item";
            break;
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::TouchArea:
            component.type = "MouseArea";
            break;
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button:
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted:
            i.insert("QtQuick.Controls");
            component.type = "Button";
            component.properties.insert("highlighted", (hint.baseElement == QPsdExporterTreeItemModel::ExportHint::NativeComponent::Button_Highlighted));
            break;
        }
        // Component files have their own coordinate space; x/y belong on the instance side only
        component.properties.remove("x");
        component.properties.remove("y");
        if (!saveTo(hint.componentName + ".ui", &component, i, x))
            return false;

        Element element;
        element.type = hint.componentName;
        element.id = id;
        if (!id.isEmpty())
            exports->insert(id);
        outputBase(index, &element, imports);
        if (!hint.visible || (item && !item->isVisible()))
            element.properties.insert("visible", false);
        parent->children.append(element);
        break;
                            }
    case QPsdExporterTreeItemModel::ExportHint::Merged:
    case QPsdExporterTreeItemModel::ExportHint::Skip:
        return true;
    }

    return true;
}

bool QPsdExporterQtQuickPlugin::saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const
{
    QFile file(dir.absoluteFilePath(baseName + ".qml"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&file);

    writeLicenseHeader(out);

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
        if (element->type.trimmed().isEmpty()) {
            for (auto &child : element->children) {
                if (!traverseElement(&child, level))
                    return false;
            }
            return true;
        }

        // apply layers recursively
        auto layers = element->layers;
        std::function<void(Element *)> apply;
        apply = [&](Element *parent) {
            if (layers.isEmpty())
                return;
            auto layer = layers.takeFirst();
            if (layer.type.trimmed().isEmpty()) {
                apply(parent);
                return;
            }
            // Extract inline maskSource from layer effect and promote to a separate
            // child of the parent with visible:false + layer.enabled:true + unique id.
            // Inline maskSource inside layer.effect doesn't render as a texture;
            // it must be a scene graph item referenced by id.
            for (int i = layer.children.size() - 1; i >= 0; i--) {
                auto &child = layer.children[i];
                if (child.type.startsWith("maskSource:"_L1)) {
                    const auto maskId = u"_mask_%1"_s.arg(m_blendCounter++);
                    // Create the mask item as a child of parent
                    Element maskItem;
                    maskItem.id = maskId;
                    // "maskSource: Rectangle" → "Rectangle"
                    maskItem.type = child.type.mid(child.type.indexOf(':'_L1) + 2);
                    maskItem.properties = child.properties;
                    maskItem.children = child.children;
                    maskItem.properties.insert("visible"_L1, false);
                    maskItem.properties.insert("layer.enabled"_L1, true);
                    parent->children.prepend(maskItem);
                    // Replace inline child with id reference property
                    layer.properties.insert("maskSource"_L1, maskId);
                    layer.children.removeAt(i);
                }
            }
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
            if (!traverseElement(&child, level))
                return false;
        }
        level--;
        out << QByteArray(level * 4, ' ') << "}\n";
        return true;
    };
    return traverseElement(element, 0);
}

QT_END_NAMESPACE

#include "qtquick.moc"
