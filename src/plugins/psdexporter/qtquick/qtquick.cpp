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
#include <QtGui/QPen>

#include <QtPsdCore/qpsdblend.h>
#include <QtPsdCore/QPsdSofiEffect>
#include <QtPsdGui/QPsdBorder>

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

    EffectMode m_effectMode = NoGPU;
    mutable bool m_needsBlendShader = false;
    mutable int m_blendCounter = 0;

    bool outputBase(const QModelIndex &index, Element *element, ImportData *imports, QRect rectBounds = {}) const;
    bool outputRect(const QRectF &rect, Element *element, bool skipEmpty = false) const;
    bool outputPath(const QPainterPath &path, Element *element) const;
    bool outputFolder(const QModelIndex &folderIndex, Element *element, ImportData *imports, ExportData *exports, QPsdBlend::Mode groupBlendMode = QPsdBlend::PassThrough) const;
    bool outputText(const QModelIndex &textIndex, Element *element, ImportData *imports) const;
    bool outputShape(const QModelIndex &shapeIndex, Element *element, ImportData *imports) const;
    bool outputImage(const QModelIndex &imageIndex, Element *element, ImportData *imports) const;

    bool traverseTree(const QModelIndex &index, Element *parent, ImportData *imports, ExportData *exports, std::optional<QPsdExporterTreeItemModel::ExportHint::Type> hintOverload = std::nullopt, QPsdBlend::Mode groupBlendMode = QPsdBlend::PassThrough) const;
    void applyBlendModes(Element *element, ImportData *imports) const;

    bool saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const;
};

