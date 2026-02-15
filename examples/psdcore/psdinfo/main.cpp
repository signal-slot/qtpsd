// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QMetaEnum>
#include <QtCore/QLoggingCategory>
#include <QtCore/QDirIterator>

#include <QtPsdCore/QPsdParser>

#include <map>
#include <set>

using namespace Qt::StringLiterals;

QString colorModeString(QPsdFileHeader::ColorMode colorMode) {
    switch (colorMode) {
    case QPsdFileHeader::ColorMode::Bitmap:
        return "Bitmap";
    case QPsdFileHeader::ColorMode::Grayscale:
        return "Grayscale";
    case QPsdFileHeader::ColorMode::Indexed:
        return "Indexed";
    case QPsdFileHeader::ColorMode::RGB:
        return "RGB";
    case QPsdFileHeader::ColorMode::CMYK:
        return "CMYK";
    case QPsdFileHeader::ColorMode::Multichannel:
        return "Multichannel";
    case QPsdFileHeader::ColorMode::Duotone:
        return "Duotone";
    case QPsdFileHeader::ColorMode::Lab:
        return "Lab";
    }

    return "(unknown)";
}

// Global state for warning stats collection
struct WarningStats {
    // warning message -> set of files
    std::map<QString, std::set<QString>> warnings;
    // category -> key -> set of files (for structured "not supported" warnings)
    std::map<QByteArray, std::set<QString>> unsupportedDescriptors;
    std::map<QByteArray, std::set<QString>> unsupportedALI;
    std::map<QByteArray, std::set<QString>> unsupportedEffects;
    QString currentFile;
};

static WarningStats *s_stats = nullptr;

static void statsMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (!s_stats || type != QtWarningMsg)
        return;

    const auto category = QLatin1StringView(context.category ? context.category : "default");
    const auto message = msg.trimmed();

    if (category == "qt.psdcore.descriptor"_L1) {
        // Pattern: "<osType> not supported for <key>"
        const auto idx = message.indexOf(" not supported for "_L1);
        if (idx > 0) {
            const auto osType = message.left(idx).trimmed().toLatin1();
            s_stats->unsupportedDescriptors[osType].insert(s_stats->currentFile);
        }
    } else if (category == "qt.psdcore.effectslayer"_L1) {
        // Pattern: "<osType> not supported"
        const auto idx = message.indexOf(" not supported"_L1);
        if (idx > 0) {
            const auto osType = message.left(idx).trimmed().toLatin1();
            s_stats->unsupportedEffects[osType].insert(s_stats->currentFile);
        }
    }

    if (category == "default"_L1) {
        // Check for ALI "not supported" pattern: "<pos> <key> <length> not supported <data>"
        const auto idx = message.indexOf(" not supported"_L1);
        if (idx > 0) {
            const auto parts = message.left(idx).split(u' ');
            if (parts.size() >= 3) {
                bool ok = false;
                parts[2].toInt(&ok);
                if (ok) {
                    s_stats->unsupportedALI[parts[1].toLatin1()].insert(s_stats->currentFile);
                    return;
                }
            }
        }
    }

    // Collect all warnings generically
    // Normalize: strip hex addresses and numbers to group similar warnings
    QString normalized = message;
    // Replace hex addresses like 0x1234 with <addr>
    static const QRegularExpression hexRe(u"0x[0-9a-fA-F]+"_s);
    normalized.replace(hexRe, u"<addr>"_s);
    // Replace standalone numbers with <N>
    static const QRegularExpression numRe(u"(?<=\\s)\\d+(?=\\s|$)"_s);
    normalized.replace(numRe, u"<N>"_s);

    auto key = u"[%1] %2"_s.arg(category, normalized);
    s_stats->warnings[key].insert(s_stats->currentFile);
}

static void showInfo(const QStringList &files)
{
    QTextStream out(stdout);
    out.setEncoding(QStringConverter::Encoding::System);

    for (const auto &filename : files) {
        QPsdParser parser;
        parser.load(filename);
        const auto fileHeader = parser.fileHeader();

        out << u"filename:%1"_s.arg(filename) << Qt::endl;
        out << u"width:%1"_s.arg(fileHeader.width()) << Qt::endl;
        out << u"height:%1"_s.arg(fileHeader.height()) << Qt::endl;
        out << u"color mode:%1"_s.arg(colorModeString(fileHeader.colorMode())) << Qt::endl;
        out << u"color depth:%1"_s.arg(fileHeader.depth()) << Qt::endl;
        out << u"channels:%1"_s.arg(fileHeader.channels()) << Qt::endl;
        out << Qt::endl;
    }
}

static void printFrequencyTable(QTextStream &out, const QString &title,
                                const std::map<QByteArray, std::set<QString>> &data)
{
    if (data.empty())
        return;

    // Sort by count descending
    QList<std::pair<QByteArray, int>> sorted;
    for (const auto &[key, fileSet] : data)
        sorted.append({key, static_cast<int>(fileSet.size())});
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
        return a.second > b.second;
    });

    out << title << Qt::endl;
    out << QString(title.size(), u'-') << Qt::endl;
    out << u"| Type     | Count | Example files"_s << Qt::endl;
    out << u"|----------|-------|---------------"_s << Qt::endl;
    for (const auto &[key, count] : sorted) {
        QStringList examples;
        int shown = 0;
        for (const auto &f : data.at(key)) {
            examples << f;
            if (++shown >= 3) break;
        }
        auto extra = (data.at(key).size() > 3)
            ? u" (+%1 more)"_s.arg(data.at(key).size() - 3)
            : QString();
        out << u"| "_s
            << QString::fromLatin1(key).leftJustified(8)
            << u" | "_s
            << QString::number(count).rightJustified(5)
            << u" | "_s
            << examples.join(u", "_s)
            << extra << Qt::endl;
    }
    out << Qt::endl;
}

