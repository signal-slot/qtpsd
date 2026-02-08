// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdfontmapper.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDataStream>
#include <QtCore/QMutex>
#include <QtCore/QReadWriteLock>
#include <QtCore/QSettings>
#include <QtGui/QFontDatabase>
#include <QtGui/QFontInfo>
#include <QtGui/QRawFont>

QT_BEGIN_NAMESPACE

// Parse OpenType 'name' table to extract a specific name ID
// Returns empty string if not found
static QString parseNameTable(const QByteArray &nameTable, quint16 targetNameId)
{
    if (nameTable.size() < 6) {
        return {};
    }

    QDataStream stream(nameTable);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 format, count, stringOffset;
    stream >> format >> count >> stringOffset;

    if (format > 1 || nameTable.size() < static_cast<int>(6 + count * 12)) {
        return {};
    }

    // Read name records
    for (quint16 i = 0; i < count; ++i) {
        quint16 platformId, encodingId, languageId, nameId, length, offset;
        stream >> platformId >> encodingId >> languageId >> nameId >> length >> offset;

        if (nameId != targetNameId) {
            continue;
        }

        // Extract the string
        int stringStart = stringOffset + offset;
        if (stringStart + length > nameTable.size()) {
            continue;
        }

        QByteArray stringData = nameTable.mid(stringStart, length);

        // Platform 3 (Windows) with encoding 1 (Unicode BMP) - UTF-16BE
        if (platformId == 3 && encodingId == 1) {
            // Convert UTF-16BE to QString
            QString result;
            for (int j = 0; j + 1 < stringData.size(); j += 2) {
                quint16 ch = (static_cast<quint8>(stringData[j]) << 8) | static_cast<quint8>(stringData[j + 1]);
                result += QChar(ch);
            }
            return result;
        }
        // Platform 1 (Mac) with encoding 0 (Roman) - ASCII-like
        else if (platformId == 1 && encodingId == 0) {
            return QString::fromLatin1(stringData);
        }
        // Platform 0 (Unicode) - UTF-16BE
        else if (platformId == 0) {
            QString result;
            for (int j = 0; j + 1 < stringData.size(); j += 2) {
                quint16 ch = (static_cast<quint8>(stringData[j]) << 8) | static_cast<quint8>(stringData[j + 1]);
                result += QChar(ch);
            }
            return result;
        }
    }

    return {};
}

// Get PostScript name from a font
static QString getPostScriptName(const QString &family, const QString &style = QString())
{
    QFont font(family);
    if (!style.isEmpty()) {
        font.setStyleName(style);
    }

    QRawFont rawFont = QRawFont::fromFont(font);
    if (!rawFont.isValid()) {
        return {};
    }

    QByteArray nameTable = rawFont.fontTable("name");
    if (nameTable.isEmpty()) {
        return {};
    }

    // Name ID 6 = PostScript name
    return parseNameTable(nameTable, 6);
}

class QPsdFontMapper::Private
{
public:
    mutable QReadWriteLock lock;

    // Global font mappings (persistent via QSettings)
    QHash<QString, QString> globalMappings;

    // Per-PSD font mappings (psd path -> font mappings)
    QHash<QString, QHash<QString, QString>> contextMappings;

    // Cache for resolved fonts
    mutable QHash<QString, QFont> fontCache;

    // PostScript name -> Qt family name mapping
    QHash<QString, QString> postScriptToFamily;

    // Cached list of available font families
    QStringList availableFamilies;

    Private() {
        availableFamilies = QFontDatabase::families();
        buildPostScriptNameTable();
    }

    // Build the PostScript name to family name mapping table
    void buildPostScriptNameTable();

    // Parse PostScript name into family and style components
    static QPair<QString, QString> parsePostScriptName(const QString &name);

    // Try to find matching font from available families
    QFont findMatchingFont(const QString &family, const QString &style) const;

    // Automatic font resolution using improved algorithm
    QFont autoResolveFont(const QString &fontName) const;
};

