// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDENUM_H
#define QPSDENUM_H

#include <QtPsdCore/qpsdsection.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdEnum : public QPsdSection
{
public:
    QPsdEnum();
    QPsdEnum(QIODevice *source , quint32 *length);
    QPsdEnum(const QPsdEnum &other);
    QPsdEnum &operator=(const QPsdEnum &other);
    QT_MOVE_ASSIGNMENT_OPERATOR_IMPL_VIA_PURE_SWAP(QPsdEnum)
    void swap(QPsdEnum &other) noexcept { d.swap(other.d); }
    virtual ~QPsdEnum();

    QByteArray type() const;
    QByteArray value() const;

    void write(QIODevice *dest) const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

Q_PSDCORE_EXPORT QDebug operator<<(QDebug s, const QPsdEnum &value);

QT_END_NAMESPACE

Q_DECLARE_SHARED(QPsdEnum)

#endif // QPSDENUM_H
