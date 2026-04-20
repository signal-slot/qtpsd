// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef FIG_TO_REST_H
#define FIG_TO_REST_H

#include <QtCore/QByteArray>
#include <QtCore/QCborValue>
#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

namespace FigToRest {

// Convert a decoded kiwi NODE_CHANGES message into a REST-API-shaped file JSON
// compatible with the existing FigmaLayerTreeItemModel::buildFromFigmaJson() path.
// `metaJson` is the contents of meta.json inside the .fig ZIP (used for file name).
// On return: `out` contains a JSON object with keys: name, lastModified, document{...}.
// Optional: `imagesByHash` will be populated from commandsBlob decoding only — the
// caller is expected to supply real fill images separately via model->setNodeImage().
bool convertMessageToFileJson(const QCborValue &message,
                              const QString &metaJson,
                              QJsonObject *out,
                              QString *error);

// Decode a kiwi commandsBlob payload into an SVG path data string.
QString commandsBlobToSvgPath(const QByteArray &blobBytes);

// Decode a kiwi vectorNetworkBlob payload into an SVG path data string.
// Returns empty if the blob is malformed or has no renderable geometry.
QString vectorNetworkBlobToSvgPath(const QByteArray &blobBytes);

} // namespace FigToRest

#endif
