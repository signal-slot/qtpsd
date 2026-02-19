// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDTYPETOOLOBJECTSETTING_H
#define QPSDTYPETOOLOBJECTSETTING_H

#include <QtPsdCore/qpsdsection.h>
#include <QtPsdCore/qpsddescriptor.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdTypeToolObjectSetting : public QPsdSection
{
public:
    QPsdTypeToolObjectSetting();
    QPsdTypeToolObjectSetting(QIODevice *source, quint32 *length);
    QPsdTypeToolObjectSetting(const QPsdTypeToolObjectSetting &other);
    QPsdTypeToolObjectSetting &operator=(const QPsdTypeToolObjectSetting &other);
    ~QPsdTypeToolObjectSetting() override;

    QList<qreal> transform() const;
    void setTransform(const QList<qreal> &transform);
    QPsdDescriptor textData() const;
    void setTextData(const QPsdDescriptor &textData);
    QPsdDescriptor warpData() const;
    void setWarpData(const QPsdDescriptor &warpData);
    QRect rect() const;
    void setRect(const QRect &rect);
    QRectF bounds() const;

    QByteArray rawData() const;
    void setRawData(const QByteArray &data);

    void write(QIODevice *dest) const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDTYPETOOLOBJECTSETTING_H
