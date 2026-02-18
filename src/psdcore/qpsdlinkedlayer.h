// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDLINKEDLAYER_H
#define QPSDLINKEDLAYER_H

#include <QtPsdCore/qpsdsection.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdLinkedLayer : public QPsdSection
{
public:
    QPsdLinkedLayer();
    QPsdLinkedLayer(QIODevice *source, quint32 length);
    QPsdLinkedLayer(const QPsdLinkedLayer &other);
    QPsdLinkedLayer &operator=(const QPsdLinkedLayer &other);
    void swap(QPsdLinkedLayer &other) noexcept { d.swap(other.d); }
    ~QPsdLinkedLayer() override;

    struct LinkedFile {
        QByteArray uniqueId;
        QString name;
        QByteArray type;
        QByteArray data;
    };
    QList<LinkedFile> files() const;

    void write(QIODevice *dest) const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

Q_DECLARE_SHARED(QPsdLinkedLayer)

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QPsdLinkedLayer)

#endif // QPSDLINKEDLAYER_H
