// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/qpsdqmlexporterplugin.h>
#include <QtPsdGui/qpsdtextlayeritem.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QProcess>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtGui/QFont>

QT_BEGIN_NAMESPACE

// Qt for MCUs exporter. Reuses the QML emission pipeline from
// QPsdQmlExporterPlugin and customises behaviour via virtual hooks to fit
// MCU's stricter qmltocpp / fontcompiler:
//   - Forces NoGPU mode (no Qt5Compat / MultiEffect / ShaderEffect available)
//   - Bundles a CJK-capable single-file font (auto-discovered via fontconfig)
//   - Writes a `.qmlproject` companion file alongside the .ui.qml outputs
//   - Strips QML constructs MCU rejects: Rectangle.border, unsupported
//     font.family, anchors.fill on component roots, Button.highlighted
//   - Replaces blend / adjustment composition with no-ops (non-Normal blends
//     are pre-flattened via the Photoshop merged image in the base class)
class QPsdExporterQtForMcuPlugin : public QPsdQmlExporterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdExporterFactoryInterface" FILE "qtformcu.json")
public:
    int priority() const override { return 9; }
    QIcon icon() const override {
        return QIcon(":/qtformcu/qtformcu.png");
    }
    QString name() const override {
        return tr("Qt for &MCUs");
    }
    ExportType exportType() const override { return QPsdExporterPlugin::Directory; }

