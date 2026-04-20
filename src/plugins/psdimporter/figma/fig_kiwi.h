// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef FIG_KIWI_H
#define FIG_KIWI_H

#include <QtCore/QByteArray>
#include <QtCore/QCborValue>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QVector>

namespace FigKiwi {

// Builtin type indices in the extended Figma kiwi dialect.
enum class Builtin : qint8 {
    Bool = 0,
    Byte = 1,
    Int = 2,
    Uint = 3,
    Float = 4,
    String = 5,
    Int64 = 6,
    Uint64 = 7,
};

struct Field
{
    QString name;
    // Either a builtin type (hasBuiltin true) or an index into Schema::definitions.
    bool hasBuiltin = false;
    Builtin builtin = Builtin::Bool;
    int defIndex = -1;   // when !hasBuiltin
    bool isArray = false;
    quint32 value = 0;   // field tag (MESSAGE) or enum value (ENUM)
};

enum class Kind : qint8 {
    Enum = 0,
    Struct = 1,
    Message = 2,
};

struct Definition
{
    QString name;
    Kind kind = Kind::Struct;
    QVector<Field> fields;
    // For enums: value -> name (fast lookup)
    QHash<quint32, QString> enumValues;
};

struct Schema
{
    QVector<Definition> definitions;
    QHash<QString, int> indexByName;
    int messageEntryIndex = -1; // the root MESSAGE def (usually last or matches "Message")
    int indexOf(const QString &name) const { return indexByName.value(name, -1); }
};

// Parse the binary kiwi schema (already decompressed).
bool parseSchema(const QByteArray &raw, Schema *out, QString *error);

// Decode a message given the compiled schema. Returns a QCborMap value, or invalid on error.
// Uses the schema's messageEntryIndex to drive the root message type.
QCborValue decodeMessage(const QByteArray &raw, const Schema &schema, QString *error);

} // namespace FigKiwi

#endif
