// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdexporterplugin.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QtMath>
#include <QtGui/QFontMetrics>

QT_BEGIN_NAMESPACE

QMimeDatabase QPsdExporterPlugin::mimeDatabase;

class QPsdExporterPlugin::Private {
public:
    Private(QPsdExporterPlugin *parent);
    ~Private() = default;
        
    void generateChildrenRectMap(const QPersistentModelIndex &index) const;
    void generateIndexMap(const QPersistentModelIndex &index, const QPoint &topLeft) const;

    QPsdExporterPlugin *q;
    const QPsdExporterTreeItemModel *model = nullptr;
};

QPsdExporterPlugin::Private::Private(QPsdExporterPlugin *parent) : q(parent)
{}

void QPsdExporterPlugin::Private::generateChildrenRectMap(const QPersistentModelIndex &index) const
{
    QRect childrenRect;

    bool hasChildren = model->hasChildren(index);
    if (hasChildren) {
        for (int i = model->rowCount(index) -1; i >= 0; i--) {
            QModelIndex childIndex = model->index(i, 0, index);
            generateChildrenRectMap(childIndex);

            childrenRect |= q->childrenRectMap.value(childIndex);
        }
    }

    if (index.isValid()) {
        if (hasChildren) {
            q->childrenRectMap.insert(index, childrenRect);
        } else {
            q->childrenRectMap.insert(index, model->rect(index));
        }
    }
}

void QPsdExporterPlugin::Private::generateIndexMap(const QPersistentModelIndex &index, const QPoint &topLeft) const
{
    QPoint childTopLeft = topLeft;
    if (index.isValid()) {
        QRect contentRect = q->childrenRectMap.value(index);
        q->indexRectMap.insert(index, contentRect.translated(-topLeft));
        
        childTopLeft = contentRect.topLeft();

        const auto hint = model->layerHint(index);
        // New format: Button (Native) pulls text/image from sibling/nephew layers
        if (hint.type == QPsdExporterTreeItemModel::ExportHint::Native
            && (hint.baseElement == QPsdExporterTreeItemModel::ExportHint::Button
                || hint.baseElement == QPsdExporterTreeItemModel::ExportHint::Button_Highlighted)
            && (!hint.textSource.isEmpty() || !hint.imageSource.isEmpty())) {
            auto tryMerge = [&](const QModelIndex &i) {
                const auto name = model->layerName(i);
                if (name == hint.textSource || name == hint.imageSource)
                    q->indexMergeMap.insert(index, i);
            };
            const auto parentIndex = model->parent(index);
            for (int si = 0; si < model->rowCount(parentIndex); ++si) {
                const auto i = model->index(si, 0, parentIndex);
                if (i == index)
                    continue;
                tryMerge(i);
                for (int ci = 0; ci < model->rowCount(i); ++ci)
                    tryMerge(model->index(ci, 0, i));
            }
        }
        // Backward compat: old Merged + componentName (push model)
        if (hint.type == QPsdExporterTreeItemModel::ExportHint::Merged
            && !hint.componentName.isEmpty()) {
            const auto parentIndex = model->parent(index);
            for (int si = 0; si < model->rowCount(parentIndex); ++si) {
                const auto i = model->index(si, 0, parentIndex);
                if (i == index)
                    continue;
                if (model->layerName(i) == hint.componentName) {
                    q->indexMergeMap.insert(i, index);  // target ← source
                }
            }
        }
    }

    for (int i = 0; i < model->rowCount(index); i++) {
        QModelIndex childIndex = model->index(i, 0, index);

        generateIndexMap(childIndex, childTopLeft);
    }
}

QPsdExporterPlugin::QPsdExporterPlugin(QObject *parent)
    : QPsdAbstractPlugin(parent), d(new Private(this))
{}

QPsdExporterPlugin::~QPsdExporterPlugin()
{}

const QPsdExporterTreeItemModel *QPsdExporterPlugin::model() const
{
    return d->model;
}

void QPsdExporterPlugin::setModel(const QPsdExporterTreeItemModel *model) const
{
    d->model = model;
}

namespace {
static const QRegularExpression notPrintable("[^a-zA-Z0-9]"_L1);
}

QString QPsdExporterPlugin::toUpperCamelCase(const QString &str, const QString &separator)
{
    QString s = str;
    s.replace(QRegularExpression("([a-z0-9])([A-Z])"_L1), "\\1 \\2"_L1);
    QStringList parts = s.split(notPrintable, Qt::SkipEmptyParts);
    for (auto &part : parts) {
        part = part.toLower();
        part[0] = part[0].toUpper();
    }
    return parts.join(separator);
}