void QPsdFontMapper::Private::buildPostScriptNameTable()
{
    for (const QString &family : availableFamilies) {
        // Get PostScript name for default style
        QString psName = getPostScriptName(family);
        if (!psName.isEmpty()) {
            postScriptToFamily.insert(psName, family);
        }

        // Also get PostScript names for each style
        const QStringList styles = QFontDatabase::styles(family);
        for (const QString &style : styles) {
            psName = getPostScriptName(family, style);
            if (!psName.isEmpty() && !postScriptToFamily.contains(psName)) {
                postScriptToFamily.insert(psName, family);
            }
        }
    }
}

// Normalize a font name for comparison: lowercase, remove non-alphanumeric
static QString normalizeFontName(const QString &name)
{
    QString result;
    for (const QChar &c : name) {
        if (c.isLetterOrNumber()) {
            result += c.toLower();
        }
    }
    return result;
}

// Extract style keywords from font name
static QStringList extractStyleKeywords(const QString &name)
{
    static const QStringList styleKeywords = {
        "bold"_L1, "italic"_L1, "oblique"_L1, "light"_L1, "medium"_L1,
        "regular"_L1, "thin"_L1, "heavy"_L1, "black"_L1, "semibold"_L1,
        "demibold"_L1, "extrabold"_L1, "ultralight"_L1, "condensed"_L1,
        "extended"_L1, "narrow"_L1, "wide"_L1
    };

    QStringList found;
    QString lower = name.toLower();
    for (const QString &kw : styleKeywords) {
        if (lower.contains(kw)) {
            found.append(kw);
        }
    }
    return found;
}

QPair<QString, QString> QPsdFontMapper::Private::parsePostScriptName(const QString &name)
{
    // Common style suffixes to detect
    static const QStringList styleSuffixes = {
        "Bold"_L1, "Italic"_L1, "Light"_L1, "Medium"_L1, "Regular"_L1,
        "Thin"_L1, "Heavy"_L1, "Black"_L1, "SemiBold"_L1, "ExtraBold"_L1,
        "BoldItalic"_L1, "LightItalic"_L1, "MediumItalic"_L1
    };

    QString family = name;
    QString style;

    // First try splitting by hyphen
    int hyphenPos = name.lastIndexOf('-'_L1);
    if (hyphenPos > 0) {
        family = name.left(hyphenPos);
        style = name.mid(hyphenPos + 1);
    } else if (name.contains('_'_L1)) {
        // Try splitting by last underscore before style keyword
        for (const QString &suffix : styleSuffixes) {
            if (name.endsWith(suffix)) {
                int pos = name.length() - suffix.length();
                if (pos > 0 && name.at(pos - 1) == '_'_L1) {
                    family = name.left(pos - 1);
                    style = suffix;
                    break;
                }
            }
        }
        // If no style found, check for underscore-separated style
        if (style.isEmpty()) {
            int lastUnderscore = name.lastIndexOf('_'_L1);
            if (lastUnderscore > 0) {
                QString potentialStyle = name.mid(lastUnderscore + 1);
                for (const QString &suffix : styleSuffixes) {
                    if (potentialStyle.compare(suffix, Qt::CaseInsensitive) == 0) {
                        family = name.left(lastUnderscore);
                        style = potentialStyle;
                        break;
                    }
                }
            }
        }
    }

    // Expand CamelCase: insert spaces before capitals that follow lowercase
    QString expandedFamily;
    for (int i = 0; i < family.length(); ++i) {
        QChar c = family.at(i);
        if (i > 0) {
            QChar prev = family.at(i - 1);
            // Insert space: before uppercase after lowercase, or before uppercase that starts a lowercase sequence
            if ((c.isUpper() && prev.isLower()) ||
                (c.isUpper() && i + 1 < family.length() && family.at(i + 1).isLower() && prev.isUpper())) {
                expandedFamily += ' '_L1;
            }
            // Replace underscores with spaces
            if (prev == '_'_L1) {
                if (!expandedFamily.isEmpty() && expandedFamily.back() == '_'_L1) {
                    expandedFamily.chop(1);
                    expandedFamily += ' '_L1;
                }
            }
        }
        if (c != '_'_L1) {
            expandedFamily += c;
        } else {
            expandedFamily += ' '_L1;
        }
    }

    return qMakePair(expandedFamily.simplified(), style);
}

