// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsddescriptor.h"
#include "qpsddescriptorplugin.h"
#include "qpsdenum.h"
#include "qpsdunitfloat.h"

#include <QtCore/QBuffer>
#include <QtCore/QStringEncoder>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdDescriptor, "qt.psdcore.descriptor")

class QPsdDescriptor::Private : public QSharedData
{
public:
    QString name;
    QByteArray classID;
    QHash<QByteArray, QVariant> data;
    QList<QByteArray> keyOrder;
    void parse(QIODevice *source, quint32 *length);
};

void QPsdDescriptor::Private::parse(QIODevice *source, quint32 *length)
{
    // Descriptor structure
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577411_21585

    // Unicode string: name from classID
    name = readString(source, length);

    // classID: 4 bytes (length), followed either by string or (if length is zero) 4-byte classID
    auto size = readS32(source, length);
    classID = readByteArray(source, size == 0 ? 4 : size, length);

    auto count = readS32(source, length);

    qCDebug(lcQPsdDescriptor) << name << classID << count;
    while (count-- > 0) {
        qCDebug(lcQPsdDescriptor) << count;
        auto size = readS32(source, length);
        QByteArray key = readByteArray(source, size == 0 ? 4 : size, length);
        auto osType = readByteArray(source, 4, length);
        // load plugin for osType
        auto plugin = QPsdDescriptorPlugin::plugin(osType);
        if (plugin) {
            auto value = plugin->parse(source, length);
            data.insert(key, value);
            keyOrder.append(key);
            if (value.typeId() == QMetaType::QByteArray) {
                value = value.toByteArray().left(20);
            }
            qCDebug(lcQPsdDescriptor) << key << osType << value;
        } else {
            qCWarning(lcQPsdDescriptor) << osType << "not supported for" << key.left(4);
            break;
        }
    }
}

QPsdDescriptor::QPsdDescriptor()
    : QPsdSection()
    , d(new Private)
{}

QPsdDescriptor::QPsdDescriptor(const QByteArray &data, int version)
    : QPsdDescriptor()
{
    QByteArray dataCopy = data;
    QBuffer buffer(&dataCopy);
    buffer.open(QIODevice::ReadOnly);
    quint32 length = data.size();
    if (version >= 0) {
        // check first 16 bytes for version
        quint32 v = readU32(&buffer, &length);
        if (v != static_cast<quint32>(version)) {
            qCWarning(lcQPsdDescriptor) << "Version mismatch: expected" << version << "but got" << v;
            return;
        }
    }
    d->parse(&buffer, &length);
}

QPsdDescriptor::QPsdDescriptor(QIODevice *source, quint32 *length)
    : QPsdDescriptor()
{
    d->parse(source, length);
}

