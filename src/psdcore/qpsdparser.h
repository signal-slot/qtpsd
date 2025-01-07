// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDCORE_H
#define QPSDCORE_H

#include <QtPsdCore/qpsdcoreglobal.h>
#include <QtCore/QSharedDataPointer>

#include <QtPsdCore/qpsdfileheader.h>
#include <QtPsdCore/qpsdcolormodedata.h>
#include <QtPsdCore/qpsdimageresources.h>
#include <QtPsdCore/qpsdlayerandmaskinformation.h>
#include <QtPsdCore/qpsdimagedata.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdParser
{
public:
    QPsdParser();
    QPsdParser(const QPsdParser &other);
    QPsdParser &operator=(const QPsdParser &other);
    QT_MOVE_ASSIGNMENT_OPERATOR_IMPL_VIA_PURE_SWAP(QPsdParser)
    void swap(QPsdParser &other) noexcept { d.swap(other.d); }
    ~QPsdParser();

    /*!
     * Returns the PSD file header information.
     */
    QPsdFileHeader fileHeader() const;

    /*!
     * Returns the color mode data section of the PSD file.
     */
    QPsdColorModeData colorModeData() const;

    /*!
     * Returns the image resources section containing metadata.
     */
    QPsdImageResources imageResources() const;

    /*!
     * Returns layer and mask information section.
     */
    QPsdLayerAndMaskInformation layerAndMaskInformation() const;

    /*!
     * Returns the image data section.
     */
    QPsdImageData imageData() const;

    /*!
     * Loads and parses a PSD file from the specified source path.
     * \param source The path to the PSD file to load.
     */
    void load(const QString &source);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDCORE_H