QFont QPsdFontMapper::Private::findMatchingFont(const QString &family, const QString &style) const
{
    QFont font;

    // 1. Try exact match: "Family Style" as family name
    if (!style.isEmpty()) {
        QString familyWithStyle = family + " "_L1 + style;
        if (availableFamilies.contains(familyWithStyle)) {
            font.setFamily(familyWithStyle);
            font.setStyleName(style);
            return font;
        }
    }

    // 2. Try family name directly
    if (availableFamilies.contains(family)) {
        font.setFamily(family);
        if (!style.isEmpty()) {
            font.setStyleName(style);
        }
        return font;
    }

    // 3. Try case-insensitive match
    for (const QString &availableFamily : availableFamilies) {
        if (availableFamily.compare(family, Qt::CaseInsensitive) == 0) {
            font.setFamily(availableFamily);
            if (!style.isEmpty()) {
                font.setStyleName(style);
            }
            return font;
        }
    }

    // 4. Try matching without spaces (for fonts like "MyriadPro" vs "Myriad Pro")
    QString familyNoSpaces = family;
    familyNoSpaces.remove(' '_L1);
    for (const QString &availableFamily : availableFamilies) {
        QString availableNoSpaces = availableFamily;
        availableNoSpaces.remove(' '_L1);
        if (availableNoSpaces.compare(familyNoSpaces, Qt::CaseInsensitive) == 0) {
            font.setFamily(availableFamily);
            if (!style.isEmpty()) {
                font.setStyleName(style);
            }
            return font;
        }
    }

    // 5. Try partial prefix match (useful for fonts with version suffixes)
    for (const QString &availableFamily : availableFamilies) {
        QString availableNoSpaces = availableFamily;
        availableNoSpaces.remove(' '_L1);
        if (availableNoSpaces.startsWith(familyNoSpaces, Qt::CaseInsensitive) ||
            familyNoSpaces.startsWith(availableNoSpaces, Qt::CaseInsensitive)) {
            font.setFamily(availableFamily);
            if (!style.isEmpty()) {
                font.setStyleName(style);
            }
            return font;
        }
    }

    // 6. Fuzzy match using normalized names (remove all non-alphanumeric)
    // This handles cases like "LINESeedJP_A_TTF" vs "LINE Seed JP App_TTF"
    QString normalizedFamily = normalizeFontName(family);
    // Remove style from normalized name for better matching
    QString normalizedFamilyNoStyle = normalizedFamily;
    if (!style.isEmpty()) {
        QString normalizedStyle = normalizeFontName(style);
        if (normalizedFamilyNoStyle.endsWith(normalizedStyle)) {
            normalizedFamilyNoStyle.chop(normalizedStyle.length());
        }
    }

    QString bestMatch;
    int bestScore = 0;

    for (const QString &availableFamily : availableFamilies) {
        QString normalizedAvailable = normalizeFontName(availableFamily);

        // Check if the style matches
        QStringList requestedStyles = extractStyleKeywords(style);
        QStringList availableStyles = extractStyleKeywords(availableFamily);
        bool styleMatches = requestedStyles.isEmpty() ||
            std::any_of(requestedStyles.begin(), requestedStyles.end(),
                        [&availableStyles](const QString &s) { return availableStyles.contains(s); });

        if (!styleMatches) {
            continue;
        }

        // Remove style keywords from available for family comparison
        QString normalizedAvailableNoStyle = normalizedAvailable;
        for (const QString &kw : availableStyles) {
            normalizedAvailableNoStyle.remove(kw);
        }

        // Calculate similarity score
        int score = 0;

        // Exact normalized match (highest priority)
        if (normalizedFamilyNoStyle == normalizedAvailableNoStyle) {
            score = 1000;
        }
        // One contains the other
        else if (normalizedFamilyNoStyle.contains(normalizedAvailableNoStyle) ||
                 normalizedAvailableNoStyle.contains(normalizedFamilyNoStyle)) {
            int minLen = qMin(normalizedFamilyNoStyle.length(), normalizedAvailableNoStyle.length());
            score = 500 + minLen;
        }
        // Common prefix
        else {
            int commonPrefix = 0;
            int minLen = qMin(normalizedFamilyNoStyle.length(), normalizedAvailableNoStyle.length());
            for (int i = 0; i < minLen; ++i) {
                if (normalizedFamilyNoStyle.at(i) == normalizedAvailableNoStyle.at(i)) {
                    commonPrefix++;
                } else {
                    break;
                }
            }
            // Require at least 4 chars common prefix to consider a match
            if (commonPrefix >= 4) {
                score = commonPrefix * 10;
            }
        }

        if (score > bestScore) {
            bestScore = score;
            bestMatch = availableFamily;
        }
    }

    if (!bestMatch.isEmpty() && bestScore >= 40) {  // Minimum threshold
        font.setFamily(bestMatch);
        if (!style.isEmpty()) {
            font.setStyleName(style);
        }
        return font;
    }

    // No match found - return font with the original name
    font.setFamily(family);
    if (!style.isEmpty()) {
        font.setStyleName(style);
    }
    return font;
}