protected:
    // ---- Hooks -----------------------------------------------------------

    void onBeforeExport(const ExportConfig & /*config*/) const override {
        // Reset per-export state.
        m_bundledFontFamily.clear();
        m_bundledFontFiles.clear();

        // Discover and bundle a single-file font (TTF/OTF) that covers the
        // scripts used by the PSD's text layers. The MCU fontcompiler does
        // not register sub-families of a .ttc collection, so a standalone
        // file is required.
        QString regularPath;

        const QByteArray override = qgetenv("QTPSD_MCU_FONT_FILE");
        if (!override.isEmpty() && QFile::exists(QString::fromLocal8Bit(override)))
            regularPath = QString::fromLocal8Bit(override);

        if (regularPath.isEmpty()) {
            QStringList langCandidates = detectFontLangs();
            if (langCandidates.isEmpty())
                langCandidates << "ja"_L1;
            langCandidates.append("en"_L1);

            for (const auto &lang : std::as_const(langCandidates)) {
                regularPath = pickFontForLang(lang);
                if (!regularPath.isEmpty())
                    break;
            }
        }

        if (regularPath.isEmpty())
            return;

        QString family;
        QProcess fq;
        fq.start("fc-query"_L1, { "--format=%{family[0]}\n"_L1, regularPath });
        if (fq.waitForFinished(5000) && fq.exitStatus() == QProcess::NormalExit)
            family = QString::fromLocal8Bit(fq.readAllStandardOutput()).trimmed();

        if (family.isEmpty()) {
            qWarning() << "Qt for MCU exporter: failed to determine family for"
                       << regularPath << "(install fontconfig or set QTPSD_MCU_FONT_FILE)";
            return;
        }

        dir.mkpath("fonts"_L1);
        m_bundledFontFamily = family;

        auto bundle = [&](const QString &src) {
            const QString destName = QFileInfo(src).fileName();
            const QString dest = dir.absoluteFilePath("fonts/"_L1 + destName);
            QFile::remove(dest);
            if (QFile::copy(src, dest))
                m_bundledFontFiles.append("fonts/"_L1 + destName);
        };

        bundle(regularPath);

        // If the PSD uses bold/medium/light text, also bundle the matching
        // weight variants of the same family so MCU renders authentic
        // weights instead of synthesising them from the Regular outline.
        const QSet<int> weights = detectFontWeights();
        for (int w : weights) {
            // Skip Regular (already bundled). Map QFont weight to fontconfig
            // weight scale: QFont::Normal(400)→80, Bold(700)→200, etc.
            if (w == QFont::Normal)
                continue;
            const int fcWeight = qFontToFontconfigWeight(w);
            const QString variant = pickFamilyVariant(family, fcWeight);
            if (variant.isEmpty() || variant == regularPath)
                continue;
            bundle(variant);
        }
    }

    // Scan PSD text runs and collect distinct QFont::Weight values used.
    QSet<int> detectFontWeights() const {
        QSet<int> weights;
        const QPsdExporterTreeItemModel *m = model();
        if (!m)
            return weights;
        std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &idx) {
            if (idx.isValid()) {
                const auto *item = m->layerItem(idx);
                if (const auto *txt = dynamic_cast<const QPsdTextLayerItem *>(item)) {
                    for (const auto &run : txt->runs())
                        weights.insert(run.font.weight());
                }
            }
            for (int r = 0; r < m->rowCount(idx); ++r)
                walk(m->index(r, 0, idx));
        };
        walk(QModelIndex {});
        return weights;
    }

    static int qFontToFontconfigWeight(int qfontWeight) {
        // fontconfig weight constants: thin=0, light=50, regular=80,
        // medium=100, bold=200, black=210. QFont uses CSS-style 100..900.
        if (qfontWeight <= 100) return 0;       // Thin
        if (qfontWeight <= 200) return 40;      // ExtraLight
        if (qfontWeight <= 300) return 50;      // Light
        if (qfontWeight <= 400) return 80;      // Regular
        if (qfontWeight <= 500) return 100;     // Medium
        if (qfontWeight <= 600) return 180;     // SemiBold
        if (qfontWeight <= 700) return 200;     // Bold
        if (qfontWeight <= 800) return 205;     // ExtraBold
        return 210;                              // Black
    }

    // Find a TTF/OTF file for <family> at the given fontconfig weight.
    QString pickFamilyVariant(const QString &family, int fcWeight) const {
        QProcess fc;
        fc.start("fc-list"_L1, {
            ":family="_L1 + family + ":weight="_L1 + QString::number(fcWeight),
            "file"_L1,
        });
        if (!fc.waitForFinished(5000) || fc.exitStatus() != QProcess::NormalExit)
            return {};
        const auto lines = QString::fromLocal8Bit(fc.readAllStandardOutput()).split(u'\n', Qt::SkipEmptyParts);
        for (const auto &line : lines) {
            QString path = line.trimmed();
            if (path.endsWith(u':'))
                path.chop(1);
            const auto suffix = QFileInfo(path).suffix().toLower();
            if (suffix == "ttf"_L1 || suffix == "otf"_L1)
                return path;
        }
        return {};
    }

    // Walk the model's text layers and return fontconfig `lang` codes for the
    // scripts present, in coverage-descending order.
    QStringList detectFontLangs() const {
        QHash<QString, int> counts; // lang -> char count
        const QPsdExporterTreeItemModel *m = model();
        if (!m)
            return {};
        std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &idx) {
            if (idx.isValid()) {
                const auto *item = m->layerItem(idx);
                if (const auto *txt = dynamic_cast<const QPsdTextLayerItem *>(item)) {
                    for (const auto &run : txt->runs()) {
                        for (QChar ch : run.text) {
                            const auto code = ch.unicode();
                            QString lang;
                            if (code >= 0x3040 && code <= 0x309F) lang = "ja"_L1;     // Hiragana
                            else if (code >= 0x30A0 && code <= 0x30FF) lang = "ja"_L1; // Katakana
                            else if (code >= 0xAC00 && code <= 0xD7AF) lang = "ko"_L1; // Hangul
                            else if (code >= 0x0E00 && code <= 0x0E7F) lang = "th"_L1; // Thai
                            else if (code >= 0x0590 && code <= 0x05FF) lang = "he"_L1; // Hebrew
                            else if (code >= 0x0600 && code <= 0x06FF) lang = "ar"_L1; // Arabic
                            else if (code >= 0x0900 && code <= 0x097F) lang = "hi"_L1; // Devanagari
                            else if (code >= 0x4E00 && code <= 0x9FFF) lang = "zh"_L1; // Han (default zh, may overlap ja)
                            if (!lang.isEmpty())
                                counts[lang]++;
                        }
                    }
                }
            }
            for (int r = 0; r < m->rowCount(idx); ++r)
                walk(m->index(r, 0, idx));
        };
        walk(QModelIndex {});

        // If Han chars but no kana detected, treat as zh; if kana present, treat
        // Han as ja (since they share glyphs in Japanese fonts).
        if (counts.contains("ja"_L1) && counts.contains("zh"_L1)) {
            counts["ja"_L1] += counts.take("zh"_L1);
        }

        auto keys = counts.keys();
        std::sort(keys.begin(), keys.end(),
                  [&](const QString &a, const QString &b) { return counts[a] > counts[b]; });
        return keys;
    }

    // fc-list :lang=<lang> filtered to single-file TTF/OTF entries. Prefers
    // Regular weight (fontconfig scale: 80 = Regular, 200 = Bold) so body
    // text isn't rendered in the first bold/black variant fontconfig returns.
    // Falls back to any weight if no Regular candidate exists.
    QString pickFontForLang(const QString &lang) const {
        auto runFc = [](const QString &spec) -> QStringList {
            QProcess fc;
            fc.start("fc-list"_L1, { spec, "file"_L1 });
            if (!fc.waitForFinished(5000) || fc.exitStatus() != QProcess::NormalExit)
                return {};
            return QString::fromLocal8Bit(fc.readAllStandardOutput()).split(u'\n', Qt::SkipEmptyParts);
        };
        auto firstSingleFile = [](const QStringList &lines) -> QString {
            for (const auto &line : lines) {
                QString path = line.trimmed();
                if (path.endsWith(u':'))
                    path.chop(1);
                const auto suffix = QFileInfo(path).suffix().toLower();
                if (suffix == "ttf"_L1 || suffix == "otf"_L1)
                    return path;
            }
            return {};
        };

        if (auto p = firstSingleFile(runFc(":lang="_L1 + lang + ":weight=80"_L1));
                !p.isEmpty()) {
            return p;
        }
        return firstSingleFile(runFc(":lang="_L1 + lang));
    }

    void onAfterExport(const ExportConfig & /*config*/) const override {
        // Generate the .qmlproject companion file so the exported directory
        // can be opened directly with the Qt for MCUs tooling.
        QFile project(dir.absoluteFilePath("MainWindow.qmlproject"_L1));
        if (!project.open(QIODevice::WriteOnly | QIODevice::Text))
            return;
        QTextStream out(&project);
        writeLicenseHeader(out);
        out << "import QmlProject 1.3\n\n";
        out << "Project {\n";
        out << "    mainFile: \"MainWindow.ui.qml\"\n";
        out << "    QmlFiles {\n";
        out << "        files: [\n";
        const QStringList qmlFiles = dir.entryList(QStringList() << "*.qml"_L1, QDir::Files, QDir::Name);
        for (qsizetype i = 0; i < qmlFiles.size(); ++i) {
            out << "            \"" << qmlFiles.at(i) << "\"";
            if (i + 1 < qmlFiles.size())
                out << ",";
            out << "\n";
        }
        out << "        ]\n";
        out << "    }\n";
        out << "    ImageFiles {\n";
        out << "        directory: \"images\"\n";
        out << "    }\n";
        if (!m_bundledFontFiles.isEmpty()) {
            out << "    FontFiles {\n";
            out << "        files: [\n";
            for (qsizetype i = 0; i < m_bundledFontFiles.size(); ++i) {
                out << "            \"" << m_bundledFontFiles.at(i) << "\"";
                if (i + 1 < m_bundledFontFiles.size())
                    out << ",";
                out << "\n";
            }
            out << "        ]\n";
            out << "    }\n";
            out << "    MCU.Config {\n";
            out << "        defaultFontFamily: \"" << m_bundledFontFamily << "\"\n";
            out << "    }\n";
        }
        if (m_usedControls) {
            out << "    ModuleFiles {\n";
            out << "        MCU.qulModules: [ \"Controls\" ]\n";
            out << "    }\n";
        }
        out << "}\n";
    }

    // MCU has no Rectangle.border group property and Text.font.family must
    // match a Fonts {}-registered family. The generator skips both at the
    // emission site; MCU.Config.defaultFontFamily covers the default font.
    bool supportsRectangleBorder() const override { return false; }
    bool supportsFontFamily() const override { return false; }

    void adjustComponentRoot(Element &component, const QModelIndex &index) const override {
        // MCU's qmltocpp rejects `anchors.fill: parent` (and other anchors.*)
        // on a component root because the root has no parent until instantiated.
        // Strip them and ensure an explicit size from the layer bounds.
        const QStringList anchorKeys = component.properties.keys().filter(QRegularExpression("^anchors\\."_L1));
        for (const auto &key : anchorKeys)
            component.properties.remove(key);
        if (!component.properties.contains("width"_L1) || !component.properties.contains("height"_L1)) {
            QRect r = computeBaseRect(index);
            if (r.isEmpty())
                r = QRect(QPoint(0, 0), canvasSize());
            component.properties.insert("width", r.width() * horizontalScale);
            component.properties.insert("height", r.height() * verticalScale);
        }
    }

    // Qt for MCUs' QtQuick.Controls Button has no `highlighted` property.
    bool buttonHighlightedSupported() const override { return false; }

    // No ShaderEffect / Qt5Compat: non-Normal blends are already pre-flattened
    // via the merged-image fallback in the base class. Just strip any residual
    // `property string blendMode` annotation so it isn't serialised.
    void applyBlendModes(Element *element, ImportData * /*imports*/) const override {
        for (auto &child : element->children)
            child.properties.remove(u"property string blendMode"_s);
    }

    // No ShaderEffect: adjustment layers cannot be rendered at runtime.
    void applyAdjustmentLayer(const QPsdAbstractLayerItem * /*item*/, Element * /*parent*/, ImportData * /*imports*/) const override {
    }

private:
    mutable QStringList m_bundledFontFiles;
    mutable QString m_bundledFontFamily;
};

QT_END_NAMESPACE

#include "qtformcu.moc"
