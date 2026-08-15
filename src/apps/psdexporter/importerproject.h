// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef IMPORTERPROJECT_H
#define IMPORTERPROJECT_H

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QVariantMap>

struct ImporterProject
{
    QString key;
    QVariantMap options;
    QList<QVariantMap> pageHints;

    static ImporterProject fromVariantMap(const QVariantMap &root);
    QVariantMap toVariantMap() const;

    void makeSourceRelativeTo(const QString &fileName);
    void makeSourceResolvedFrom(const QString &fileName);
};

#endif // IMPORTERPROJECT_H
