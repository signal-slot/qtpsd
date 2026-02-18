// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDSECTION_H
#define QPSDSECTION_H

#include <QtPsdCore/qpsdcoreglobal.h>

#include <QtCore/QIODevice>
#include <QtCore/QRect>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QtEndian>
#include <QtCore/QVariant>

QT_BEGIN_NAMESPACE

class QPsdColorSpace;

class Q_PSDCORE_EXPORT QPsdSection
{
public:
    QPsdSection();
    QPsdSection(const QPsdSection &other);
    QPsdSection &operator=(const QPsdSection &other);
    void swap(QPsdSection &other) noexcept { d.swap(other.d); }
    virtual ~QPsdSection();

    bool hasError() const { return !errorString().isEmpty(); }
    QString errorString() const;
protected:
    void setErrorString(const QString &errorString);

    static void skip(QIODevice *source, quint32 size, quint32 *length = nullptr) {
        if (length)
            *length -= size;
        source->skip(size);
    }

    template<typename T>
    static T read(QIODevice *source, quint32 *length = nullptr) {
        QByteArray data = source->read(sizeof(T));
        if (length)
            *length -= sizeof(T);
        return qFromBigEndian<T>(data.constData());
    }

    static qint8 readS8(QIODevice *source, quint32 *length = nullptr) {
        return read<qint8>(source, length);
    }
    static qint16 readS16(QIODevice *source, quint32 *length = nullptr) {
        return read<qint16>(source, length);
    }
    static qint32 readS32(QIODevice *source, quint32 *length = nullptr) {
        return read<qint32>(source, length);
    }

    static quint8 readU8(QIODevice *source, quint32 *length = nullptr) {
        return read<quint8>(source, length);
    }
    static quint16 readU16(QIODevice *source, quint32 *length = nullptr) {
        return read<quint16>(source, length);
    }
    static quint32 readU32(QIODevice *source, quint32 *length = nullptr) {
        return read<quint32>(source, length);
    }
    static quint64 readU64(QIODevice *source, quint32 *length = nullptr) {
        return read<quint64>(source, length);
    }

    static float readFloat(QIODevice *source, quint32 *length = nullptr) {
        return read<float>(source, length);
    }

    static double readDouble(QIODevice *source, quint32 *length = nullptr) {
        return read<double>(source, length);
    }

    template<typename T>
    static T readLE(QIODevice *source, quint32 *length = nullptr) {
        QByteArray data = source->read(sizeof(T));
        if (length)
            *length -= sizeof(T);
        return qFromLittleEndian<T>(data.constData());
    }

    static quint32 readU32LE(QIODevice *source, quint32 *length = nullptr) {
        return readLE<quint32>(source, length);
    }

    static QByteArray readPascalString(QIODevice *source, int padding = 1, quint32 *length = nullptr);
    static QByteArray readByteArray(QIODevice *source, quint32 size, quint32 *length = nullptr);
    static QString readString(QIODevice *source, quint32 *length = nullptr);
    static QString readStringLE(QIODevice *source, quint32 *length = nullptr);

    static QRect readRectangle(QIODevice *source, quint32 *length = nullptr);
    static QString readColor(QIODevice *source, quint32 *length = nullptr);
    static QPsdColorSpace readColorSpace(QIODevice *source, quint32 *length = nullptr);

    static double readPathNumber(QIODevice *source, quint32 *length = nullptr);

public:
    template<typename T>
    static void write(QIODevice *dest, T value) {
        T be = qToBigEndian(value);
        dest->write(reinterpret_cast<const char *>(&be), sizeof(T));
    }

    static void writeU8(QIODevice *dest, quint8 value) {
        dest->write(reinterpret_cast<const char *>(&value), 1);
    }
    static void writeU16(QIODevice *dest, quint16 value) {
        write<quint16>(dest, value);
    }
    static void writeU32(QIODevice *dest, quint32 value) {
        write<quint32>(dest, value);
    }
    static void writeS16(QIODevice *dest, qint16 value) {
        write<qint16>(dest, value);
    }
    static void writeS32(QIODevice *dest, qint32 value) {
        write<qint32>(dest, value);
    }

    static void writeByteArray(QIODevice *dest, const QByteArray &data) {
        dest->write(data);
    }

    static void writePascalString(QIODevice *dest, const QByteArray &str, int padding = 1) {
        writeU8(dest, static_cast<quint8>(str.size()));
        if (!str.isEmpty())
            dest->write(str);
        // Pad so that total (1 + str.size()) is a multiple of padding
        int total = 1 + str.size();
        if (padding > 1 && total % padding != 0) {
            int padBytes = padding - (total % padding);
            dest->write(QByteArray(padBytes, '\0'));
        }
    }

    static void writeRectangle(QIODevice *dest, const QRect &rect) {
        writeS32(dest, rect.top());
        writeS32(dest, rect.left());
        writeS32(dest, rect.bottom() + 1);
        writeS32(dest, rect.right() + 1);
    }

    static void writeU64(QIODevice *dest, quint64 value) {
        write<quint64>(dest, value);
    }

    static void writeDouble(QIODevice *dest, double value) {
        write<double>(dest, value);
    }

    static void writeFloat(QIODevice *dest, float value) {
        write<float>(dest, value);
    }

    static void writeString(QIODevice *dest, const QString &str);
    static void writeColorSpace(QIODevice *dest, const QPsdColorSpace &cs);
    static void writePathNumber(QIODevice *dest, double value);

    static void writePadding(QIODevice *dest, int alignment) {
        if (alignment > 1 && dest->pos() % alignment != 0) {
            int padBytes = alignment - (dest->pos() % alignment);
            dest->write(QByteArray(padBytes, '\0'));
        }
    }

protected:
    static int even(int size)
    {
        return size % 2 == 0 ? size : size + 1;
    }
    static void padding(QIODevice *source, int count = 2) {
        if (source->pos() % count > 0)
            source->read(count - source->pos() % count);
    }
protected:
    class EnsureSeek {
    public:
        EnsureSeek(QIODevice *io, qint64 length, int padding = 0)
            : _io(io)
        {
            if (padding == 0 || length == 0 || length % padding == 0) {
                _paddingSize = 0;
            } else {
                _paddingSize = padding - length % padding;
            }

            _pos = io->pos() + length + _paddingSize;
        }

        ~EnsureSeek() {
            _io->seek(_pos);
        }

        qint64 bytesAvailable() const {
            return _pos - _io->pos();
        }

        qint32 paddingSize() const {
            return _paddingSize;
        }

    private:
        QIODevice *_io;
        qint64 _pos;
        qint32 _paddingSize;
    };
private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDSECTION_H
