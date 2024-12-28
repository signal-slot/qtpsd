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
    };
    QPsdAbstractLayerItem(int width, int height);
    QPsdAbstractLayerItem(const QPsdLayerRecord &record);
    QPsdAbstractLayerItem();
    virtual ~QPsdAbstractLayerItem();
    virtual Type type() const = 0;

    QPsdLayerRecord record() const;
    QPsdFolderLayerItem *parent() const;
    void setParent(QPsdFolderLayerItem *parent);

    quint32 id() const;
    QString name() const;
    QColor color() const;
    bool isVisible() const;
    qreal opacity() const;
    QRect rect() const;
    QGradient *gradient() const;
    QCborMap dropShadow() const;
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
    QImage image() const;
    QImage transparencyMask() const;

    QT_DEPRECATED
    QList<QPsdAbstractLayerItem *> group() const;
    void setGroup(const QList<QPsdAbstractLayerItem *> &group);

    QPsdLinkedLayer::LinkedFile linkedFile() const;
    void setLinkedFile(const QPsdLinkedLayer::LinkedFile &linkedFile);

    QT_DEPRECATED
    QPsdAbstractLayerItem *maskItem() const;
    void setMaskItem(QPsdAbstractLayerItem *maskItem);

    QVariantList effects() const;

    struct ExportHint {
        enum Type {
            Embed,
            Merge,
            Custom,
            Native,
            Skip,
            None,
        };
        enum NativeComponent {
            Container,
            TouchArea,
            Button,
            Button_Highlighted,
        };

        QString id;
        Type type = Embed;
        QString componentName;
        NativeComponent baseElement = Container;
        bool visible = true;
        QSet<QString> properties;

        bool isDefaultValue() const {
            return id.isEmpty() && type == Embed && componentName.isEmpty() && baseElement == Container;
        }

        static NativeComponent nativeName2Code(const QString &name) {

#define IF(x) if (name == u###x##_s) \
            return x; \
            else
            IF(Container)
            IF(TouchArea)
            IF(Button)
            IF(Button_Highlighted)
#undef IF
            {
                qWarning() << name << "is not a valid NativeComponent";
            }
            return Container;
        }
        static QString nativeCode2Name(NativeComponent code) {
            auto parentheses = [](const QString &s) {
                QString ret = s;
                if (ret.count('_'_L1) == 1) {
                    const auto pos = ret.indexOf('_'_L1);
                    ret.replace(pos, 1, '('_L1);
                    ret.append(')'_L1);
                }
                return ret;
            };
            switch (code) {
#define CASE(x) case x: return parentheses(u###x##_s)
            CASE(Container);
            CASE(TouchArea);
            CASE(Button);
            CASE(Button_Highlighted);
#undef CASE
            default:
                qWarning() << code << "is not a valid NativeComponent";
                break;
            }
            return QString();
        }
    };

    ExportHint exportHint() const;
    void setExportHint(const ExportHint &exportHint) const; // mutable

protected:
    QPsdAbstractLayerItem::PathInfo parseShape(const QPsdVectorMaskSetting &vms) const;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDABSTRACTLAYERITEM_H
