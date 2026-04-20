// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "fig_archive.h"

#include <QtCore/QBuffer>
#include <QtCore/QFile>
#include <QtCore/private/qzipreader_p.h>

#ifdef QPSD_FIGMA_LOCAL_FIG
#  include <zlib.h>
#  include <zstd.h>
#endif

using namespace Qt::StringLiterals;

namespace FigArchive {

static constexpr char FIG_KIWI_PRELUDE[] = "fig-kiwi";
static constexpr int PRELUDE_LEN = 8;

#ifdef QPSD_FIGMA_LOCAL_FIG
QByteArray decompressDeflateRaw(const QByteArray &in, QString *error)
{
    z_stream strm {};
    if (inflateInit2(&strm, -15) != Z_OK) {
        if (error) *error = QStringLiteral("inflateInit2 failed");
        return {};
    }
    strm.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    strm.avail_in = static_cast<uInt>(in.size());

    QByteArray out;
    constexpr int BUF = 64 * 1024;
    QByteArray buf(BUF, Qt::Uninitialized);
    int ret;
    do {
        strm.next_out = reinterpret_cast<Bytef *>(buf.data());
        strm.avail_out = BUF;
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&strm);
            if (error) *error = QStringLiteral("inflate failed: %1").arg(ret);
            return {};
        }
        out.append(buf.constData(), BUF - strm.avail_out);
    } while (ret != Z_STREAM_END);
    inflateEnd(&strm);
    return out;
}

QByteArray decompressZstd(const QByteArray &in, QString *error)
{
    const auto expected = ZSTD_getFrameContentSize(in.constData(), in.size());
    if (expected == ZSTD_CONTENTSIZE_ERROR) {
        if (error) *error = QStringLiteral("not a valid zstd frame");
        return {};
    }

    // If size unknown, use streaming; otherwise allocate exact.
    if (expected == ZSTD_CONTENTSIZE_UNKNOWN) {
        ZSTD_DCtx *dctx = ZSTD_createDCtx();
        QByteArray out;
        QByteArray buf(ZSTD_DStreamOutSize(), Qt::Uninitialized);
        ZSTD_inBuffer ib { in.constData(), static_cast<size_t>(in.size()), 0 };
        while (ib.pos < ib.size) {
            ZSTD_outBuffer ob { buf.data(), static_cast<size_t>(buf.size()), 0 };
            const size_t r = ZSTD_decompressStream(dctx, &ob, &ib);
            if (ZSTD_isError(r)) {
                if (error) *error = QStringLiteral("zstd stream: %1").arg(QString::fromUtf8(ZSTD_getErrorName(r)));
                ZSTD_freeDCtx(dctx);
                return {};
            }
            out.append(buf.constData(), qsizetype(ob.pos));
            if (r == 0) break;
        }
        ZSTD_freeDCtx(dctx);
        return out;
    }

    QByteArray out(qsizetype(expected), Qt::Uninitialized);
    const size_t r = ZSTD_decompress(out.data(), out.size(), in.constData(), in.size());
    if (ZSTD_isError(r)) {
        if (error) *error = QStringLiteral("zstd: %1").arg(QString::fromUtf8(ZSTD_getErrorName(r)));
        return {};
    }
    out.resize(qsizetype(r));
    return out;
}

QByteArray decompressAuto(const QByteArray &in, QString *error)
{
    // ZSTD magic: 28 B5 2F FD
    if (in.size() >= 4
        && quint8(in[0]) == 0x28 && quint8(in[1]) == 0xb5
        && quint8(in[2]) == 0x2f && quint8(in[3]) == 0xfd) {
        return decompressZstd(in, error);
    }
    return decompressDeflateRaw(in, error);
}
#else // !QPSD_FIGMA_LOCAL_FIG — stubs so callers link, but refuse to run

QByteArray decompressDeflateRaw(const QByteArray &, QString *error)
{
    if (error) *error = QStringLiteral("local .fig support was disabled at build time (missing zlib/libzstd)");
    return {};
}

QByteArray decompressZstd(const QByteArray &, QString *error)
{
    if (error) *error = QStringLiteral("local .fig support was disabled at build time (missing libzstd)");
    return {};
}

