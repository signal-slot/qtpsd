// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdparser.h"

#include <QtCore/QFile>

QT_BEGIN_NAMESPACE

class QPsdParser::Private : public QSharedData
{
public:
    QPsdFileHeader fileHeader;
    QPsdColorModeData colorModeData;
    QPsdImageResources imageResources;
    QPsdLayerAndMaskInformation layerAndMaskInformation;
    QPsdImageData imageData;
};

QPsdParser::QPsdParser()
    : d(new Private)
{}

QPsdParser::QPsdParser(const QPsdParser &other)
    : d(other.d)
{}

QPsdParser &QPsdParser::operator=(const QPsdParser &other)
{
    if (this != &other)
        d.operator=(other.d);
    return *this;
}

QPsdParser::~QPsdParser() = default;

void QPsdParser::load(const QString &psd)
{
    QFile file(psd);
    if (!file.open(QFile::ReadOnly)) {
        qWarning() << file.errorString();
        return;
    }

    d->fileHeader = QPsdFileHeader(&file);
    if (!file.isOpen())
        return;

    d->colorModeData = QPsdColorModeData(&file);
    if (!file.isOpen())
        return;

    d->imageResources = QPsdImageResources(&file);
    if (!file.isOpen())
        return;

    d->layerAndMaskInformation = QPsdLayerAndMaskInformation(&file);
    if (!file.isOpen())
        return;

    d->imageData = QPsdImageData(d->fileHeader, &file);
    if (!file.isOpen())
        return;

    file.close();
}

QPsdFileHeader QPsdParser::fileHeader() const
{
    return d->fileHeader;
}

QPsdColorModeData QPsdParser::colorModeData() const
{
    return d->colorModeData;
}

QPsdImageResources QPsdParser::imageResources() const
{
    return d->imageResources;
}

QPsdLayerAndMaskInformation QPsdParser::layerAndMaskInformation() const
{
    return d->layerAndMaskInformation;
}

QPsdImageData QPsdParser::imageData() const
{
    return d->imageData;
}

QT_END_NAMESPACE