QString QPsdExporterPlugin::toLowerCamelCase(const QString &str)
{
    QString s = str;
    s.replace(QRegularExpression("([a-z0-9])([A-Z])"_L1), "\\1 \\2"_L1);
    QStringList parts = s.split(notPrintable, Qt::SkipEmptyParts);
    bool first = true;
    for (auto &part : parts) {
        part = part.toLower();
        if (first) {
            first = false;
        } else {
            part[0] = part[0].toUpper();
        }
    }
    return parts.join(QString());
}

QString QPsdExporterPlugin::toSnakeCase(const QString &str)
{
    QString s = str;
    s.replace(QRegularExpression("([a-z0-9])([A-Z])"_L1), "\\1 \\2"_L1);
    QStringList parts = s.split(notPrintable, Qt::SkipEmptyParts);
    return parts.join("_"_L1).toLower();
}

QString QPsdExporterPlugin::toKebabCase(const QString &str)
{
    QString s = str;
    s.replace(QRegularExpression("([a-z0-9])([A-Z])"_L1), "\\1 \\2"_L1);
    QStringList parts = s.split(notPrintable, Qt::SkipEmptyParts);
    return parts.join("-"_L1).toLower();
}

QString QPsdExporterPlugin::imageFileName(const QString &name, const QString &format, const QByteArray &uniqueId)
{
    QFileInfo fileInfo(name);
    QString suffix = fileInfo.suffix();
    QString basename = fileInfo.completeBaseName();

    QString snakeName = toSnakeCase(basename);
    if (snakeName.length() < basename.length()) {
        QByteArray hashInput = basename.toUtf8();
        if (!uniqueId.isEmpty())
            hashInput += uniqueId;
        basename = QString::fromLatin1(QCryptographicHash::hash(hashInput, QCryptographicHash::Sha256).toHex().left(16));
    } else {
        if (suffix.isEmpty()) {
            return u"%1.%2"_s.arg(snakeName, format.toLower());
        }

        if (suffix.compare(format, Qt::CaseInsensitive) == 0) {
            return u"%1.%2"_s.arg(basename, suffix);
        }

        if ((format.compare("jpg"_L1, Qt::CaseInsensitive) || format.compare("jpeg"_L1, Qt::CaseInsensitive))
            && (suffix.compare("jpg"_L1, Qt::CaseInsensitive) || suffix.compare("jpeg"_L1, Qt::CaseInsensitive))) {
            return u"%1.%2"_s.arg(basename, suffix);
        }

        basename = snakeName;
    }

    return u"%1.%2"_s.arg(basename, format.toLower());
}

bool QPsdExporterPlugin::generateMaps() const
{
    childrenRectMap.clear();
    indexRectMap.clear();
    indexMergeMap.clear();

    d->generateChildrenRectMap({});
    d->generateIndexMap({}, QPoint(0, 0));

    return true;
}

QVariantMap QPsdExporterPlugin::ExportConfig::toVariantMap() const
{
    QVariantMap map;
    if (!targetSize.isEmpty())
        map.insert("resolution"_L1, targetSize);
    map.insert("fontScaleFactor"_L1, fontScaleFactor);
    map.insert("makeCompact"_L1, makeCompact);
    map.insert("imageScaling"_L1, imageScaling);
    if (!licenseText.isEmpty())
        map.insert("licenseText"_L1, licenseText);
    return map;
}

QPsdExporterPlugin::ExportConfig QPsdExporterPlugin::ExportConfig::fromVariantMap(const QVariantMap &map)
{
    ExportConfig config;
    config.targetSize = map.value("resolution"_L1).toSize();
    config.fontScaleFactor = map.value("fontScaleFactor"_L1, 1.0).toReal();
    config.makeCompact = map.value("makeCompact"_L1, false).toBool();
    config.imageScaling = map.value("imageScaling"_L1, false).toBool();
    config.licenseText = map.value("licenseText"_L1).toString();
    return config;
}