QPsdDescriptor::QPsdDescriptor(const QPsdDescriptor &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdDescriptor &QPsdDescriptor::operator=(const QPsdDescriptor &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdDescriptor::~QPsdDescriptor() = default;

QString QPsdDescriptor::name() const
{
    return d->name;
}

void QPsdDescriptor::setName(const QString &name)
{
    d->name = name;
}

QByteArray QPsdDescriptor::classID() const
{
    return d->classID;
}

void QPsdDescriptor::setClassID(const QByteArray &classID)
{
    d->classID = classID;
}

QHash<QByteArray, QVariant> QPsdDescriptor::data() const
{
    return d->data;
}

void QPsdDescriptor::setData(const QHash<QByteArray, QVariant> &data)
{
    d->data = data;
}

QList<QByteArray> QPsdDescriptor::keyOrder() const
{
    return d->keyOrder;
}

void QPsdDescriptor::setKeyOrder(const QList<QByteArray> &keyOrder)
{
    d->keyOrder = keyOrder;
}

static QByteArray inferOsType(const QVariant &value)
{
    if (value.typeId() == QMetaType::Bool)
        return "bool"_ba;
    if (value.typeId() == QMetaType::Double)
        return "doub"_ba;
    if (value.typeId() == QMetaType::Int)
        return "long"_ba;
    if (value.typeId() == QMetaType::QString)
        return "TEXT"_ba;
    if (value.typeId() == QMetaType::QByteArray)
        return "tdta"_ba;
    if (value.canConvert<QPsdEnum>())
        return "enum"_ba;
    if (value.canConvert<QPsdUnitFloat>())
        return "UntF"_ba;
    if (value.canConvert<QPsdDescriptor>())
        return "Objc"_ba;
    if (value.typeId() == QMetaType::QVariantList)
        return "VlLs"_ba;
    if (value.typeId() == QMetaType::QVariantMap) {
        // Distinguish ObAr vs Pth by content: Pth values are QStrings, ObAr values are QVariantLists
        const auto map = value.toMap();
        if (!map.isEmpty()) {
            const auto &first = map.constBegin().value();
            if (first.typeId() == QMetaType::QString)
                return "Pth "_ba;
        }
        return "ObAr"_ba;
    }
    return {};
}

static void writeDescriptorValue(QIODevice *dest, const QByteArray &osType, const QVariant &value);

static void writeDescriptorValue(QIODevice *dest, const QByteArray &osType, const QVariant &value)
{
    if (osType == "bool") {
        QPsdSection::writeU8(dest, value.toBool() ? 1 : 0);
    } else if (osType == "doub") {
        QPsdSection::writeDouble(dest, value.toDouble());
    } else if (osType == "long") {
        QPsdSection::writeS32(dest, value.toInt());
    } else if (osType == "TEXT") {
        QPsdSection::writeString(dest, value.toString());
    } else if (osType == "tdta") {
        const auto data = value.toByteArray();
        QPsdSection::writeS32(dest, data.size());
        dest->write(data);
    } else if (osType == "enum") {
        value.value<QPsdEnum>().write(dest);
    } else if (osType == "UntF") {
        value.value<QPsdUnitFloat>().write(dest);
    } else if (osType == "Objc" || osType == "GlbO") {
        value.value<QPsdDescriptor>().write(dest);
    } else if (osType == "VlLs") {
        const auto list = value.toList();
        QPsdSection::writeS32(dest, list.size());
        for (const auto &item : list) {
            const auto itemOsType = inferOsType(item);
            dest->write(itemOsType.leftJustified(4, '\0', true));
            writeDescriptorValue(dest, itemOsType, item);
        }
    } else if (osType == "ObAr") {
        const auto map = value.toMap();
        // ObAr format: version(U32=16) + classID header + count + per-item data
        QPsdSection::writeU32(dest, 16);
        // Name (empty string)
        QPsdSection::writeS32(dest, 0);
        // ClassID (empty = 4 bytes null)
        QPsdSection::writeS32(dest, 0);
        dest->write("null", 4);
        // Item count
        QPsdSection::writeS32(dest, map.size());
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            // Key
            const auto key = it.key().toUtf8();
            QPsdSection::writeS32(dest, key.size() == 4 ? 0 : key.size());
            dest->write(key);
            // Signature
            dest->write("8BIM", 4);
            // Each value is a QVariantList of QPsdUnitFloat
            const auto values = it.value().toList();
            if (!values.isEmpty()) {
                const auto firstUf = values.first().value<QPsdUnitFloat>();
                dest->write(QPsdUnitFloat::unitToTag(firstUf.unit()));
                QPsdSection::writeS32(dest, values.size());
                for (const auto &v : values) {
                    QPsdSection::writeDouble(dest, v.value<QPsdUnitFloat>().value());
                }
            } else {
                dest->write("#Nne", 4);
                QPsdSection::writeS32(dest, 0);
            }
        }
    } else if (osType == "Pth ") {
        const auto map = value.toMap();
        // Pth format: U32 totalLen + signature(4) + U32LE pathSize + LE Unicode string
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            const auto signature = it.key().toUtf8().left(4);
            const auto path = it.value().toString();

            // Encode the path as UTF-16LE with null terminator
            QStringEncoder encoder(QStringEncoder::Utf16LE);
            QByteArray encoded = encoder.encode(path);
            // Add null terminator
            encoded.append('\0');
            encoded.append('\0');
            quint32 charCount = encoded.size() / 2;

            // Total length: signature(4) + pathSize(U32LE=4) + charCount(U32LE=4) + encoded data
            quint32 totalLen = 4 + 4 + 4 + encoded.size();
            QPsdSection::writeU32(dest, totalLen);
            dest->write(signature.leftJustified(4, '\0', true));
            // pathSize in LE
            quint32 pathSize = 4 + encoded.size(); // charCount(4) + data
            quint32 pathSizeLE = qToLittleEndian(pathSize);
            dest->write(reinterpret_cast<const char *>(&pathSizeLE), 4);
            // charCount in LE
            quint32 charCountLE = qToLittleEndian(charCount);
            dest->write(reinterpret_cast<const char *>(&charCountLE), 4);
            dest->write(encoded);
            break; // Only one entry
        }
    }
}

void QPsdDescriptor::write(QIODevice *dest) const
{
    // Unicode string: name
    writeString(dest, d->name);

    // ClassID: S32(size) + bytes. Size 0 means exactly 4 bytes.
    writeS32(dest, d->classID.size() == 4 ? 0 : d->classID.size());
    writeByteArray(dest, d->classID);

    // Item count
    writeS32(dest, d->data.size());

    // Items — use keyOrder for deterministic output matching original parse order
    const auto &keys = d->keyOrder.isEmpty()
        ? d->data.keys()
        : d->keyOrder;
    for (const auto &key : keys) {
        if (!d->data.contains(key))
            continue;
        const auto &value = d->data.value(key);
        // Key: S32(size) + bytes. Size 0 means exactly 4 bytes.
        writeS32(dest, key.size() == 4 ? 0 : key.size());
        writeByteArray(dest, key);

        // osType
        const auto osType = inferOsType(value);
        dest->write(osType.leftJustified(4, '\0', true));

        // Value
        writeDescriptorValue(dest, osType, value);
    }
}

QDebug operator<<(QDebug s, const QPsdDescriptor &value)
{
    QDebugStateSaver saver(s);
    s.nospace() << "QPsdDescriptor(" << value.name() << ", " << value.classID() << ", " << value.data() << ")";
    return s;
}

QT_END_NAMESPACE