QFont QPsdFontMapper::Private::autoResolveFont(const QString &fontName) const
{
    // First try exact match
    QFont font(fontName);
    if (QFontInfo(font).exactMatch()) {
        return font;
    }

    // Check if we have a PostScript name to family mapping
    // The font name from PSD is typically a PostScript name
    if (postScriptToFamily.contains(fontName)) {
        QString family = postScriptToFamily.value(fontName);
        font.setFamily(family);
        // Try to extract style from the PostScript name
        auto [parsedFamily, style] = parsePostScriptName(fontName);
        if (!style.isEmpty()) {
            font.setStyleName(style);
        }
        return font;
    }

    // Also try without hyphen suffix (style part)
    // e.g., "LINESeedJP_A_TTF-Bold" -> "LINESeedJP_A_TTF"
    int hyphenPos = fontName.lastIndexOf('-'_L1);
    if (hyphenPos > 0) {
        QString baseName = fontName.left(hyphenPos);
        QString stylePart = fontName.mid(hyphenPos + 1);
        if (postScriptToFamily.contains(baseName)) {
            QString family = postScriptToFamily.value(baseName);
            font.setFamily(family);
            font.setStyleName(stylePart);
            return font;
        }
    }

    // Parse the PostScript name and find matching font
    auto [family, style] = parsePostScriptName(fontName);

    // Find matching font using fuzzy matching
    return findMatchingFont(family, style);
}

QPsdFontMapper::QPsdFontMapper()
    : d(new Private)
{
    loadGlobalMappings();
}

QPsdFontMapper::~QPsdFontMapper() = default;

QPsdFontMapper *QPsdFontMapper::instance()
{
    static QPsdFontMapper instance;
    return &instance;
}

void QPsdFontMapper::setGlobalMapping(const QString &fromFont, const QString &toFont)
{
    QWriteLocker locker(&d->lock);
    d->globalMappings.insert(fromFont, toFont);
    d->fontCache.clear();
}

void QPsdFontMapper::removeGlobalMapping(const QString &fromFont)
{
    QWriteLocker locker(&d->lock);
    d->globalMappings.remove(fromFont);
    d->fontCache.clear();
}

QHash<QString, QString> QPsdFontMapper::globalMappings() const
{
    QReadLocker locker(&d->lock);
    return d->globalMappings;
}

void QPsdFontMapper::setContextMappings(const QString &psdPath, const QHash<QString, QString> &mappings)
{
    QWriteLocker locker(&d->lock);
    d->contextMappings.insert(psdPath, mappings);
    d->fontCache.clear();
}

