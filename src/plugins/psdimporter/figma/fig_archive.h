// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef FIG_ARCHIVE_H
#define FIG_ARCHIVE_H

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QString>

namespace FigArchive {

struct Archive
{
    QHash<QString, QByteArray> zipFiles;
    QByteArray schemaChunk;
    QByteArray dataChunk;
    QByteArray preview;
    QString metaJson;
    quint32 version = 0;
    QString magic;
};

bool readFigFile(const QString &path, Archive *out, QString *error);
bool readFigData(const QByteArray &data, Archive *out, QString *error);

QByteArray decompressDeflateRaw(const QByteArray &in, QString *error);
QByteArray decompressZstd(const QByteArray &in, QString *error);
QByteArray decompressAuto(const QByteArray &in, QString *error);

} // namespace FigArchive

#endif
