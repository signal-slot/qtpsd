// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdtextlayeritem.h"
#include "qpsdfontmapper.h"

#include <QtCore/QCborMap>
#include <QtGui/QFontInfo>
#include <QtGui/QFontDatabase>

#include <QtPsdCore/QPsdTypeToolObjectSetting>
#include <QtPsdCore/QPsdEngineDataParser>

QT_BEGIN_NAMESPACE

namespace {
// Thread-local storage for current PSD path context
Q_CONSTINIT static thread_local QString s_currentPsdPath;
}

class QPsdTextLayerItem::Private
{
public:
    QList<Run> runs;
    QRectF bounds;
    QRectF textFrame;
    TextType textType = TextType::PointText;
    QPointF textOrigin; // Text baseline anchor (tx, ty from transform)
};

QPsdTextLayerItem::QPsdTextLayerItem(const QPsdLayerRecord &record)
    : QPsdAbstractLayerItem(record)
    , d(new Private)
{
    auto appendFallbackRun = [&](const QString &text = QString()) {
        Run run;
        run.text = text;
        run.font = QFont();
        run.color = Qt::black;
        run.alignment = Qt::AlignLeft | Qt::AlignVCenter;
        d->runs.append(run);
    };

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

    // Store text origin (tx, ty) - this is the baseline anchor for point text
    if (transformParam.size() >= 6) {
        d->textOrigin = QPointF(transformParam[4], transformParam[5]);
    }

    const auto engineDataData = textData.data().value("EngineData").toByteArray();
    QPsdEngineDataParser::ParseError parseError;
    const auto engineData = QPsdEngineDataParser::parseEngineData(engineDataData, &parseError);
    if (parseError) {
        qWarning() << "QPsdTextLayerItem: failed to parse EngineData:" << parseError.errorMessage;
        appendFallbackRun();
        d->bounds = tysh.bounds();
        return;
    }
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

    const QCborMap baseStyleSheetData =
        styleSheetSet.isEmpty()
            ? QCborMap()
            : styleSheetSet.first().toMap().value("StyleSheetData"_L1).toMap();

    int start = 0;
    const int runCount = qMin(runLengthArray.size(), runArray.size());
    for (int i = 0; i < runCount; i++) {
        Run run;
        const auto map = runArray.at(i).toMap();
        const auto styleSheet = map.value("StyleSheet"_L1).toMap();
        const auto styleSheetDataOverride = styleSheet.value("StyleSheetData"_L1).toMap();

        auto styleSheetData = baseStyleSheetData;
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
            if (values.size() >= 4) {
                run.color.setRgbF(values.at(1).toDouble(), values.at(2).toDouble(), values.at(3).toDouble(), values.at(0).toDouble());
            } else {
                qWarning() << "QPsdTextLayerItem: invalid FillColor.Values size" << values.size();
                run.color = Qt::black;
            }
            break; }
        default:
            qWarning() << "Unknown color type" << colorType;
            break;
        }
        const auto fontIndex = styleSheetData.value("Font"_L1).toInteger();
        if (fontIndex >= 0 && fontIndex < fontSet.size()) {
            const auto fontInfo = fontSet.at(fontIndex).toMap();
            run.originalFontName = fontInfo.value("Name"_L1).toString();
            run.font = QPsdFontMapper::instance()->resolveFont(run.originalFontName, s_currentPsdPath);
        } else {
            qWarning() << "QPsdTextLayerItem: invalid font index" << fontIndex << "fontSet.size=" << fontSet.size();
            run.font = QFont();
        }
        run.font.setKerning(autoKerning);
        const auto ligatures = styleSheetData.value("Ligatures"_L1).toBool();
        if (!ligatures && styleSheetData.contains("Tracking"_L1)) {
            const auto tracking = styleSheetData.value("Tracking"_L1).toDouble();
            run.font.setLetterSpacing(QFont::PercentageSpacing, tracking);
        }
        const auto fontSize = styleSheetData.value("FontSize"_L1).toDouble();
        run.font.setPointSizeF(transform.m22() * fontSize);
        const auto runLength = qMax(0, runLengthArray.at(i).toInteger());
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

    if (d->runs.isEmpty() && !text.isEmpty()) {
        appendFallbackRun(text);
        d->runs.last().alignment = defaultHorizontalAlignment | Qt::AlignVCenter;
    }

    if (d->runs.isEmpty())
        appendFallbackRun();

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

    const auto rendered = engineDict.value("Rendered"_L1).toMap();
    const auto shapes = rendered.value("Shapes"_L1).toMap();
    const auto childrenArray = shapes.value("Children"_L1).toArray();
    if (!childrenArray.isEmpty()) {
        const auto child = childrenArray.at(0).toMap();
        const auto shapeType = child.value("ShapeType"_L1).toInteger();
        d->textType = shapeType == 1 ? TextType::ParagraphText : TextType::PointText;

        // Parse text frame from BoxBounds for ParagraphText
        if (d->textType == TextType::ParagraphText) {
            const auto cookie = child.value("Cookie"_L1).toMap();
            const auto photoshop = cookie.value("Photoshop"_L1).toMap();
            const auto boxBounds = photoshop.value("BoxBounds"_L1).toArray();
            if (boxBounds.size() == 4) {
                const auto left = boxBounds.at(0).toDouble();
                const auto top = boxBounds.at(1).toDouble();
                const auto right = boxBounds.at(2).toDouble();
                const auto bottom = boxBounds.at(3).toDouble();
                // Transform from local to document coordinates using the TySh transform
                const auto docLeft = transform.map(QPointF(left, top)).x();
                const auto docTop = transform.map(QPointF(left, top)).y();
                const auto docRight = transform.map(QPointF(right, bottom)).x();
                const auto docBottom = transform.map(QPointF(right, bottom)).y();
                d->textFrame = QRectF(docLeft, docTop, docRight - docLeft, docBottom - docTop);
            }
        }
    }
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

QPsdTextLayerItem::TextType QPsdTextLayerItem::textType() const {
    return d->textType;
}

QPointF QPsdTextLayerItem::textOrigin() const {
    return d->textOrigin;
}

QRectF QPsdTextLayerItem::textFrame() const {
    return d->textFrame;
}

void QPsdTextLayerItem::setCurrentPsdPath(const QString &path)
{
    s_currentPsdPath = path;
}

QString QPsdTextLayerItem::currentPsdPath()
{
    return s_currentPsdPath;
}

QT_END_NAMESPACE
