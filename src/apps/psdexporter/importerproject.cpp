// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "importerproject.h"

using namespace Qt::StringLiterals;

namespace {

QList<QVariantMap> toVariantMapList(const QVariantList &varList)
{
    QList<QVariantMap> list;
    list.reserve(varList.size());
    for (const auto &var : varList) {
        list.append(var.toMap());
    }
    return list;
}

QVariantList toVariantList(const QList<QVariantMap> &list)
{
    QVariantList varList;
    varList.reserve(list.size());
    for (const auto &item : list) {
        varList.append(item);
    }
    return varList;
}

} // namespace

ImporterProject ImporterProject::fromVariantMap(const QVariantMap &root)
{
    return {
        root.value("key"_L1).toString(),
        root.value("options"_L1).toMap(),
        toVariantMapList(root.value("pageHints"_L1).toList()),
    };
}

QVariantMap ImporterProject::toVariantMap() const
{
    return {
        { "key"_L1, key },
        { "options"_L1, options },
        { "pageHints"_L1, toVariantList(pageHints) },
    };
}
