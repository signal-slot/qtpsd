// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDQMLEXPORTERPLUGIN_H
#define QPSDQMLEXPORTERPLUGIN_H

#include <QtPsdExporter/qpsdexporterglobal.h>
#include <QtPsdExporter/qpsdexporterplugin.h>

#include <QtPsdCore/qpsdblend.h>
#include <QtPsdGui/qpsdabstractlayeritem.h>

#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QVariantHash>

QT_BEGIN_NAMESPACE

// Shared implementation for exporter plugins that emit QML-like syntax. Both
// the Qt Quick plugin (full effects palette) and the Qt for MCUs plugin
// (NoGPU subset) inherit from this and customise behaviour via virtual hooks.
class Q_PSDEXPORTER_EXPORT QPsdQmlExporterPlugin : public QPsdExporterPlugin
{
    Q_OBJECT
public:
    // Visual effect dispatch. NoGPU produces CPU-friendly output that runs on
    // any QML runtime (including Qt for MCUs). Qt5Effects uses
    // Qt5Compat.GraphicalEffects items; EffectMaker uses bundled custom
    // ShaderEffect blend / adjustment shaders.
    enum EffectMode { NoGPU, Qt5Effects, EffectMaker };
    Q_ENUM(EffectMode)

    // In-memory representation of a single QML element. `children` are nested
    // items; `layers` are layer.effect chains promoted to children at the
    // serialise step (see saveTo()).
    struct Element {
        QString type;
        QString id;
        QString name;
        QVariantHash properties;
        QList<Element> children;
        QList<Element> layers;
    };

    explicit QPsdQmlExporterPlugin(QObject *parent = nullptr);
    ~QPsdQmlExporterPlugin() override;

protected:
    using ImportData = QSet<QString>;
    using ExportData = QSet<QString>;

    // ---------- Customisation hooks ----------------------------------------

    // Active effect mode. Default = NoGPU. The Qt Quick plugin overrides this
    // to return its property-backed mode; Qt for MCUs keeps the default.
    virtual EffectMode effectMode() const { return NoGPU; }

    // Called once at the start / end of exportTo(), respectively. Lets
    // subclasses copy auxiliary files (shaders, fonts) and generate companion
    // project files (.qmlproject) without re-implementing exportTo().
    virtual void onBeforeExport(const ExportConfig &config) const;
    virtual void onAfterExport(const ExportConfig &config) const;

    // Target-capability probes. Concrete targets advertise which optional
    // QML constructs they can render so the generator can skip them at the
    // emission site (cleaner and cheaper than post-processing the tree).
    // Defaults reflect Qt Quick's full capability surface; the Qt for MCUs
    // override returns false where its qmltocpp / runtime is more restrictive.
    virtual bool supportsRectangleBorder() const { return true; }
    virtual bool supportsFontFamily() const { return true; }
    virtual bool buttonHighlightedSupported() const { return true; }
    // FlexboxLayout (QtQuick.Layouts) is Qt 6.9+; MCU runtime lacks it.
    virtual bool supportsFlexbox() const { return true; }

    // Called from traverseTree() in the Component case after the inner
    // component element is built and x/y are stripped. Default = no-op.
    // Qt for MCUs strips anchors and forces an explicit width/height because
    // its qmltocpp rejects `anchors.fill: parent` on a component root.
    virtual void adjustComponentRoot(Element &component, const QModelIndex &index) const;

    // ---------- Template method --------------------------------------------

    bool exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const ExportConfig &config) const override;

    // ---------- Output / traversal -----------------------------------------

    bool outputBase(const QModelIndex &index, Element *element, ImportData *imports, QRect rectBounds = {}) const;
    bool outputRect(const QRectF &rect, Element *element, bool skipEmpty = false) const;
    bool outputPath(const QPainterPath &path, Element *element) const;
    bool outputFolder(const QModelIndex &folderIndex, Element *element, ImportData *imports, ExportData *exports, QPsdBlend::Mode groupBlendMode = QPsdBlend::PassThrough) const;
    bool outputText(const QModelIndex &textIndex, Element *element, ImportData *imports) const;
    bool outputShape(const QModelIndex &shapeIndex, Element *element, ImportData *imports) const;
    bool outputImage(const QModelIndex &imageIndex, Element *element, ImportData *imports) const;

    bool traverseTree(const QModelIndex &index, Element *parent, ImportData *imports, ExportData *exports, std::optional<QPsdExporterTreeItemModel::ExportHint::Type> hintOverload = std::nullopt, QPsdBlend::Mode groupBlendMode = QPsdBlend::PassThrough) const;

    // Virtual so the MCU plugin can degrade these to no-ops (it cannot render
    // GPU-based blend modes or adjustment shaders).
    virtual void applyBlendModes(Element *element, ImportData *imports) const;
    virtual void applyAdjustmentLayer(const QPsdAbstractLayerItem *item, Element *parent, ImportData *imports) const;

    // Map a Photoshop blend mode to its Qt5Compat.GraphicalEffects.Blend.mode
    // string. Returns empty for modes with no direct mapping (PassThrough,
    // Invalid). Subclasses that don't honour blend modes simply ignore it.
    static QString blendModeString(QPsdBlend::Mode mode);

    // Recursively clear element.layers (the layer.effect chain) so a previously
    // accumulated effect tree won't be serialised. Called when an element is
    // re-wrapped into a different container.
    static void stripLayerEffects(Element &element);

    bool saveTo(const QString &baseName, Element *element, const ImportData &imports, const ExportData &exports) const;

    // ---------- Shared mutable state ---------------------------------------

    mutable int m_blendCounter = 0;
    mutable int m_maskCounter = 0;
    mutable int m_adjustmentCounter = 0;
    mutable bool m_needsBlendShader = false;
    mutable bool m_needsAdjustmentShader = false;
    // True once any saved QML file imports QtQuick.Controls. Lets the Qt for
    // MCUs subclass emit `MCU.qulModules: ["Controls"]` only when needed.
    mutable bool m_usedControls = false;
};

QT_END_NAMESPACE

#endif // QPSDQMLEXPORTERPLUGIN_H
