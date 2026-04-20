// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "fig_kiwi.h"

#include <QtCore/QCborArray>
#include <QtCore/QCborMap>
#include <QtCore/QString>

#include <cstring>
#include <limits>

namespace FigKiwi {

namespace {

class Reader
{
public:
    Reader(const QByteArray &buf, QString *error)
        : d(reinterpret_cast<const quint8 *>(buf.constData()))
        , end(d + buf.size())
        , err(error)
    {}

    bool fail(const QString &m) { if (err && err->isEmpty()) *err = m; hasFailed = true; return false; }
    bool failed() const { return hasFailed; }

    bool ensure(qsizetype n)
    {
        if (d + n > end) return fail(QStringLiteral("unexpected end of buffer"));
        return true;
    }

    quint8 readByte()
    {
        if (!ensure(1)) return 0;
        return *d++;
    }

    QByteArray readBytes(qsizetype n)
    {
        if (!ensure(n)) return {};
        QByteArray r(reinterpret_cast<const char *>(d), n);
        d += n;
        return r;
    }

    quint32 readVarUint()
    {
        quint32 value = 0;
        int shift = 0;
        while (true) {
            if (!ensure(1)) return 0;
            const quint8 b = *d++;
            value |= quint32(b & 0x7f) << shift;
            if (!(b & 0x80) || shift >= 35) break;
            shift += 7;
        }
        return value;
    }

    qint32 readVarInt()
    {
        const quint32 v = readVarUint();
        return (v & 1) ? qint32(~(v >> 1)) : qint32(v >> 1);
    }

    quint64 readVarUint64()
    {
        quint64 value = 0;
        int shift = 0;
        quint8 b = 0;
        while (shift < 56) {
            if (!ensure(1)) return 0;
            b = *d++;
            if (!(b & 0x80)) {
                value |= quint64(b) << shift;
                return value;
            }
            value |= quint64(b & 0x7f) << shift;
            shift += 7;
        }
        if (!ensure(1)) return 0;
        b = *d++;
        value |= quint64(b) << shift;
        return value;
    }

    qint64 readVarInt64()
    {
        const quint64 v = readVarUint64();
        return (v & 1) ? qint64(~(v >> 1)) : qint64(v >> 1);
    }

    // Var-length float: matches evanw/kiwi encoding.
    double readVarFloat()
    {
        if (!ensure(1)) return 0;
        if (*d == 0) { ++d; return 0.0; }
        if (!ensure(4)) return 0;
        quint32 bits = quint32(d[0]) | (quint32(d[1]) << 8)
                     | (quint32(d[2]) << 16) | (quint32(d[3]) << 24);
        d += 4;
        // Rotate left by 23 (or right by 9)
        bits = (bits << 23) | (bits >> 9);
        float f;
        std::memcpy(&f, &bits, sizeof(f));
        return double(f);
    }

    QString readString()
    {
        QString result;
        while (true) {
            if (!ensure(1)) return result;
            const quint8 a = *d++;
            quint32 cp;
            if (a < 0xc0) {
                cp = a;
            } else {
                if (!ensure(1)) return result;
                const quint8 b = *d++;
                if (a < 0xe0) {
                    cp = (quint32(a & 0x1f) << 6) | quint32(b & 0x3f);
                } else {
                    if (!ensure(1)) return result;
                    const quint8 c = *d++;
                    if (a < 0xf0) {
                        cp = (quint32(a & 0x0f) << 12) | (quint32(b & 0x3f) << 6) | quint32(c & 0x3f);
                    } else {
                        if (!ensure(1)) return result;
                        const quint8 dd = *d++;
                        cp = (quint32(a & 0x07) << 18) | (quint32(b & 0x3f) << 12)
                           | (quint32(c & 0x3f) << 6) | quint32(dd & 0x3f);
                    }
                }
            }
            if (cp == 0) break;
            if (cp < 0x10000) {
                result.append(QChar(char16_t(cp)));
            } else {
                cp -= 0x10000;
                result.append(QChar(char16_t((cp >> 10) + 0xd800)));
                result.append(QChar(char16_t((cp & 0x3ff) + 0xdc00)));
            }
        }
        return result;
    }

