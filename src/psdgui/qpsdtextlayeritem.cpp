// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdtextlayeritem.h"

#include <QtCore/QCborMap>
#include <QtGui/QFontInfo>
#include <QtGui/QFontDatabase>
#include <QtGui/QFontMetrics>

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
    QString familySpecified = font.family().section('-'_L1, 0, 0);
    QString styleSpecified = font.family().section('-'_L1, 1, 1);

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
            familyWithoutSpaces.remove(' '_L1);
            if (familyWithoutSpaces == familySpecified) {
                font.setFamily(family);
                font.setStyleName(styleSpecified);
                cache.insert(name, font);
                return font;
            }
        }
    }

    QString familySpecifiedBeginning = familySpecified.section(' '_L1, 0, 0);
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
    QRectF fontAdjustedBounds;
    TextType textType;
};

QPsdTextLayerItem::QPsdTextLayerItem(const QPsdLayerRecord &record)
    : QPsdAbstractLayerItem(record)
    , d(new Private)
{
    const auto additionalLayerInformation = record.additionalLayerInformation();
    const auto tysh = additionalLayerInformation.value("TySh").value<QPsdTypeToolObjectSetting>();
    const auto textData = tysh.textData();
    const auto transformParam = tysh.transform();
    // Use identity matrix as default if transform parameters are missing or incomplete
    const QTransform transform = (transformParam.size() >= 6)
        ? QTransform(
            transformParam[0], transformParam[1],
            transformParam[2], transformParam[3],
            transformParam[4], transformParam[5])
        : QTransform();

    const auto engineDataData = textData.data().value("EngineData").toByteArray();
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

    // Parse paragraph-level alignment for horizontal alignment
    const auto paragraphRun = engineDict.value("ParagraphRun"_L1).toMap();
    const auto paragraphRunArray = paragraphRun.value("RunArray"_L1).toArray();
    Qt::Alignment defaultHorizontalAlignment = Qt::AlignLeft; // Default to left alignment

    if (!paragraphRunArray.isEmpty()) {
        const auto firstParagraph = paragraphRunArray.first().toMap();
        const auto paragraphSheet = firstParagraph.value("ParagraphSheet"_L1).toMap();
        const auto paragraphProperties = paragraphSheet.value("Properties"_L1).toMap();

        if (paragraphProperties.contains("Justification"_L1)) {
            const auto justification = paragraphProperties.value("Justification"_L1).toInteger();
            switch (justification) {
            case 0: // left
                defaultHorizontalAlignment = Qt::AlignLeft;
                break;
            case 1: // right
                defaultHorizontalAlignment = Qt::AlignRight;
                break;
            case 2: // center
                defaultHorizontalAlignment = Qt::AlignHCenter;
                break;
            case 3: // justify
                defaultHorizontalAlignment = Qt::AlignJustify;
                break;
            default:
                qDebug() << "Unknown justification:" << justification;
                break;
            }
        }
    }

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
        if (!ligatures && styleSheetData.contains("Tracking"_L1)) {
            const auto tracking = styleSheetData.value("Tracking"_L1).toDouble();
            run.font.setLetterSpacing(QFont::PercentageSpacing, tracking);
        }
        const auto fontSize = styleSheetData.value("FontSize"_L1).toDouble();
        run.font.setPointSizeF(transform.m22() * fontSize / 1.5);
        const auto runLength = runLengthArray.at(i).toInteger();
        // replace 0x03 (ETX) to newline for Shift+Enter in Photoshop
        // https://community.adobe.com/t5/photoshop-ecosystem-discussions/replacing-quot-shift-enter-quot-aka-etx-aka-lt-0x03-gt-aka-end-of-transmission-character-within-text/td-p/12517124
        run.text = text.mid(start, runLength).replace('\x03'_L1, '\n'_L1);
        start += runLength;

        // Set horizontal alignment from paragraph justification
        run.alignment = defaultHorizontalAlignment;

        // Parse vertical alignment from StyleRunAlignment
        if (styleSheetData.contains("StyleRunAlignment"_L1)) {
            // https://documentation.help/Illustrator-CS6/pe_StyleRunAlignmentType.html
            const auto styleRunAlignment = styleSheetData.value("StyleRunAlignment"_L1).toInteger();
            // Qt doesn't support icf
            // https://learn.microsoft.com/en-us/typography/opentype/spec/baselinetags#icfbox
            switch (styleRunAlignment) {
            case 0: // bottom
                run.alignment |= Qt::AlignBottom;
                break;
            case 1: // icf bottom
                run.alignment |= Qt::AlignBottom;
                break;
            case 2: // roman baseline
                run.alignment |= Qt::AlignBottom;
                break;
            case 3: // center
                run.alignment |= Qt::AlignVCenter;
                break;
            case 4: // icf top
                run.alignment |= Qt::AlignTop;
                break;
            case 5: // top
                run.alignment |= Qt::AlignTop;
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

    qreal contentHeight = 0;
    qreal lineHeight = -1;
    qreal lineLeading = -1;
    for (int i = 0; i < d->runs.length(); i++) {
        QFontMetrics fontMetrics(d->runs.at(i).font);
        if (lineHeight < fontMetrics.height()) {
            lineHeight = fontMetrics.height();
            lineLeading = fontMetrics.leading();
        }

        const auto texts = d->runs.at(i).text.trimmed().split("\n");
        if (texts.size() > 1) {
            contentHeight += lineHeight + lineLeading + (fontMetrics.height() + fontMetrics.leading()) * (texts.size() - 2);
        }
    }
    contentHeight += lineHeight * 1.1;   // 1.1 is ad-hoc param

    d->bounds = tysh.bounds();

    d->fontAdjustedBounds = d->bounds;
    d->fontAdjustedBounds.setHeight(contentHeight);

    const auto rendered = engineDict.value("Rendered"_L1).toMap();
    const auto shapes = rendered.value("Shapes"_L1).toMap();
    const auto childrenArray = shapes.value("Children"_L1).toArray();
    const auto child = childrenArray.at(0).toMap();
    const auto shapeType = child.value("ShapeType"_L1).toInteger();

    d->textType = shapeType == 1 ? TextType::ParagraphText : TextType::PointText;
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

QRectF QPsdTextLayerItem::fontAdjustedBounds() const {
    return d->fontAdjustedBounds;
}

QPsdTextLayerItem::TextType QPsdTextLayerItem::textType() const {
    return d->textType;
}

QT_END_NAMESPACE