bool QPsdExporterPlugin::initializeExport(const QPsdExporterTreeItemModel *model,
                                          const QString &to,
                                          const ExportConfig &config,
                                          const QString &imageSubDir) const
{
    setModel(model);
    dir = QDir(to);

    const QSize originalSize = model->size();
    const QSize targetSize = config.targetSize.isEmpty() ? originalSize : config.targetSize;
    horizontalScale = targetSize.width() / qreal(originalSize.width());
    verticalScale = targetSize.height() / qreal(originalSize.height());
    unitScale = std::min(horizontalScale, verticalScale);
    fontScaleFactor = config.fontScaleFactor * verticalScale;
    makeCompact = config.makeCompact;
    imageScaling = config.imageScaling;
    licenseText = config.licenseText;

    if (!imageSubDir.isEmpty()) {
        imageStore = QPsdImageStore(dir, imageSubDir);
    }

    return generateMaps();
}

void QPsdExporterPlugin::applyFillOpacity(QImage &image, qreal fillOpacity)
{
    if (fillOpacity >= 1.0)
        return;
    image = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        QRgb *scanLine = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const int alpha = qAlpha(scanLine[x]);
            if (alpha > 0) {
                const int newAlpha = qRound(alpha * fillOpacity);
                scanLine[x] = qRgba(qRed(scanLine[x]), qGreen(scanLine[x]), qBlue(scanLine[x]), newAlpha);
            }
        }
    }
}

QString QPsdExporterPlugin::saveLayerImage(const QPsdImageLayerItem *image) const
{
    const qreal fillOpacity = image->fillOpacity();
    const bool hasEffects = !image->dropShadow().isEmpty() || !image->effects().isEmpty() || image->border();
    const bool needsFillOpacity = hasEffects && fillOpacity < 1.0;

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
                applyFillOpacity(qimage, fillOpacity);
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
            applyFillOpacity(qimage, fillOpacity);
        }
        qimage = image->applyGradient(qimage);
        name = imageStore.save(imageFileName(image->name(), "PNG"_L1), qimage, "PNG");
    }

    return name;
}

QPsdExporterPlugin::OpacityResult QPsdExporterPlugin::computeEffectiveOpacity(const QPsdAbstractLayerItem *item)
{
    const qreal opacity = item->opacity();
    const qreal fillOpacity = item->fillOpacity();
    const bool hasEffects = !item->dropShadow().isEmpty() || !item->effects().isEmpty();
    const qreal combinedOpacity = hasEffects ? opacity : (opacity * fillOpacity);
    return { combinedOpacity, hasEffects };
}

std::optional<qreal> QPsdExporterPlugin::displayOpacity(const QPsdAbstractLayerItem *item)
{
    const auto [combinedOpacity, hasEffects] = computeEffectiveOpacity(item);
    if (hasEffects) {
        if (item->opacity() < 1.0)
            return item->opacity();
    } else {
        if (combinedOpacity < 1.0)
            return combinedOpacity;
    }
    return std::nullopt;
}

QRect QPsdExporterPlugin::adjustRectForMerge(const QModelIndex &index, QRect rect) const
{
    if (model()->layerHint(index).type == QPsdExporterTreeItemModel::ExportHint::Merged) {
        auto parentIndex = indexMergeMap.key(index);
        while (parentIndex.isValid()) {
            const auto *parent = model()->layerItem(parentIndex);
            rect.translate(-parent->rect().topLeft());
            parentIndex = model()->parent(parentIndex);
        }
    }
    return rect;
}

QRect QPsdExporterPlugin::computeBaseRect(const QModelIndex &index, QRect rectBounds) const
{
    const auto *item = model()->layerItem(index);
    QRect rect;
    if (rectBounds.isEmpty()) {
        rect = item->rect();
        if (makeCompact) {
            rect = indexRectMap.value(index);
        }
    } else {
        rect = rectBounds;
    }
    return adjustRectForMerge(index, rect);
}

QRect QPsdExporterPlugin::computeTextBounds(const QPsdTextLayerItem *text)
{
    if (text->textType() == QPsdTextLayerItem::TextType::ParagraphText) {
        return text->bounds().toRect();
    }
    const auto runs = text->runs();
    if (runs.isEmpty()) {
        return text->bounds().toRect();
    }
    const auto &firstRun = runs.first();
    QFont metricsFont = firstRun.font;
    metricsFont.setPixelSize(qRound(firstRun.font.pointSizeF()));
    QFontMetrics fm(metricsFont);
    QRectF adjustedBounds = text->bounds();
    adjustedBounds.setY(text->textOrigin().y() - fm.ascent());
    adjustedBounds.setHeight(fm.height());
    return adjustedBounds.toRect();
}

