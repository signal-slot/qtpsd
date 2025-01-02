// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdimagestore.h"
#include "qpsdexporterplugin.h"

#include <QtCore/QCryptographicHash>

QT_BEGIN_NAMESPACE

class QPsdImageStore::Private : public QSharedData {
public:
    Private(const QDir &d, const QString &p) : dir(d), path(p) {
        dir.mkpath(path);
    }

    const QDir dir;
    const QString path;

    QHash<QString, QString> hash;

    QDir imageDir();
    QString sha256hex(const QByteArray &bytes);
    std::pair<QByteArray, QString> sha256image(const QImage &image, const char *format);
    QString sha256file(const QDir &dir, const QString &filename);
};

QPsdImageStore::QPsdImageStore(const QDir &dir, const QString &path) 
    : d(new Private(dir, path))
{
}

QPsdImageStore::QPsdImageStore(const QPsdImageStore &other)
    : d(other.d)
{
}

QPsdImageStore::~QPsdImageStore() = default;

QPsdImageStore &QPsdImageStore::operator=(const QPsdImageStore &other)
{
    if (this != &other) {
        d = other.d;
    }

    return *this;
}

QString QPsdImageStore::save(const QString &filename, const QImage &image, const char *format)
{
    QString fname = filename;
    QFileInfo fileInfo(filename);
    int i = 0;
    while (true) {
        // first, check if hash is registered
        if (d->hash.contains(fname)) {
            // if already registered, check image hash
            QString sha256registered = d->hash.value(fname);
            std::pair<QByteArray, QString> sha256img = d->sha256image(image, format);

            if (sha256registered == sha256img.second) {
                // if hash matched
                break;
            }
            // if hash not matched, increment filename at below
        } else {
            // if not registered in hash, check file is exists
            QString sha256 = d->sha256file(d->imageDir(), fname);
            if (!sha256.isEmpty()) {
                // file found
                d->hash.insert(fname, sha256);

                // re-check hash
                continue;
            } else {
                // file not found
                std::pair<QByteArray, QString> sha256img = d->sha256image(image, format);

                // create file
                QFile f(d->imageDir().absoluteFilePath(fname));
                if (f.open(QIODevice::WriteOnly)) {
                    f.write(sha256img.first);
                    f.close();
                    d->hash.insert(fname, sha256img.second);
                    break;
                }
            }
        }

        // increment filename and retry    
        fname = u"%1_%2.%3"_s.arg(fileInfo.completeBaseName()).arg(++i).arg(fileInfo.suffix());
    }

    return fname;
}

QDir QPsdImageStore::Private::imageDir()
{
    return QDir(dir.absoluteFilePath(path));
}

QString QPsdImageStore::Private::sha256hex(const QByteArray &bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

std::pair<QByteArray, QString> QPsdImageStore::Private::sha256image(const QImage &image, const char *format)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, format);

    return std::make_pair(bytes, sha256hex(bytes));
}

QString QPsdImageStore::Private::sha256file(const QDir &dir, const QString &filename)
{
    QCryptographicHash sha256(QCryptographicHash::Sha256);

    QFile file(dir.absoluteFilePath(filename));
    if (file.open(QIODevice::ReadOnly)) {
        sha256.addData(&file);
        file.close();

        return QString::fromLatin1(sha256.result().toHex());
    } else {
        return {};
    }
}

QT_END_NAMESPACE
