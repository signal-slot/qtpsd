// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDPLACEDLAYER_H
#define QPSDPLACEDLAYER_H

#include <QtPsdCore/qpsdsection.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdPlacedLayer : public QPsdSection
{
public:
    QPsdPlacedLayer();
    QPsdPlacedLayer(QIODevice *source, quint32 length);
    QPsdPlacedLayer(const QPsdPlacedLayer &other);
    QPsdPlacedLayer &operator=(const QPsdPlacedLayer &other);
    void swap(QPsdPlacedLayer &other) noexcept { d.swap(other.d); }
    ~QPsdPlacedLayer() override;

    QByteArray uniqueId() const;

    void write(QIODevice *dest) const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

Q_DECLARE_SHARED(QPsdPlacedLayer)

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QPsdPlacedLayer)

#endif // QPSDPLACEDLAYER_H