    qsizetype remaining() const { return end - d; }

private:
    const quint8 *d;
    const quint8 *end;
    QString *err;
    bool hasFailed = false;
};

static QCborValue readBuiltin(Reader &r, Builtin b)
{
    switch (b) {
    case Builtin::Bool: return QCborValue(bool(r.readByte() != 0));
    case Builtin::Byte: return QCborValue(qint64(r.readByte()));
    case Builtin::Int:  return QCborValue(qint64(r.readVarInt()));
    case Builtin::Uint: return QCborValue(qint64(r.readVarUint()));
    case Builtin::Float: return QCborValue(r.readVarFloat());
    case Builtin::String: return QCborValue(r.readString());
    case Builtin::Int64: return QCborValue(qint64(r.readVarInt64()));
    case Builtin::Uint64: {
        const quint64 u = r.readVarUint64();
        // QCborValue doesn't have a direct u64 type; represent as signed int if it fits,
        // otherwise stringify to avoid information loss.
        if (u <= quint64(std::numeric_limits<qint64>::max()))
            return QCborValue(qint64(u));
        return QCborValue(QString::number(u));
    }
    }
    return {};
}

static QCborValue readEnumValue(Reader &r, const Definition &def)
{
    const quint32 v = r.readVarUint();
    const auto it = def.enumValues.constFind(v);
    if (it != def.enumValues.cend())
        return QCborValue(*it);
    return QCborValue(qint64(v));
}

static QCborValue readDefinition(Reader &r, const Schema &schema, int defIndex);

static QCborValue readFieldValue(Reader &r, const Schema &schema, const Field &f)
{
    if (f.isArray) {
        // Special case: array of bytes → QByteArray
        if (f.hasBuiltin && f.builtin == Builtin::Byte) {
            const quint32 len = r.readVarUint();
            return QCborValue(r.readBytes(len));
        }
        const quint32 len = r.readVarUint();
        QCborArray arr;
        for (quint32 i = 0; i < len; ++i) {
            Field scalar = f;
            scalar.isArray = false;
            arr.append(readFieldValue(r, schema, scalar));
            if (r.failed()) return arr;
        }
        return QCborValue(arr);
    }
    if (f.hasBuiltin)
        return readBuiltin(r, f.builtin);
    if (f.defIndex < 0 || f.defIndex >= schema.definitions.size()) {
        r.fail(QStringLiteral("invalid def index %1").arg(f.defIndex));
        return {};
    }
    const Definition &dref = schema.definitions[f.defIndex];
    if (dref.kind == Kind::Enum)
        return readEnumValue(r, dref);
    return readDefinition(r, schema, f.defIndex);
}

static QCborValue readDefinition(Reader &r, const Schema &schema, int defIndex)
{
    const Definition &d = schema.definitions[defIndex];
    QCborMap result;
    if (d.kind == Kind::Message) {
        while (!r.failed()) {
            const quint32 tag = r.readVarUint();
            if (tag == 0) break;
            // Find field with matching value
            const Field *match = nullptr;
            for (const Field &f : d.fields) {
                if (f.value == tag) { match = &f; break; }
            }
            if (!match) {
                r.fail(QStringLiteral("unknown field tag %1 in %2").arg(tag).arg(d.name));
                break;
            }
            result.insert(QCborValue(match->name), readFieldValue(r, schema, *match));
        }
    } else { // STRUCT
        for (const Field &f : d.fields) {
            if (r.failed()) break;
            result.insert(QCborValue(f.name), readFieldValue(r, schema, f));
        }
    }
    return QCborValue(result);
}

} // anonymous

bool parseSchema(const QByteArray &raw, Schema *out, QString *error)
{
    Reader r(raw, error);
    const quint32 defCount = r.readVarUint();
    if (r.failed()) return false;

    out->definitions.resize(int(defCount));

    // Temporary stash of per-field raw type ints, resolved in pass 2.
    struct RawType { qint32 type; };
    QVector<QVector<RawType>> rawTypes;
    rawTypes.resize(int(defCount));

    for (quint32 i = 0; i < defCount; ++i) {
        Definition &def = out->definitions[int(i)];
        def.name = r.readString();
        const quint8 kindByte = r.readByte();
        switch (kindByte) {
        case 0: def.kind = Kind::Enum; break;
        case 1: def.kind = Kind::Struct; break;
        case 2: def.kind = Kind::Message; break;
        default:
            r.fail(QStringLiteral("unknown def kind %1 at %2").arg(kindByte).arg(def.name));
            return false;
        }
        const quint32 fcount = r.readVarUint();
        def.fields.resize(int(fcount));
        rawTypes[int(i)].resize(int(fcount));
        for (quint32 j = 0; j < fcount; ++j) {
            Field &fd = def.fields[int(j)];
            fd.name = r.readString();
            rawTypes[int(i)][int(j)].type = r.readVarInt();
            fd.isArray = (r.readByte() & 1) != 0;
            fd.value = r.readVarUint();
            if (def.kind == Kind::Enum)
                def.enumValues.insert(fd.value, fd.name);
        }
        out->indexByName.insert(def.name, int(i));
    }
    if (r.failed()) return false;

    // Pass 2: resolve types
    for (int i = 0; i < out->definitions.size(); ++i) {
        if (out->definitions[i].kind == Kind::Enum)
            continue; // enum field types are not meaningful
        for (int j = 0; j < out->definitions[i].fields.size(); ++j) {
            const qint32 t = rawTypes[i][j].type;
            Field &fd = out->definitions[i].fields[j];
            if (t < 0) {
                const int idx = ~t;
                if (idx < 0 || idx > int(Builtin::Uint64)) {
                    if (error) *error = QStringLiteral("unknown builtin index %1 in %2.%3").arg(idx).arg(out->definitions[i].name, fd.name);
                    return false;
                }
                fd.hasBuiltin = true;
                fd.builtin = Builtin(idx);
            } else {
                if (t >= out->definitions.size()) {
                    if (error) *error = QStringLiteral("def ref out of range: %1").arg(t);
                    return false;
                }
                fd.hasBuiltin = false;
                fd.defIndex = t;
            }
        }
    }

    out->messageEntryIndex = out->indexOf(QStringLiteral("Message"));
    return true;
}

QCborValue decodeMessage(const QByteArray &raw, const Schema &schema, QString *error)
{
    if (schema.messageEntryIndex < 0) {
        if (error) *error = QStringLiteral("schema has no root \"Message\" type");
        return {};
    }
    Reader r(raw, error);
    return readDefinition(r, schema, schema.messageEntryIndex);
}


} // namespace FigKiwi
