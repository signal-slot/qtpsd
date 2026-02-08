// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDFONTMAPPER_H
#define QPSDFONTMAPPER_H

#include <QtPsdGui/qpsdguiglobal.h>

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtGui/QFont>

QT_BEGIN_NAMESPACE

class Q_PSDGUI_EXPORT QPsdFontMapper
{
public:
    static QPsdFontMapper *instance();

    // Global mappings (persistent via QSettings)
    void setGlobalMapping(const QString &fromFont, const QString &toFont);
    void removeGlobalMapping(const QString &fromFont);
    QHash<QString, QString> globalMappings() const;

    // Per-PSD context mappings (stored in hint file)
    void setContextMappings(const QString &psdPath, const QHash<QString, QString> &mappings);
    QHash<QString, QString> contextMappings(const QString &psdPath) const;
    void clearContextMappings(const QString &psdPath);

    // Main API - resolves font with priority: per-PSD > global > automatic
    QFont resolveFont(const QString &fontName, const QString &psdPath = QString()) const;

    // Global persistence (QSettings)
    void loadGlobalMappings();
    void saveGlobalMappings() const;

    // Per-PSD persistence (hint file integration)
    void loadFromHint(const QString &psdPath, const QJsonObject &fontMapping);
    QJsonObject toHint(const QString &psdPath) const;

    // Clear all caches (useful for testing)
    void clearCache();

private:
    QPsdFontMapper();
    ~QPsdFontMapper();
    Q_DISABLE_COPY_MOVE(QPsdFontMapper)

    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDFONTMAPPER_H