static void showDescriptorStats(const QStringList &files)
{
    WarningStats stats;
    s_stats = &stats;
    qInstallMessageHandler(statsMessageHandler);

    QTextStream out(stdout);
    out.setEncoding(QStringConverter::Encoding::System);

    int fileCount = 0;
    for (const auto &filename : files) {
        stats.currentFile = QFileInfo(filename).fileName();
        QPsdParser parser;
        parser.load(filename);
        fileCount++;
        out << u"\rparsed %1/%2 files"_s.arg(fileCount).arg(files.size());
        out.flush();
    }
    out << Qt::endl << Qt::endl;

    // Restore default handler
    qInstallMessageHandler(nullptr);
    s_stats = nullptr;

    out << u"=== Parsing Statistics (%1 files) ==="_s.arg(fileCount) << Qt::endl << Qt::endl;

    printFrequencyTable(out, u"Unsupported Descriptor Types (osType)"_s, stats.unsupportedDescriptors);
    printFrequencyTable(out, u"Unsupported Additional Layer Information Keys"_s, stats.unsupportedALI);
    printFrequencyTable(out, u"Unsupported Effects Layer Types"_s, stats.unsupportedEffects);

    if (stats.unsupportedDescriptors.empty()
        && stats.unsupportedALI.empty()
        && stats.unsupportedEffects.empty()) {
        out << u"All descriptor types, ALI keys, and effects layer types are supported."_s << Qt::endl << Qt::endl;
    }

    // Print all warnings grouped by message
    if (!stats.warnings.empty()) {
        // Sort by count descending
        QList<std::pair<QString, int>> sorted;
        for (const auto &[msg, fileSet] : stats.warnings)
            sorted.append({msg, static_cast<int>(fileSet.size())});
        std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
            return a.second > b.second;
        });

        out << u"All Warnings (grouped by message)"_s << Qt::endl;
        out << u"---------------------------------"_s << Qt::endl;
        out << u"| Count | Warning"_s << Qt::endl;
        out << u"|-------|--------"_s << Qt::endl;
        for (const auto &[msg, count] : sorted) {
            QStringList examples;
            int shown = 0;
            for (const auto &f : stats.warnings.at(msg)) {
                examples << f;
                if (++shown >= 3) break;
            }
            auto extra = (stats.warnings.at(msg).size() > 3)
                ? u" (+%1 more)"_s.arg(stats.warnings.at(msg).size() - 3)
                : QString();
            out << u"| "_s
                << QString::number(count).rightJustified(5)
                << u" | "_s
                << msg << Qt::endl;
            out << u"|       |   files: "_s
                << examples.join(u", "_s)
                << extra << Qt::endl;
        }
        out << Qt::endl;
    }
}

int main(int argc, char *argv[])
{
    qSetMessagePattern("");

    QCoreApplication::setOrganizationName("Signal Slot Inc.");
    QCoreApplication::setOrganizationDomain("signal-slot.co.jp");
    QCoreApplication::setApplicationName("PsdInfo");
    QCoreApplication::setApplicationVersion("0.2.0");

    QCoreApplication app(argc, argv);

    QCommandLineParser cmdlineParser;
    cmdlineParser.setApplicationDescription("show PSD information");
    cmdlineParser.addVersionOption();
    cmdlineParser.addHelpOption();

    QCommandLineOption descriptorStatsOption(
        u"descriptor-stats"_s,
        u"Parse files and report unsupported descriptor/plugin statistics"_s);
    cmdlineParser.addOption(descriptorStatsOption);

    QCommandLineOption recursiveOption(
        u"recursive"_s,
        u"Recursively scan directories for .psd files"_s);
    cmdlineParser.addOption(recursiveOption);

    cmdlineParser.addPositionalArgument("files", "PSD file(s) or directory", "[files...]");

    cmdlineParser.process(app);

    const QStringList positionalArguments = cmdlineParser.positionalArguments();
    if (positionalArguments.isEmpty()) {
        cmdlineParser.showHelp();
        Q_UNREACHABLE_RETURN(1);
    }

    // Resolve file list (expand directories)
    QStringList files;
    const bool recursive = cmdlineParser.isSet(recursiveOption);
    for (const auto &arg : positionalArguments) {
        QFileInfo fi(arg);
        if (fi.isDir()) {
            QDirIterator it(arg, {u"*.psd"_s, u"*.psb"_s},
                            QDir::Files,
                            recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
            while (it.hasNext())
                files << it.next();
        } else if (fi.isFile()) {
            files << arg;
        } else {
            QTextStream(stderr) << u"Warning: %1 not found, skipping"_s.arg(arg) << Qt::endl;
        }
    }

    if (files.isEmpty()) {
        QTextStream(stderr) << u"No PSD files found"_s << Qt::endl;
        return 1;
    }

    files.sort();

    if (cmdlineParser.isSet(descriptorStatsOption)) {
        showDescriptorStats(files);
    } else {
        showInfo(files);
    }

    return 0;
}
