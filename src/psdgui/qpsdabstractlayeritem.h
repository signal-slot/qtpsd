// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDABSTRACTLAYERITEM_H
#define QPSDABSTRACTLAYERITEM_H

#include <QtPsdGui/qpsdguiglobal.h>

#include <QtGui/QImage>
#include <QtGui/QPainterPath>

#include <QtPsdCore/qpsdlayerrecord.h>
#include <QtPsdCore/qpsdlinkedlayer.h>
#include <QtPsdCore/qpsdvectormasksetting.h>

QT_BEGIN_NAMESPACE

class QPsdFolderLayerItem;
class QGradient;
class QPsdBorder;
class QPsdPatternFill;

class Q_PSDGUI_EXPORT QPsdAbstractLayerItem
{
public:
    enum Type {
        Text,
        Shape,
        Image,
        Folder,
        Adjustment,
    };
    QPsdAbstractLayerItem(int width, int height);
    QPsdAbstractLayerItem(const QPsdLayerRecord &record);
    QPsdAbstractLayerItem();
    virtual ~QPsdAbstractLayerItem();
    virtual Type type() const = 0;

    QPsdLayerRecord record() const;
    void setRecord(const QPsdLayerRecord &record);

    quint32 id() const;
    QString name() const;
    QColor color() const;
    bool isVisible() const;
    qreal opacity() const;
    qreal fillOpacity() const;
    QRect rect() const;
    QGradient *gradient() const;
    qreal gradientOpacity() const;
    QCborMap dropShadow() const;
    QCborMap innerShadow() const;
    QPsdBorder *border() const;
    QPsdPatternFill *patternFill() const;
    struct PathInfo {
        enum Type {
            None,
            Rectangle,
            RoundedRectangle,
            Path,
        };
        Type type = None;
        QRectF rect;
        qreal radius = 0;
        QPainterPath path;
    };
    PathInfo vectorMask() const;

    struct AutoLayout {
        enum Direction { None, Row, Column };
        enum Align { Min, Center, Max, SpaceBetween, Baseline };
        enum SizingMode { Fixed, Hug, Fill };
        Direction direction = None;
        qreal itemSpacing = 0;
        qreal paddingLeft = 0;
        qreal paddingTop = 0;
        qreal paddingRight = 0;
        qreal paddingBottom = 0;
        Align primaryAxisAlign = Min;
        Align counterAxisAlign = Min;
        SizingMode primaryAxisSizing = Fixed;
        SizingMode counterAxisSizing = Fixed;
        bool isValid() const { return direction != None; }
    };
    struct AutoLayoutChild {
        AutoLayout::SizingMode horizontal = AutoLayout::Fixed;
        AutoLayout::SizingMode vertical = AutoLayout::Fixed;
        qreal grow = 0;
        bool stretchSelf = false;
        bool set = false;
    };
    AutoLayout autoLayout() const;
    void setAutoLayout(const AutoLayout &autoLayout);
    AutoLayoutChild autoLayoutChild() const;
    void setAutoLayoutChild(const AutoLayoutChild &autoLayoutChild);

    // Figma constraints. Per-axis: how a child reacts when the parent frame
    // resizes. Scale is left as Min for now since it requires per-instance
    // proportional metrics that Qt anchors can't express directly.
    struct Constraints {
        enum Axis { Min, Max, Center, Stretch, Scale };
        Axis horizontal = Min;
        Axis vertical = Min;
        bool set = false;
    };
    Constraints constraints() const;
    void setConstraints(const Constraints &constraints);

    // Component / Instance metadata for design-system inputs (Figma).
    // `componentName` non-empty marks this layer as a reusable component
    // master — exporters that support component extraction emit it as a
    // standalone file. `referencedComponent` non-empty marks the layer as
    // an instance reference — the exporter should emit a reference element
    // instead of recursing into children.
    QString componentName() const;
    void setComponentName(const QString &componentName);
    QString referencedComponent() const;
    void setReferencedComponent(const QString &referencedComponent);

    // Design-token (Figma Variables) bindings on this layer's primitive
    // properties. Keys are exporter-facing tags: "fill" (primary fill colour),
    // "stroke" (stroke colour). Values are token names that resolve against
    // QPsdExporterTreeItemModel::designTokens(). The importer initially
    // stores raw Figma variable ids and rewrites them to token names once
    // the full design-tokens map is known.
    QMap<QString, QString> variableBindings() const;
    void setVariableBinding(const QString &property, const QString &token);
    void clearVariableBindings();

    QImage image() const;
    QImage applyGradient(const QImage &image) const;
    QImage transparencyMask() const;
    QImage layerMask() const;
    QRect layerMaskRect() const;
    quint8 layerMaskDefaultColor() const;
    quint8 layerMaskDensity() const;

    QPsdLinkedLayer::LinkedFile linkedFile() const;
    void setLinkedFile(const QPsdLinkedLayer::LinkedFile &linkedFile);

    void setIccProfile(const QByteArray &iccProfile);

    void setId(quint32 id);
    void setName(const QString &name);
    void setVisible(bool visible);
    void setOpacity(qreal opacity);
    void setFillOpacity(qreal opacity);
    void setRect(const QRect &rect);
    void setGradient(QGradient *gradient);
    void setGradientOpacity(qreal opacity);
    void setDropShadow(const QCborMap &shadow);
    void setInnerShadow(const QCborMap &shadow);
    qreal layerBlur() const;
    void setLayerBlur(qreal radius);
    void setImage(const QImage &image);
    void setVectorMask(const PathInfo &info);

    QVariantList effects() const;

protected:
    QPsdAbstractLayerItem::PathInfo parseShape(const QPsdVectorMaskSetting &vms) const;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDABSTRACTLAYERITEM_H
