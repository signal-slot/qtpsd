// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdexporterplugin.h"

#include <QtCore/QCryptographicHash>

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
        if (hint.type == QPsdExporterTreeItemModel::ExportHint::Merge) {
            const auto groupIndexes = model->groupIndexes(index);
            for (const auto &i : groupIndexes) {
                if (i == index)
                    continue;
                if (model->layerName(i) == hint.componentName) {
                    q->indexMergeMap.insert(i, index);
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
        basename = QString::fromLatin1(QCryptographicHash::hash(hashInput, QCryptographicHash::Sha256).toHex());
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

QT_END_NAMESPACE
