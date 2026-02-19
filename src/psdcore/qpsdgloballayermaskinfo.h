// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDGLOBALLAYERMASKINFO_H
#define QPSDGLOBALLAYERMASKINFO_H

#include <QtPsdCore/qpsdsection.h>
#include <QtPsdCore/qpsdcolorspace.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdGlobalLayerMaskInfo : public QPsdSection
{
public:
    enum Kind : quint8 {
        ColorSelected = 0,
        ColorProtected = 1,
        UseValueStoredPerLayer = 128,
    };
    QPsdGlobalLayerMaskInfo();
    QPsdGlobalLayerMaskInfo(QIODevice *source);
    QPsdGlobalLayerMaskInfo(const QPsdGlobalLayerMaskInfo &other);
    QPsdGlobalLayerMaskInfo &operator=(const QPsdGlobalLayerMaskInfo &other);
    ~QPsdGlobalLayerMaskInfo() override;

    quint32 length() const;
    quint16 overlayColorSpace() const;
    QString color() const;
    QPsdColorSpace colorSpace() const;
    quint16 opacity() const;
    Kind kind() const;
    QByteArray rawData() const;

    void setOverlayColorSpace(quint16 overlayColorSpace);
    void setColor(const QString &color);
    void setColorSpace(const QPsdColorSpace &colorSpace);
    void setOpacity(quint16 opacity);
    void setKind(Kind kind);
    void setRawData(const QByteArray &rawData);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDGLOBALLAYERMASKINFO_H
