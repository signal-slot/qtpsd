// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdtextlayeritem.h"

#include <QtCore/QCborMap>
#include <QtGui/QFontInfo>
#include <QtGui/QFontDatabase>

#include <QtPsdCore/QPsdTypeToolObjectSetting>
#include <QtPsdCore/QPsdEngineDataParser>

QT_BEGIN_NAMESPACE

namespace {
// MyriadPro-Bold -> "Myriad Pro", "Bold"
// SourceHanSans-Medium -> "源ノ角ゴシック JP Medium", "Medium"
// SourceHanSans-Bold -> "源ノ角ゴシック JP", "Bold"
QFont findProperFont(const QString &name) {
    static QHash<QString, QFont> cache;
    if (cache.contains(name))
        return cache.value(name);
    QFont font(name);
    if (QFontInfo(font).exactMatch()) {
        cache.insert(name, font);
        return font;
    }
    QString familySpecified = font.family().section(QLatin1Char('-'), 0, 0);
    QString styleSpecified = font.family().section(QLatin1Char('-'), 1, 1);

    static const QHash<QString, QString> substitute = {
                                                       { "MyriadPro"_L1, "Myriad Pro"_L1 },
                                                       { "SourceHanSans"_L1, u"源ノ角ゴシック JP"_s },
                                                       };
    if (substitute.contains(familySpecified)) {
        familySpecified = substitute.value(familySpecified);
    }

    static const auto families = QFontDatabase::families();
    if (families.contains(familySpecified + " " + styleSpecified)) {
        font.setFamily(familySpecified + " " + styleSpecified);
        font.setStyleName(styleSpecified);
        cache.insert(name, font);
        return font;
    } else if (families.contains(familySpecified)) {
        font.setFamily(familySpecified);
        font.setStyleName(styleSpecified);
        cache.insert(name, font);
        return font;
    } else {
        for (const auto &family : families) {
            QString familyWithoutSpaces = family;
            familyWithoutSpaces.remove(QLatin1Char(' '));
            if (familyWithoutSpaces == familySpecified) {
                font.setFamily(family);
                font.setStyleName(styleSpecified);
                cache.insert(name, font);
                return font;
            }
        }
    }

    QString familySpecifiedBeginning = familySpecified.section(QLatin1Char(' '), 0, 0);
    for (const auto &family : families) {
        if (!family.startsWith(familySpecifiedBeginning)) {
            continue;
        }
        qDebug() << family << (family == familySpecified + " " + styleSpecified) << (family == familySpecified);
    }
    qWarning() << name << "doesn't match any font" << familySpecified << styleSpecified;
    cache.insert(name, font);
    return font;
}

}

class QPsdTextLayerItem::Private
{
public:
    QList<Run> runs;
    QRectF bounds;
};

