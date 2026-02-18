// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDSECTIONDIVIDERSETTING_H
#define QPSDSECTIONDIVIDERSETTING_H

#include <QtPsdCore/qpsdsection.h>
#include <QtPsdCore/qpsdblend.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdSectionDividerSetting : public QPsdSection
{
public:
    enum Type {
        AnyOtherTypeOfLayer = 0,
        OpenFolder = 1,
        ClosedFolder = 2,
        BoundingSectionDivider = 3,
    };
    enum SubType {
        Normal = 0,
        SceneGroup = 1,
    };

    QPsdSectionDividerSetting();
    QPsdSectionDividerSetting(QIODevice *source, quint32 *length);
    QPsdSectionDividerSetting(const QPsdSectionDividerSetting &other);
    QPsdSectionDividerSetting &operator=(const QPsdSectionDividerSetting &other);
    ~QPsdSectionDividerSetting() override;

    Type type() const;
    QPsdBlend::Mode key() const;
    SubType subType() const;

    void write(QIODevice *dest) const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

Q_DECLARE_SHARED(QPsdSectionDividerSetting)

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QPsdSectionDividerSetting)


#endif // QPSDSECTIONDIVIDERSETTING_H
