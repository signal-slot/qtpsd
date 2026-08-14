// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "importerproject.h"

using namespace Qt::StringLiterals;

ImporterProject ImporterProject::fromVariantMap(const QVariantMap &root)
{
    return {
        root.value("key"_L1).toString(),
        root.value("options"_L1).toMap(),
    };
}

QVariantMap ImporterProject::toVariantMap() const
{
    return {
        { "key"_L1, key },
        { "options"_L1, options },
    };
}
