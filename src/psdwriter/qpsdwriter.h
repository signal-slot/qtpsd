// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDWRITER_H
#define QPSDWRITER_H

#include <QtPsdWriter/qpsdwriterglobal.h>

#include <QtPsdCore/qpsdfileheader.h>
#include <QtPsdCore/qpsdcolormodedata.h>
#include <QtPsdCore/qpsdimageresources.h>
#include <QtPsdCore/qpsdlayerandmaskinformation.h>
#include <QtPsdCore/qpsdimagedata.h>

QT_BEGIN_NAMESPACE

class Q_PSDWRITER_EXPORT QPsdWriter
{
public:
    QPsdWriter();
    QPsdWriter(const QPsdWriter &other);
    QPsdWriter &operator=(const QPsdWriter &other);
    QT_MOVE_ASSIGNMENT_OPERATOR_IMPL_VIA_PURE_SWAP(QPsdWriter)
    void swap(QPsdWriter &other) noexcept { d.swap(other.d); }
    ~QPsdWriter();

    QPsdFileHeader fileHeader() const;
    void setFileHeader(const QPsdFileHeader &fileHeader);

    QPsdColorModeData colorModeData() const;
    void setColorModeData(const QPsdColorModeData &colorModeData);

    QPsdImageResources imageResources() const;
    void setImageResources(const QPsdImageResources &imageResources);

    QPsdLayerAndMaskInformation layerAndMaskInformation() const;
    void setLayerAndMaskInformation(const QPsdLayerAndMaskInformation &info);

    QPsdImageData imageData() const;
    void setImageData(const QPsdImageData &imageData);

    bool write(QIODevice *device) const;
    bool write(const QString &filePath) const;

    bool useRawBytes() const;
    void setUseRawBytes(bool enable);

    QString errorString() const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDWRITER_H
