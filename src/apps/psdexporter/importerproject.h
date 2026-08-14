// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef IMPORTERPROJECT_H
#define IMPORTERPROJECT_H

#include <QtCore/QString>
#include <QtCore/QVariantMap>

struct ImporterProject
{
    QString key;
    QVariantMap options;

    static ImporterProject fromVariantMap(const QVariantMap &root);
    QVariantMap toVariantMap() const;
};

#endif // IMPORTERPROJECT_H