bool QPsdExporterQtQuickPlugin::exportTo(const QPsdExporterTreeItemModel *model,  const QString &to, const ExportConfig &config) const
{
    if (!initializeExport(model, to, config, "images"_L1)) {
        return false;
    }

    m_blendCounter = 0;
    m_needsBlendShader = false;

    ImportData imports;
    imports.insert("QtQuick");
    ExportData exports;

    Element window;
    window.type = "Item";
    window.properties.insert("width", model->size().width() * horizontalScale);
    window.properties.insert("height", model->size().height() * verticalScale);

    for (int i = model->rowCount(QModelIndex {}) - 1; i >= 0; i--) {
        QModelIndex childIndex = model->index(i, 0, QModelIndex {});
        if (!traverseTree(childIndex, &window, &imports, &exports, std::nullopt))
            return false;
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
    if (rect.isEmpty()) {
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
            } else {
                Element effect;
                if (effectMode() == Qt5Effects) {
                    imports->insert("Qt5Compat.GraphicalEffects as GE");
                    effect.type = "GE.OpacityMask";
                } else {
                    imports->insert("QtQuick.Effects");
                    effect.type = "MultiEffect";
                    effect.properties.insert("maskEnabled", true);
                }
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
        outputRect(folder->artboardRect(), &artboard);
        artboard.properties.insert("color", u"\"%1\""_s.arg(folder->artboardBackground().name(QColor::HexArgb)));
        element->children.append(artboard);
    }
    for (int i = model()->rowCount(folderIndex) - 1; i >= 0; i--) {
        QModelIndex childIndex = model()->index(i, 0, folderIndex);
        if (!traverseTree(childIndex, element, imports, exports, std::nullopt, nextGroupBlendMode))
            return false;
    }
    if (effectMode() != NoGPU)
        applyBlendModes(element, imports);
    return true;
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
                bgItem.properties.insert(u"visible"_s, false);
                bgItem.children = accumulated;

                // Wrap blend-mode child as foreground source
                Element fgItem;
                fgItem.type = u"Item"_s;
                fgItem.id = u"_blend_fg_%1"_s.arg(m_blendCounter);
                fgItem.properties.insert(u"anchors.fill"_s, u"parent"_s);
                fgItem.properties.insert(u"layer.enabled"_s, true);
                fgItem.properties.insert(u"visible"_s, false);
                fgItem.children.append(child);

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
                bgItem.properties.insert(u"visible"_s, false);
                bgItem.children = accumulated;

                // Wrap blend-mode child as foreground source
                Element fgItem;
                fgItem.type = u"Item"_s;
                fgItem.id = u"_blend_fg_%1"_s.arg(m_blendCounter);
                fgItem.properties.insert(u"anchors.fill"_s, u"parent"_s);
                fgItem.properties.insert(u"layer.enabled"_s, true);
                fgItem.properties.insert(u"visible"_s, false);
                fgItem.children.append(child);

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
        if (run.font.bold() || run.fauxBold)
            element->properties.insert("font.bold", true);
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
        if (run.lineHeight > 0)
            element->properties.insert("lineHeight", run.lineHeight / run.font.pointSizeF());
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
                if (run.font.bold() || run.fauxBold)
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
                if (run.lineHeight > 0)
                    textElement.properties.insert("lineHeight", run.lineHeight / run.font.pointSizeF());
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
                const bool simpleVertical = linear->start().x() == linear->finalStop().x();
                const bool simpleHorizontal = linear->start().y() == linear->finalStop().y();

                if (simpleVertical || simpleHorizontal) {
                    // Gradient position 0=top/left, 1=bottom/right; invert if PSD direction is reversed
                    const bool reversed = simpleHorizontal
                        ? (linear->start().x() > linear->finalStop().x())
                        : (linear->start().y() > linear->finalStop().y());
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
                    effect.properties.insert("start", QPointF(linear->start().x() * horizontalScale, linear->start().y() * verticalScale));
                    effect.properties.insert("end", QPointF(linear->finalStop().x() * horizontalScale, linear->finalStop().y() * verticalScale));
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
                    fillGrad.properties.insert("x1", linear->start().x() * horizontalScale);
                    fillGrad.properties.insert("y1", linear->start().y() * verticalScale);
                    fillGrad.properties.insert("x2", linear->finalStop().x() * horizontalScale);
                    fillGrad.properties.insert("y2", linear->finalStop().y() * verticalScale);
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
            default:
                qWarning() << "Unsupported gradient type for fill layer:" << g->type();
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
                const bool simpleVertical = linear->start().x() == linear->finalStop().x();
                const bool simpleHorizontal = linear->start().y() == linear->finalStop().y();

                if (simpleVertical || simpleHorizontal) {
                    const bool reversed = simpleHorizontal
                        ? (linear->start().x() > linear->finalStop().x())
                        : (linear->start().y() > linear->finalStop().y());
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
                    effect.properties.insert("start", QPointF(linear->start().x() * horizontalScale, linear->start().y() * verticalScale));
                    effect.properties.insert("end", QPointF(linear->finalStop().x() * horizontalScale, linear->finalStop().y() * verticalScale));
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
                    fillGrad.properties.insert("x1", linear->start().x() * horizontalScale);
                    fillGrad.properties.insert("y1", linear->start().y() * verticalScale);
                    fillGrad.properties.insert("x2", linear->finalStop().x() * horizontalScale);
                    fillGrad.properties.insert("y2", linear->finalStop().y() * verticalScale);
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
                bool simpleVertical = linear->start().x() == linear->finalStop().x();
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
                    effect.properties.insert("start", QPointF(linear->start().x() * horizontalScale, linear->start().y() * verticalScale));
                    effect.properties.insert("end", QPointF(linear->finalStop().x() * horizontalScale, linear->finalStop().y() * verticalScale));
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
                    fillGrad.properties.insert("x1", linear->start().x() * horizontalScale);
                    fillGrad.properties.insert("y1", linear->start().y() * verticalScale);
                    fillGrad.properties.insert("x2", linear->finalStop().x() * horizontalScale);
                    fillGrad.properties.insert("y2", linear->finalStop().y() * verticalScale);
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

    element->type = "Image";
    if (!outputBase(imageIndex, element, imports))
        return false;
    element->properties.insert("source", u"\"images/%1\""_s.arg(name));
    element->properties.insert("fillMode", "Image.PreserveAspectFit");

    const auto *border = image->border();
    if (border && border->isEnable()) {
        Element wrapper;
        wrapper.type = "Rectangle";
        if (!outputBase(imageIndex, &wrapper, imports))
            return false;
        wrapper.properties.insert("border.width", border->size() * unitScale);
        wrapper.properties.insert("border.color", u"\"%1\""_s.arg(border->color().name(QColor::HexArgb)));
        wrapper.properties.insert("color", "\"transparent\"");
        element->properties.remove("x");
        element->properties.remove("y");
        element->properties.insert("anchors.fill", "parent");
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
        default:
            return true;
        }
        if (!generated)
            return false;

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
            if (hint.interactive) {
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
        default:
            return true;
        }
        if (!generated)
            return false;
        switch (hint.baseElement) {
        case QPsdExporterTreeItemModel::ExportHint::NativeComponent::Container:
            component.type = "Item";
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