QRectF QPsdExporterPlugin::adjustRectForStroke(const QRectF &rect,
                                                QPsdShapeLayerItem::StrokeAlignment alignment,
                                                qreal strokeWidth)
{
    switch (alignment) {
    case QPsdShapeLayerItem::StrokeCenter:
        return rect.adjusted(-strokeWidth / 2, -strokeWidth / 2,
                              strokeWidth / 2, strokeWidth / 2);
    case QPsdShapeLayerItem::StrokeOutside:
        return rect.adjusted(-strokeWidth, -strokeWidth,
                              strokeWidth, strokeWidth);
    case QPsdShapeLayerItem::StrokeInside:
    default:
        return rect;
    }
}

QString QPsdExporterPlugin::horizontalAlignmentString(Qt::Alignment alignment,
                                                       const HAlignStrings &strings)
{
    switch (static_cast<Qt::Alignment>(alignment & Qt::AlignHorizontal_Mask)) {
    case Qt::AlignRight:   return strings.right;
    case Qt::AlignHCenter: return strings.center;
    case Qt::AlignJustify:
        return strings.justify.isEmpty() ? strings.left : strings.justify;
    default:               return strings.left;
    }
}

QString QPsdExporterPlugin::verticalAlignmentString(Qt::Alignment alignment,
                                                     const VAlignStrings &strings)
{
    switch (static_cast<Qt::Alignment>(alignment & Qt::AlignVertical_Mask)) {
    case Qt::AlignTop:     return strings.top;
    case Qt::AlignBottom:  return strings.bottom;
    case Qt::AlignVCenter: return strings.center;
    default:               return {};
    }
}

const QGradient *QPsdExporterPlugin::effectiveGradient(const QPsdShapeLayerItem *item)
{
    if (item->gradient())
        return item->gradient();
    return item->brush().gradient();
}

qreal QPsdExporterPlugin::computeStrokeWidth(const QPen &pen, qreal unitScale)
{
    return std::max(1.0, pen.width() * unitScale);
}

std::optional<QPsdExporterPlugin::DropShadowInfo> QPsdExporterPlugin::parseDropShadow(
    const QCborMap &dropShadow)
{
    if (dropShadow.isEmpty())
        return std::nullopt;

    DropShadowInfo info;
    info.color = QColor(dropShadow.value("color"_L1).toString());
    info.color.setAlphaF(dropShadow.value("opacity"_L1).toDouble());
    info.angleRad = dropShadow.value("angle"_L1).toDouble() * M_PI / 180.0;
    info.distance = dropShadow.value("distance"_L1).toDouble();
    info.spread = dropShadow.value("spread"_L1).toDouble();
    info.blur = dropShadow.value("size"_L1).toDouble();
    return info;
}

QPointF QPsdExporterPlugin::dropShadowOffset(const DropShadowInfo &shadow, bool flipAngle)
{
    const auto angle = flipAngle ? (M_PI - shadow.angleRad) : shadow.angleRad;
    return QPointF(std::cos(angle) * shadow.distance,
                   std::sin(angle) * shadow.distance);
}

QList<QPsdExporterPlugin::PathCommand> QPsdExporterPlugin::pathToCommands(
    const QPainterPath &path, qreal hScale, qreal vScale)
{
    QList<PathCommand> commands;
    qreal c1x, c1y, c2x, c2y;
    int control = 1;

    for (int i = 0; i < path.elementCount(); i++) {
        const auto point = path.elementAt(i);
        const auto x = point.x * hScale;
        const auto y = point.y * vScale;
        switch (point.type) {
        case QPainterPath::MoveToElement:
            commands.append({PathCommand::MoveTo, x, y});
            break;
        case QPainterPath::LineToElement:
            commands.append({PathCommand::LineTo, x, y});
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
                commands.append({PathCommand::CubicTo, x, y, c1x, c1y, c2x, c2y});
                break;
            }
            break;
        }
    }
    commands.append(PathCommand{PathCommand::Close});
    return commands;
}

bool QPsdExporterPlugin::isFilledRect(const QPsdAbstractLayerItem::PathInfo &path,
                                       const QPsdShapeLayerItem *shape)
{
    return path.rect.topLeft() == QPointF(0, 0)
        && path.rect.size() == shape->rect().size();
}

void QPsdExporterPlugin::writeLicenseHeader(QTextStream &out, const QString &commentPrefix) const
{
    if (licenseText.isEmpty())
        return;
    const QStringList lines = licenseText.split(u'\n');
    for (const QString &line : lines) {
        out << commentPrefix << line << "\n";
    }
    out << "\n";
}

QT_END_NAMESPACE