QHash<QString, QString> QPsdFontMapper::contextMappings(const QString &psdPath) const
{
    QReadLocker locker(&d->lock);
    return d->contextMappings.value(psdPath);
}

void QPsdFontMapper::clearContextMappings(const QString &psdPath)
{
    QWriteLocker locker(&d->lock);
    d->contextMappings.remove(psdPath);
    d->fontCache.clear();
}

QFont QPsdFontMapper::resolveFont(const QString &fontName, const QString &psdPath) const
{
    // Create cache key that includes both font name and context
    QString cacheKey = psdPath.isEmpty() ? fontName : (psdPath + "|"_L1 + fontName);

    {
        QReadLocker locker(&d->lock);
        if (d->fontCache.contains(cacheKey)) {
            return d->fontCache.value(cacheKey);
        }
    }

    QFont resultFont;
    QString mappedName = fontName;

    {
        QReadLocker locker(&d->lock);

        // 1. Check per-PSD mapping first (highest priority)
        if (!psdPath.isEmpty() && d->contextMappings.contains(psdPath)) {
            const auto &psdMappings = d->contextMappings.value(psdPath);
            if (psdMappings.contains(fontName)) {
                mappedName = psdMappings.value(fontName);
            }
        }

        // 2. If not found in per-PSD, check global mapping
        if (mappedName == fontName && d->globalMappings.contains(fontName)) {
            mappedName = d->globalMappings.value(fontName);
        }
    }

    // 3. Try the mapped name first (if mapping was found)
    if (mappedName != fontName) {
        resultFont = QFont(mappedName);
        if (QFontInfo(resultFont).exactMatch()) {
            QWriteLocker locker(&d->lock);
            d->fontCache.insert(cacheKey, resultFont);
            return resultFont;
        }
    }

    // 4. Fall back to automatic resolution
    resultFont = d->autoResolveFont(fontName);

    // Log warning if no matching font was found
    if (!QFontInfo(resultFont).exactMatch()) {
        auto [family, style] = Private::parsePostScriptName(fontName);
        qWarning() << fontName << "doesn't match any font - parsed as" << family << style;
    }

    QWriteLocker locker(&d->lock);
    d->fontCache.insert(cacheKey, resultFont);
    return resultFont;
}

void QPsdFontMapper::loadGlobalMappings()
{
    QWriteLocker locker(&d->lock);
    d->globalMappings.clear();

    QSettings settings;
    settings.beginGroup("FontMapping"_L1);
    const QStringList keys = settings.childKeys();
    for (const QString &key : keys) {
        d->globalMappings.insert(key, settings.value(key).toString());
    }
    settings.endGroup();

    d->fontCache.clear();
}

void QPsdFontMapper::saveGlobalMappings() const
{
    QReadLocker locker(&d->lock);

    QSettings settings;
    settings.beginGroup("FontMapping"_L1);

    // Clear existing mappings in settings
    settings.remove(""_L1);

    // Write current mappings
    for (auto it = d->globalMappings.constBegin(); it != d->globalMappings.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
}

void QPsdFontMapper::loadFromHint(const QString &psdPath, const QJsonObject &fontMapping)
{
    QHash<QString, QString> mappings;
    for (auto it = fontMapping.constBegin(); it != fontMapping.constEnd(); ++it) {
        mappings.insert(it.key(), it.value().toString());
    }

    QWriteLocker locker(&d->lock);
    d->contextMappings.insert(psdPath, mappings);
    d->fontCache.clear();
}

QJsonObject QPsdFontMapper::toHint(const QString &psdPath) const
{
    QReadLocker locker(&d->lock);
    QJsonObject result;

    if (d->contextMappings.contains(psdPath)) {
        const auto &mappings = d->contextMappings.value(psdPath);
        for (auto it = mappings.constBegin(); it != mappings.constEnd(); ++it) {
            result.insert(it.key(), it.value());
        }
    }

    return result;
}

void QPsdFontMapper::clearCache()
{
    QWriteLocker locker(&d->lock);
    d->fontCache.clear();
}

QT_END_NAMESPACE