QPsdTextLayerItem::QPsdTextLayerItem(const QPsdLayerRecord &record)
    : QPsdAbstractLayerItem(record)
    , d(new Private)
{
    const auto additionalLayerInformation = record.additionalLayerInformation();
    const auto tysh = additionalLayerInformation.value("TySh"_L1).value<QPsdTypeToolObjectSetting>();
    const auto textData = tysh.textData();
    const auto transformParam = tysh.transform();
    const QTransform transform = QTransform(
        transformParam[0], transformParam[1],
        transformParam[2], transformParam[3],
        transformParam[4], transformParam[5]
    );

    const auto engineDataData = textData.data().value("EngineData"_L1).toByteArray();
    const auto engineData = QPsdEngineDataParser::parseEngineData(engineDataData);
    // qDebug().noquote() << QJsonDocument(engineData.toJsonObject()).toJson();

    const auto documentResources = engineData.value("DocumentResources"_L1).toMap();
    const auto fontSet = documentResources.value("FontSet"_L1).toArray();
    const auto styleSheetSet = documentResources.value("StyleSheetSet"_L1).toArray();

    const auto engineDict = engineData.value("EngineDict"_L1).toMap();
    const auto editor = engineDict.value("Editor"_L1).toMap();
    const auto text = editor.value("Text"_L1).toString().replace("\r"_L1, "\n"_L1);
    const auto styleRun = engineDict.value("StyleRun"_L1).toMap();
    const auto runArray = styleRun.value("RunArray"_L1).toArray();
    const auto runLengthArray = styleRun.value("RunLengthArray"_L1).toArray();

    int start = 0;
    for (int i = 0; i < runLengthArray.size(); i++) {
        Run run;
        const auto map = runArray.at(i).toMap();
        const auto styleSheet = map.value("StyleSheet"_L1).toMap();
        const auto styleSheetDataOverride = styleSheet.value("StyleSheetData"_L1).toMap();

        auto styleSheetData = styleSheetSet.first().toMap().value("StyleSheetData"_L1).toMap();
        // override base values with styleSheetData
        for (const auto &key : styleSheetDataOverride.keys()) {
            styleSheetData[key] = styleSheetDataOverride[key];
        }

        const auto autoKerning = styleSheetData.value("AutoKerning"_L1).toBool();
        const auto fillColor = styleSheetData.value("FillColor"_L1).toMap();
        const auto colorType = fillColor.value("Type"_L1).toInteger();
        switch (colorType) {
        case 1: { // ARGB probably
            const auto values = fillColor.value("Values"_L1).toArray();
            run.color.setRgbF(values.at(1).toDouble(), values.at(2).toDouble(), values.at(3).toDouble(), values.at(0).toDouble());
            break; }
        default:
            qWarning() << "Unknown color type" << colorType;
            break;
        }
        const auto fontIndex = styleSheetData.value("Font"_L1).toInteger();
        const auto fontInfo = fontSet.at(fontIndex).toMap();
        run.font = findProperFont(fontInfo.value("Name"_L1).toString());
        run.font.setKerning(autoKerning);
        const auto ligatures = styleSheetData.value("Ligatures"_L1).toBool();
        if (!ligatures && styleSheetData.contains(QLatin1StringView("Tracking"))) {
            const auto tracking = styleSheetData.value("Tracking"_L1).toDouble();
            run.font.setLetterSpacing(QFont::PercentageSpacing, tracking);
        }
        const auto fontSize = styleSheetData.value("FontSize"_L1).toDouble();
        run.font.setPointSizeF(transform.m22() * fontSize);
        const auto runLength = runLengthArray.at(i).toInteger();
        run.text = text.mid(start, runLength);
        start += runLength;

        if (styleSheetData.contains(QLatin1StringView("StyleRunAlignment"))) {
            // https://documentation.help/Illustrator-CS6/pe_StyleRunAlignmentType.html
            const auto styleRunAlignment = styleSheetData.value("StyleRunAlignment"_L1).toInteger();
            // Qt doesn't support icf
            // https://learn.microsoft.com/en-us/typography/opentype/spec/baselinetags#icfbox
            switch (styleRunAlignment) {
            case 0: // bottom
                run.alignment = Qt::AlignBottom;
                break;
            case 1: // icf bottom
                run.alignment = Qt::AlignBottom;
                break;
            case 2: // roman baseline
                run.alignment = Qt::AlignBottom;
                break;
            case 3: // center
                run.alignment = Qt::AlignVCenter;
                break;
            case 4: // icf top
                run.alignment = Qt::AlignTop;
                break;
            case 5: // top
                run.alignment = Qt::AlignTop;
                break;
            default:
                qWarning() << "Unknown styleRunAlignment" << styleRunAlignment;
            }
        }
        d->runs.append(run);
    }

    if (d->runs.length() > 1) {
        // merge runs with the same font and color
        QList<Run> runs;
        Run previousRun;
        for (const auto &run : d->runs) {
            if (previousRun.text.isEmpty()) {
                previousRun = run;
                continue;
            }
            if (previousRun.font.toString() == run.font.toString() && previousRun.color == run.color && previousRun.alignment == run.alignment) {
                previousRun.text += run.text;
            } else {
                runs.append(previousRun);
                previousRun = run;
            }
        }
        if (!previousRun.text.isEmpty()) {
            runs.append(previousRun);
        }
        d->runs = runs;
    }

    d->bounds = tysh.bounds();
}

QPsdTextLayerItem::QPsdTextLayerItem()
    : QPsdAbstractLayerItem()
    , d(new Private)
{}

QPsdTextLayerItem::~QPsdTextLayerItem() = default;

QList<QPsdTextLayerItem::Run> QPsdTextLayerItem::runs() const
{
    return d->runs;
}

QRectF QPsdTextLayerItem::bounds() const {
    return d->bounds;
}

QT_END_NAMESPACE
