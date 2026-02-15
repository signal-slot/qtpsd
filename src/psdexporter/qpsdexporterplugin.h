// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDEXPORTERPLUGIN_H
#define QPSDEXPORTERPLUGIN_H

#include <QtPsdExporter/qpsdexporterglobal.h>
#include <QtPsdExporter/qpsdexportertreeitemmodel.h>
#include <QtPsdExporter/qpsdimagestore.h>

#include <QtPsdCore/qpsdabstractplugin.h>
#include <QtPsdGui/qpsdfolderlayeritem.h>
#include <QtPsdGui/qpsdtextlayeritem.h>
#include <QtPsdGui/qpsdshapelayeritem.h>
#include <QtPsdGui/qpsdimagelayeritem.h>

#include <QtCore/QDir>
#include <QtCore/QMimeDatabase>
#include <QtGui/QIcon>

QT_BEGIN_NAMESPACE

#define QPsdExporterFactoryInterface_iid "org.qt-project.Qt.QPsdExporterFactoryInterface"

class Q_PSDEXPORTER_EXPORT QPsdExporterPlugin : public QPsdAbstractPlugin
{
    Q_OBJECT
public:
    enum ExportType {
        File,
        Directory,
    };
    explicit QPsdExporterPlugin(QObject *parent = nullptr);
    virtual ~QPsdExporterPlugin();

    virtual int priority() const = 0;
    virtual QIcon icon() const { return QIcon(); }
    virtual QString name() const = 0;
    virtual ExportType exportType() const = 0;
    virtual QHash<QString, QString> filters() const { return {}; }

    virtual bool exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const = 0;

    const QPsdExporterTreeItemModel *model() const;
    void setModel(const QPsdExporterTreeItemModel *model) const;

    static QByteArrayList keys() {
        return QPsdAbstractPlugin::keys<QPsdExporterPlugin>(QPsdExporterFactoryInterface_iid, "psdexporter");
    }
    static QPsdExporterPlugin *plugin(const QByteArray &key) {
        return QPsdAbstractPlugin::plugin<QPsdExporterPlugin>(QPsdExporterFactoryInterface_iid, "psdexporter", key);
    }

    static QString toUpperCamelCase(const QString &str, const QString &separator = QString());
protected:
    static QString toLowerCamelCase(const QString &str);
    static QString toSnakeCase(const QString &str);
    static QString toKebabCase(const QString &str);

    static QString imageFileName(const QString &name, const QString &format, const QByteArray &uniqueId = {});
    bool generateMaps() const;

    bool initializeExport(const QPsdExporterTreeItemModel *model,
                          const QString &to,
                          const QVariantMap &hint,
                          const QString &imageSubDir = {}) const;

    static void applyFillOpacity(QImage &image, qreal fillOpacity);

    QString saveLayerImage(const QPsdImageLayerItem *image) const;

    struct OpacityResult {
        qreal opacity;
        bool hasEffects;
    };
    static OpacityResult computeEffectiveOpacity(const QPsdAbstractLayerItem *item);

    QRect adjustRectForMerge(const QModelIndex &index, QRect rect) const;

    static QRect computeTextBounds(const QPsdTextLayerItem *text);

    static QRectF adjustRectForStroke(const QRectF &rect,
                                       QPsdShapeLayerItem::StrokeAlignment alignment,
                                       qreal strokeWidth);

    struct HAlignStrings {
        QString left;
        QString right;
        QString center;
        QString justify;
    };

    struct VAlignStrings {
        QString top;
        QString bottom;
        QString center;
    };

    static QString horizontalAlignmentString(Qt::Alignment alignment,
                                              const HAlignStrings &strings);
    static QString verticalAlignmentString(Qt::Alignment alignment,
                                            const VAlignStrings &strings);

    void writeLicenseHeader(QTextStream &out, const QString &commentPrefix = u"// "_s) const;

protected:
    static QMimeDatabase mimeDatabase;

    mutable QDir dir;
    mutable QPsdImageStore imageStore;
    mutable qreal horizontalScale = 1.0;
    mutable qreal verticalScale = 1.0;
    mutable qreal unitScale = 1.0;
    mutable qreal fontScaleFactor = 1.0;
    mutable bool makeCompact = false;
    mutable bool imageScaling = false;
    mutable QString licenseText;

    mutable QHash<const QPersistentModelIndex, QRect> childrenRectMap;
    mutable QHash<const QPersistentModelIndex, QRect> indexRectMap;
    mutable QMultiMap<const QPersistentModelIndex, QPersistentModelIndex> indexMergeMap;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDEXPORTERPLUGIN_H
