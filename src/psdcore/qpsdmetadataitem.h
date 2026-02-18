// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDMETADATAITEM_H
#define QPSDMETADATAITEM_H

#include <QtPsdCore/qpsdsection.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdMetadataItem : public QPsdSection
{
public:
    QPsdMetadataItem();
    QPsdMetadataItem(QIODevice *source, quint32 *length = nullptr);
    QPsdMetadataItem(const QPsdMetadataItem &other);
    QPsdMetadataItem &operator=(const QPsdMetadataItem &other);
    void swap(QPsdMetadataItem &other) noexcept { d.swap(other.d); }
    ~QPsdMetadataItem() override;

    QByteArray key() const;
    quint8 copyOnSheetDuplication() const;
    QByteArray data() const;

    void write(QIODevice *dest) const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

Q_DECLARE_SHARED(QPsdMetadataItem)

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QPsdMetadataItem)

#endif // QPSDMETADATAITEM_H
