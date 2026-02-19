// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDEFFECTSLAYER_H
#define QPSDEFFECTSLAYER_H

#include <QtPsdCore/qpsdsection.h>
#include <QtPsdCore/qpsdabstracteffect.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdEffectsLayer : public QPsdSection
{
public:
    QPsdEffectsLayer();
    QPsdEffectsLayer(QIODevice *source, quint32 *length = nullptr);
    QPsdEffectsLayer(const QPsdEffectsLayer &other);
    QPsdEffectsLayer &operator=(const QPsdEffectsLayer &other);
    void swap(QPsdEffectsLayer &other) noexcept { d.swap(other.d); }
    ~QPsdEffectsLayer() override;

    QVariantList effects() const;

    QByteArray rawData() const;
    void setRawData(const QByteArray &data);

    void write(QIODevice *dest) const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

Q_DECLARE_SHARED(QPsdEffectsLayer)

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QPsdEffectsLayer)

#endif // QPSDEFFECTSLAYER_H
