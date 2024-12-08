// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDDESCRIPTOR_H
#define QPSDDESCRIPTOR_H

#include <QtPsdCore/qpsdsection.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdDescriptor : public QPsdSection
{
public:
    QPsdDescriptor();
    QPsdDescriptor(QIODevice *source, quint32 *length = nullptr);
    QPsdDescriptor(const QPsdDescriptor &other);
    QPsdDescriptor &operator=(const QPsdDescriptor &other);
    void swap(QPsdDescriptor &other) noexcept { d.swap(other.d); }
    ~QPsdDescriptor() override;

    QString name() const;
    QByteArray classID() const;
    QHash<QByteArray, QVariant> data() const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

Q_DECLARE_SHARED(QPsdDescriptor)

Q_PSDCORE_EXPORT QDebug operator<<(QDebug s, const QPsdDescriptor &value);

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QPsdDescriptor)

#endif // QPSDDESCRIPTOR_H
