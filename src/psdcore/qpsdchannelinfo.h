// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDCHANNELINFO_H
#define QPSDCHANNELINFO_H

#include <QtPsdCore/qpsdsection.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdChannelInfo : public QPsdSection
{
public:
    enum ChannelID : qint8 {
        RealUserSuppliedLayerMask = -3,
        UserSuppliedLayerMask = -2,
        TransparencyMask = -1,
        Red = 0,
        Green = 1,
        Blue = 2,
        Alpha = 3,
    };
    QPsdChannelInfo();
    QPsdChannelInfo(QIODevice *source);
    QPsdChannelInfo(const QPsdChannelInfo &other);
    QPsdChannelInfo &operator=(const QPsdChannelInfo &other);
    ~QPsdChannelInfo() override;

    ChannelID id() const;
    quint32 length() const;

    void setId(ChannelID id);
    void setLength(quint32 length);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDCHANNELINFO_H