QByteArray decompressAuto(const QByteArray &, QString *error)
{
    if (error) *error = QStringLiteral("local .fig support was disabled at build time");
    return {};
}
#endif // QPSD_FIGMA_LOCAL_FIG

static bool parseFigStream(const QByteArray &figData, Archive *out, QString *error)
{
    if (figData.size() < PRELUDE_LEN + 4) {
        if (error) *error = QStringLiteral("file too small");
        return false;
    }

    const QByteArray magic = figData.left(PRELUDE_LEN);
    if (magic != QByteArray::fromRawData(FIG_KIWI_PRELUDE, PRELUDE_LEN)
        && magic != "fig-jam." && magic != "fig-deck") {
        if (error) *error = QStringLiteral("unsupported prelude: %1").arg(QString::fromLatin1(magic));
        return false;
    }
    out->magic = QString::fromLatin1(magic);

    const uchar *p = reinterpret_cast<const uchar *>(figData.constData());
    auto readU32 = [&](qsizetype off) {
        return quint32(p[off]) | (quint32(p[off+1]) << 8)
             | (quint32(p[off+2]) << 16) | (quint32(p[off+3]) << 24);
    };

    out->version = readU32(PRELUDE_LEN);

    qsizetype off = PRELUDE_LEN + 4;
    int idx = 0;
    while (off + 4 <= figData.size()) {
        const quint32 sz = readU32(off);
        off += 4;
        if (off + qsizetype(sz) > figData.size()) {
            if (error) *error = QStringLiteral("chunk %1 truncated").arg(idx);
            return false;
        }
        QByteArray chunk = figData.mid(off, sz);
        off += sz;
        if (idx == 0) out->schemaChunk = chunk;
        else if (idx == 1) out->dataChunk = chunk;
        else if (idx == 2) out->preview = chunk;
        ++idx;
    }
    if (out->schemaChunk.isEmpty() || out->dataChunk.isEmpty()) {
        if (error) *error = QStringLiteral("missing schema or data chunk");
        return false;
    }
    return true;
}

bool readFigData(const QByteArray &data, Archive *out, QString *error)
{
    // If the blob starts with ZIP magic, unzip first and find the .fig entry inside.
    if (data.size() >= 4 && quint8(data[0]) == 0x50 && quint8(data[1]) == 0x4b
        && quint8(data[2]) == 0x03 && quint8(data[3]) == 0x04) {
        QBuffer buffer;
        buffer.setData(data);
        if (!buffer.open(QIODevice::ReadOnly)) {
            if (error) *error = QStringLiteral("QBuffer open failed");
            return false;
        }
        QZipReader zip(&buffer);
        if (!zip.isReadable() || !zip.exists()) {
            if (error) *error = QStringLiteral("ZIP read failed");
            return false;
        }
        const auto entries = zip.fileInfoList();
        QByteArray figBlob;
        for (const auto &e : entries) {
            if (!e.isFile) continue;
            const QByteArray contents = zip.fileData(e.filePath);
            out->zipFiles.insert(e.filePath, contents);
            if (e.filePath == "meta.json"_L1) {
                out->metaJson = QString::fromUtf8(contents);
            }
            if (figBlob.isEmpty() && contents.size() > PRELUDE_LEN) {
                const QByteArray m = contents.left(PRELUDE_LEN);
                if (m == QByteArray::fromRawData(FIG_KIWI_PRELUDE, PRELUDE_LEN)
                    || m == "fig-jam." || m == "fig-deck") {
                    figBlob = contents;
                }
            }
        }
        if (figBlob.isEmpty()) {
            if (error) *error = QStringLiteral("no fig-kiwi entry found in ZIP");
            return false;
        }
        return parseFigStream(figBlob, out, error);
    }
    return parseFigStream(data, out, error);
}

bool readFigFile(const QString &path, Archive *out, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("cannot open %1: %2").arg(path, f.errorString());
        return false;
    }
    const QByteArray data = f.readAll();
    return readFigData(data, out, error);
}

} // namespace FigArchive
