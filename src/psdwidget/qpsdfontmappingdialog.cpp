// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdfontmappingdialog.h"

#include <QtPsdGui/QPsdFontMapper>

#include <QtCore/QHash>
#include <QtGui/QFontDatabase>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFontComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

using namespace Qt::Literals::StringLiterals;

class QPsdFontMappingDialog::Private
{
public:
    Private(QPsdFontMappingDialog *parent, const QString &psdPath);

    void setupUi();
    void populateFontList();

    QPsdFontMappingDialog *q;
    QString psdPath;
    QStringList fontsUsed;

    // UI elements
    QVBoxLayout *mainLayout = nullptr;
    QScrollArea *scrollArea = nullptr;
    QWidget *scrollWidget = nullptr;
    QGridLayout *fontGrid = nullptr;
    QDialogButtonBox *buttonBox = nullptr;

    // Font mapping rows: font name -> (combobox, global checkbox)
    struct FontRow {
        QString originalName;
        QFontComboBox *fontCombo = nullptr;
        QCheckBox *globalCheck = nullptr;
    };
    QList<FontRow> fontRows;
};

QPsdFontMappingDialog::Private::Private(QPsdFontMappingDialog *parent, const QString &psdPath)
    : q(parent)
    , psdPath(psdPath)
{
    setupUi();
}

void QPsdFontMappingDialog::Private::setupUi()
{
    q->setWindowTitle(QObject::tr("Font Mapping"));
    q->setMinimumSize(500, 300);
    q->resize(600, 400);

    mainLayout = new QVBoxLayout(q);

    // Info label
    auto *infoLabel = new QLabel(QObject::tr(
        "Map fonts from the PSD file to fonts available on your system.\n"
        "Check 'Global' to apply the mapping to all PSD files."), q);
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    // Scroll area for font list
    scrollArea = new QScrollArea(q);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    scrollWidget = new QWidget();
    fontGrid = new QGridLayout(scrollWidget);
    fontGrid->setColumnStretch(1, 1); // Font combo gets stretch

    // Header row
    auto *headerPsd = new QLabel(QObject::tr("PSD Font"), scrollWidget);
    auto *headerSystem = new QLabel(QObject::tr("System Font"), scrollWidget);
    auto *headerGlobal = new QLabel(QObject::tr("Global"), scrollWidget);

    headerPsd->setStyleSheet("font-weight: bold;"_L1);
    headerSystem->setStyleSheet("font-weight: bold;"_L1);
    headerGlobal->setStyleSheet("font-weight: bold;"_L1);

    fontGrid->addWidget(headerPsd, 0, 0);
    fontGrid->addWidget(headerSystem, 0, 1);
    fontGrid->addWidget(headerGlobal, 0, 2);

    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    // Button box
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, q);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, q, &QPsdFontMappingDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, q, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void QPsdFontMappingDialog::Private::populateFontList()
{
    // Clear existing rows
    for (auto &row : fontRows) {
        delete row.fontCombo;
        delete row.globalCheck;
    }
    fontRows.clear();

    // Remove all items except header (row 0)
    while (fontGrid->count() > 3) {
        auto *item = fontGrid->takeAt(3);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    auto *fontMapper = QPsdFontMapper::instance();
    const auto globalMappings = fontMapper->globalMappings();
    const auto contextMappings = fontMapper->contextMappings(psdPath);

    int row = 1;
    for (const QString &fontName : fontsUsed) {
        FontRow fontRow;
        fontRow.originalName = fontName;

        // Label for PSD font name
        auto *psdFontLabel = new QLabel(fontName, scrollWidget);
        psdFontLabel->setToolTip(fontName);

        // Font combo box
        fontRow.fontCombo = new QFontComboBox(scrollWidget);
        fontRow.fontCombo->setEditable(true);

        // Determine current mapping and set the combo
        QString currentMapping;
        bool isGlobal = false;

        if (contextMappings.contains(fontName)) {
            currentMapping = contextMappings.value(fontName);
            isGlobal = false;
        } else if (globalMappings.contains(fontName)) {
            currentMapping = globalMappings.value(fontName);
            isGlobal = true;
        } else {
            // Use auto-resolved font
            QFont resolved = fontMapper->resolveFont(fontName, psdPath);
            currentMapping = resolved.family();
            isGlobal = false;
        }

        fontRow.fontCombo->setCurrentFont(QFont(currentMapping));

        // Global checkbox
        fontRow.globalCheck = new QCheckBox(scrollWidget);
        fontRow.globalCheck->setChecked(isGlobal);
        fontRow.globalCheck->setToolTip(QObject::tr("Apply to all PSD files"));

        fontGrid->addWidget(psdFontLabel, row, 0);
        fontGrid->addWidget(fontRow.fontCombo, row, 1);
        fontGrid->addWidget(fontRow.globalCheck, row, 2, Qt::AlignCenter);

        fontRows.append(fontRow);
        row++;
    }

    // Add stretch at bottom
    fontGrid->setRowStretch(row, 1);
}

QPsdFontMappingDialog::QPsdFontMappingDialog(const QString &psdPath, QWidget *parent)
    : QDialog(parent)
    , d(new Private(this, psdPath))
{
}

QPsdFontMappingDialog::~QPsdFontMappingDialog() = default;

void QPsdFontMappingDialog::setFontsUsed(const QStringList &fonts)
{
    d->fontsUsed = fonts;
    d->fontsUsed.removeDuplicates();
    d->fontsUsed.sort();
    d->populateFontList();
}

QHash<QString, QString> QPsdFontMappingDialog::mappings() const
{
    QHash<QString, QString> result;
    for (const auto &row : d->fontRows) {
        QString targetFamily = row.fontCombo->currentFont().family();
        result.insert(row.originalName, targetFamily);
    }
    return result;
}

QHash<QString, bool> QPsdFontMappingDialog::globalFlags() const
{
    QHash<QString, bool> result;
    for (const auto &row : d->fontRows) {
        result.insert(row.originalName, row.globalCheck->isChecked());
    }
    return result;
}

void QPsdFontMappingDialog::accept()
{
    auto *fontMapper = QPsdFontMapper::instance();

    // Collect per-PSD mappings
    QHash<QString, QString> perPsdMappings;

    for (const auto &row : d->fontRows) {
        QString targetFamily = row.fontCombo->currentFont().family();
        bool isGlobal = row.globalCheck->isChecked();

        if (isGlobal) {
            // Save to global mappings
            fontMapper->setGlobalMapping(row.originalName, targetFamily);
        } else {
            // Collect for per-PSD mapping
            perPsdMappings.insert(row.originalName, targetFamily);
            // Remove from global if it was there
            fontMapper->removeGlobalMapping(row.originalName);
        }
    }

    // Set per-PSD mappings
    if (!perPsdMappings.isEmpty()) {
        fontMapper->setContextMappings(d->psdPath, perPsdMappings);
    } else {
        fontMapper->clearContextMappings(d->psdPath);
    }

    // Save global mappings to QSettings
    fontMapper->saveGlobalMappings();

    // Clear cache to force re-resolution
    fontMapper->clearCache();

    QDialog::accept();
}

QT_END_NAMESPACE
