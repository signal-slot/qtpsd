// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDCOLORMODEDATA_H
#define QPSDCOLORMODEDATA_H

#include <QtPsdCore/qpsdsection.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdColorModeData : public QPsdSection
{
public:
    QPsdColorModeData();
    QPsdColorModeData(QIODevice *source);
    QPsdColorModeData(const QPsdColorModeData &other);
    QPsdColorModeData &operator=(const QPsdColorModeData &other);
    ~QPsdColorModeData() override;

    QByteArray colorData() const;
    void setColorData(const QByteArray &colorData);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDCOLORMODEDATA_H
