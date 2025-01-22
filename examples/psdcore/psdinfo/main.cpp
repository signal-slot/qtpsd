// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QMetaEnum>
#include <QtCore/QLoggingCategory>

#include <QtPsdCore/QPsdParser>

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

int main(int argc, char *argv[])
{
    qSetMessagePattern("");
    // QLoggingCategory::setFilterRules("qt.psdcore.effectslayer.debug=true");

    QCoreApplication::setOrganizationName("Signal Slot Inc.");
    QCoreApplication::setOrganizationDomain("signal-slot.co.jp");
    QCoreApplication::setApplicationName("PsdInfo");
    QCoreApplication::setApplicationVersion("0.1.0");

    QCoreApplication app(argc, argv);

    QCommandLineParser cmdlineParser;
    cmdlineParser.setApplicationDescription("show PSD information");
    const QCommandLineOption versionOption = cmdlineParser.addVersionOption();
    cmdlineParser.addPositionalArgument("psdfile", "psd filename");

    cmdlineParser.process(app);

    if (cmdlineParser.isSet(versionOption)) {
        cmdlineParser.showVersion();
        Q_UNREACHABLE_RETURN(0);
    }

    const QStringList positionalArguments = cmdlineParser.positionalArguments();
    if (positionalArguments.size() != 1) {
        cmdlineParser.showHelp();
        Q_UNREACHABLE_RETURN(0);
    }

    QPsdParser parser;
    const QString &filename = positionalArguments.at(0);
    parser.load(filename);

    const auto fileHeader = parser.fileHeader();
    
    QTextStream out(stdout);
    out.setEncoding(QStringConverter::Encoding::System);
    out << u"filename:%1"_s.arg(filename) << Qt::endl;
    out << u"width:%1"_s.arg(fileHeader.width()) << Qt::endl;
    out << u"height:%1"_s.arg(fileHeader.height()) << Qt::endl;
    out << u"color mode:%1"_s.arg(colorModeString(fileHeader.colorMode())) << Qt::endl;
    out << u"color depth:%1"_s.arg(fileHeader.depth()) << Qt::endl;
    out << u"channels:%1"_s.arg(fileHeader.channels()) << Qt::endl;

    return 0;
}
