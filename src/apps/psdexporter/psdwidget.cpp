// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "psdwidget.h"
#include "ui_psdwidget.h"

#include "psdtreeitemmodel.h"
#include "exportdialog.h"

#include <QtPsdGui/QPsdTextLayerItem>
#include <QtPsdWidget/QPsdFontMappingDialog>
#include <QtPsdImporter/QPsdImporterPlugin>

#include <QtCore/QCryptographicHash>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>

#include <QtGui/QClipboard>
#include <QtGui/QDesktopServices>
#include <QtGui/QPainter>

#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QStyledItemDelegate>

class NameDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (auto *lineEdit = qobject_cast<QLineEdit *>(editor)) {
            QString placeholder = index.data(PsdTreeItemModel::PlaceholderTextRole).toString();
            lineEdit->setPlaceholderText(placeholder);
        }
        return editor;
    }
};

class CenteredCheckBoxDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // Draw background
        opt.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

        // Draw centered checkbox
        QStyleOptionButton checkBoxOpt;
        checkBoxOpt.state = QStyle::State_Enabled;
        if (index.data(Qt::CheckStateRole).toInt() == Qt::Checked)
            checkBoxOpt.state |= QStyle::State_On;
        else
            checkBoxOpt.state |= QStyle::State_Off;

        QRect checkBoxRect = opt.widget->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &checkBoxOpt, opt.widget);
        checkBoxOpt.rect = QStyle::alignedRect(opt.direction, Qt::AlignCenter, checkBoxRect.size(), opt.rect);

        opt.widget->style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &checkBoxOpt, painter, opt.widget);
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseButtonDblClick) {
            const Qt::CheckState currentState = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
            const Qt::CheckState newState = (currentState == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
            return model->setData(index, newState, Qt::CheckStateRole);
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }
};

class PsdWidget::Private : public Ui::PsdWidget
{
public:
    Private(::PsdWidget *parent);

    void updateAttributes();
    void applyAttributes();
    void populateTextSourceCombo();

    QPsdWidgetTreeItemModel widgetModel;
    PsdTreeItemModel model;

    QString errorMessage;
    QSettings settings;
    QString windowTitle;
    QComboBox *textSourceCombo = nullptr;

private:
    ::PsdWidget *q;
    QButtonGroup types;
};

PsdWidget::Private::Private(::PsdWidget *parent)
    : q(parent)
{
    setupUi(q);
    model.setSourceModel(&widgetModel);
    treeView->setModel(&model);
    treeView->setItemDelegateForColumn(PsdTreeItemModel::Name, new NameDelegate(treeView));
    treeView->setItemDelegateForColumn(PsdTreeItemModel::Use, new CenteredCheckBoxDelegate(treeView));
    treeView->setItemDelegateForColumn(PsdTreeItemModel::Visible, new CenteredCheckBoxDelegate(treeView));

    // Configure Use and Visible columns: fixed small size, non-resizable
    auto *header = treeView->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(PsdTreeItemModel::Name, QHeaderView::Stretch);
    header->setSectionResizeMode(PsdTreeItemModel::Use, QHeaderView::Fixed);
    header->resizeSection(PsdTreeItemModel::Use, 30);
    header->setSectionResizeMode(PsdTreeItemModel::Visible, QHeaderView::Fixed);
    header->resizeSection(PsdTreeItemModel::Visible, 30);

    connect(&model, &PsdTreeItemModel::dataChanged, q, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
        const auto *model = dynamic_cast<const PsdTreeItemModel *>(topLeft.model());
        if (model) {
            for (int r = topLeft.row(); r <= bottomRight.row(); r++) {
                for (int c = topLeft.column(); c <= bottomRight.column(); c++) {
                    QModelIndex index = topLeft.sibling(r, c);
                    if (roles.empty() || roles.contains(Qt::CheckStateRole)) {
                        if (c == PsdTreeItemModel::Column::Visible) {
                            psdView->setItemVisible(model->layerId(index), model->isVisible(index));
                        } else {
                            q->setWindowModified(true);
                            q->setWindowTitle(windowTitle);
                        }
                    }
                }
            }
        }
    });

    connect(treeView, &QTreeView::expanded, q, [this](const QModelIndex &index) {
        const auto *model = dynamic_cast<const PsdTreeItemModel *>(index.model());
        if (model) {
            const auto lyid = model->layerId(index);
            settings.setValue(u"%1-x"_s.arg(lyid), true);
        }
    });

    connect(treeView, &QTreeView::collapsed, q, [this](const QModelIndex &index) {
        const auto *model = dynamic_cast<const PsdTreeItemModel *>(index.model());
        if (model) {
            const auto lyid = model->layerId(index);
            settings.setValue(u"%1-x"_s.arg(lyid), false);
        }
    });

    connect(treeView->selectionModel(), &QItemSelectionModel::selectionChanged, q, [this]() {
        updateAttributes();
        const auto rows = treeView->selectionModel()->selectedRows();
        if (rows.size() == 1) {
            // Select single item in view and scroll to it
            QModelIndex sourceIndex = model.mapToSource(rows.first());
            psdView->selectItem(sourceIndex);
        } else {
            psdView->clearSelection();
        }
    });

    auto expandToIndex = [this](const QModelIndex &sourceIndex) {
        QModelIndex index = model.mapFromSource(sourceIndex);
        QModelIndex parent = index.parent();
        while (parent.isValid()) {
            treeView->expand(parent);
            parent = parent.parent();
        }
        treeView->scrollTo(index);
    };

    connect(psdView, &QPsdView::itemSelected, q, [this, expandToIndex](const QModelIndex &index) {
        expandToIndex(index);
        QModelIndex proxyIndex = model.mapFromSource(index);
        treeView->selectionModel()->select(proxyIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    });

    connect(psdView, &QPsdView::itemsSelected, q, [this, expandToIndex](const QModelIndexList &indexes) {
        treeView->selectionModel()->clearSelection();
        for (const auto &index : indexes) {
            expandToIndex(index);
            QModelIndex proxyIndex = model.mapFromSource(index);
            treeView->selectionModel()->select(proxyIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    });

    connect(&model, &PsdTreeItemModel::fileInfoChanged, q, [this](const QFileInfo &fileInfo) {
        windowTitle = fileInfo.fileName();
        q->setWindowTitle(windowTitle);
    });

    connect(psdView, &QPsdView::scaleChanged, q, &::PsdWidget::viewScaleChanged);

    updateAttributes();
    settings.beginGroup("Files");

    // Make fontValue label clickable to open font mapping dialog
    fontValue->setCursor(Qt::PointingHandCursor);
    fontValue->installEventFilter(q);

    for (auto *radioButton : type->findChildren<QRadioButton *>()) {
        types.addButton(radioButton);
        connect(radioButton, &QRadioButton::clicked, q, [this](bool checked) {
            if (types.exclusive()) return;
            if (!checked) return;
            types.setExclusive(true);
            auto *radioButton = qobject_cast<QRadioButton *>(q->sender());
            radioButton->setChecked(true);
        });
    }
    types.setId(typeEmbed, QPsdExporterTreeItemModel::ExportHint::Embed);
    types.setId(typeComponent, QPsdExporterTreeItemModel::ExportHint::Component);
    types.setId(typeNative, QPsdExporterTreeItemModel::ExportHint::Native);
    types.setId(typeSkip, QPsdExporterTreeItemModel::ExportHint::Skip);

#define ADDITEM(x) \
    customBase->addItem(QPsdExporterTreeItemModel::ExportHint::nativeCode2Name(QPsdExporterTreeItemModel::ExportHint::x), QPsdExporterTreeItemModel::ExportHint::NativeComponent::x); \
        nativeBase->addItem(QPsdExporterTreeItemModel::ExportHint::nativeCode2Name(QPsdExporterTreeItemModel::ExportHint::x), QPsdExporterTreeItemModel::ExportHint::NativeComponent::x)

    ADDITEM(Container);
    ADDITEM(Button);
    ADDITEM(Button_Highlighted);
#undef ADDITEM

    auto isReset = [&]() -> bool {
        return !buttonBox->button(QDialogButtonBox::Discard)->isEnabled() && !buttonBox->button(QDialogButtonBox::Apply)->isEnabled();
    };
    auto changed = [&]() {
        buttonBox->button(QDialogButtonBox::Discard)->setEnabled(true);
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        treeView->setEnabled(false);
    };
    connect(typeEmbed, &QRadioButton::toggled, q, [=](bool checked) {
        embedWithTouch->setEnabled(checked);
        if (isReset())
            changed();
    });
    embedWithTouch->setEnabled(typeEmbed->isChecked());
    connect(embedWithTouch, &QCheckBox::checkStateChanged, q, [=]() {
        if (isReset())
            changed();
    });

    connect(typeComponent, &QRadioButton::toggled, q, [=](bool checked) {
        customEnabled->setEnabled(checked);
        custom->setEnabled(checked && customEnabled->isChecked());
        customBase->setEnabled(checked);
        if (isReset())
            changed();
    });
    connect(customEnabled, &QCheckBox::toggled, q, [=](bool checked) {
        custom->setEnabled(checked);
        if (isReset())
            changed();
    });

    custom->setEnabled(typeComponent->isChecked());
    connect(custom, &QLineEdit::textChanged, q, [=]() {
        if (isReset())
            changed();
    });
    customBase->setEnabled(typeComponent->isChecked());
    connect(customBase, &QComboBox::currentIndexChanged, q, [=]() {
        if (isReset())
            changed();
    });

    connect(typeNative, &QRadioButton::toggled, q, [=](bool checked) {
        nativeBase->setEnabled(checked);
        if (isReset())
            changed();
    });
    nativeBase->setEnabled(typeNative->isChecked());
    connect(nativeBase, &QComboBox::currentIndexChanged, q, [=]() {
        if (isReset())
            changed();
    });

    // Text source combo for Button native elements
    textSourceCombo = new QComboBox(q);
    textSourceCombo->hide();
    auto *propertiesGrid = qobject_cast<QGridLayout *>(properties->layout());
    propertiesGrid->addWidget(textSourceCombo, 4, 1);
    connect(textSourceCombo, &QComboBox::currentIndexChanged, q, [=]() {
        if (isReset())
            changed();
    });

    auto updateTextSourceVisibility = [this]() {
        const bool isNative = typeNative->isChecked();
        const auto base = static_cast<QPsdExporterTreeItemModel::ExportHint::NativeComponent>(
            nativeBase->currentData().toInt());
        const bool isButton = isNative
            && (base == QPsdExporterTreeItemModel::ExportHint::Button
                || base == QPsdExporterTreeItemModel::ExportHint::Button_Highlighted);
        if (isButton) {
            populateTextSourceCombo();
            textSourceCombo->show();
            textValue->hide();
            text->setEnabled(true);
        } else {
            textSourceCombo->hide();
            textValue->show();
        }
    };
    connect(typeNative, &QRadioButton::toggled, q, [=]() { updateTextSourceVisibility(); });
    connect(nativeBase, &QComboBox::currentIndexChanged, q, [=]() { updateTextSourceVisibility(); });

    const auto propertyCheckBoxes = properties->findChildren<QCheckBox *>();
    for (auto *checkBox : propertyCheckBoxes) {
        connect(checkBox, &QCheckBox::toggled, q, [=](bool checked) {
            if (isReset())
                changed();
        });
    }
    connect(buttonBox, &QDialogButtonBox::clicked, q, [this](QAbstractButton *button) {
        switch (buttonBox->buttonRole(button)) {
        case QDialogButtonBox::ApplyRole:
            applyAttributes();
            break;
        case QDialogButtonBox::DestructiveRole: // Discard
            updateAttributes();
            break;
        default:
            qWarning() << buttonBox->buttonRole(button) << "not found";
        }
        treeView->setEnabled(true);
    });
}

template <typename T>
class UniqueOrNot
{
public:
    UniqueOrNot(T invalidValue = T())
        : uniqueValue(T())
        , invalidValue(invalidValue)
    {}

    void add(const T &v)
    {
        if (!unique) {
        } if (first) {
            uniqueValue = v;
            first = false;
        } else if (uniqueValue != v) {
            unique = false;
            uniqueValue = invalidValue;
        }
    }

    bool isUnique() const { return unique; }
    T value() const { return uniqueValue; }

private:
    bool first = true;
    bool unique = true;
    T uniqueValue;
    T invalidValue;
};

void PsdWidget::Private::updateAttributes()
{
    const auto rows = treeView->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        attributes->setEnabled(false);
        emit q->selectionInfoChanged(QString());
        // Clear all value labels
        visibleValue->clear();
        positionValue->clear();
        sizeValue->clear();
        colorValue->clear();
        textValue->clear();
        fontValue->clear();
        imageValue->clear();
        return;
    }

    // Emit selection info for status bar and update property value labels
    if (rows.size() == 1) {
        const auto *item = model.layerItem(rows.first());
        if (item) {
            const auto rect = item->rect();
            emit q->selectionInfoChanged(QObject::tr("Position: %1, %2  Size: %3 × %4")
                .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));

            // Update property value labels
            visibleValue->setText(item->isVisible() ? QObject::tr("Yes") : QObject::tr("No"));
            positionValue->setText(QStringLiteral("%1, %2").arg(rect.x()).arg(rect.y()));
            sizeValue->setText(QStringLiteral("%1 × %2").arg(rect.width()).arg(rect.height()));

            // Color value
            const auto color = item->color();
            if (color.isValid()) {
                colorValue->setText(color.name(QColor::HexRgb));
            } else {
                colorValue->clear();
            }

            // Text and Font values (for text layers)
            if (item->type() == QPsdAbstractLayerItem::Text) {
                const auto *textItem = dynamic_cast<const QPsdTextLayerItem *>(item);
                if (textItem) {
                    const auto runs = textItem->runs();
                    // Text value
                    QStringList texts;
                    for (const auto &run : runs) {
                        texts.append(run.text);
                    }
                    QString fullText = texts.join(QString());
                    // Truncate if too long
                    if (fullText.length() > 20) {
                        fullText = fullText.left(17) + QStringLiteral("...");
                    }
                    fullText.replace(u'\n', u' ');
                    textValue->setText(fullText);

                    // Font value - show first run's font
                    if (!runs.isEmpty()) {
                        const auto &font = runs.first().font;
                        fontValue->setText(QStringLiteral("%1 %2pt").arg(font.family()).arg(font.pointSizeF(), 0, 'f', 1));
                    } else {
                        fontValue->clear();
                    }
                } else {
                    textValue->clear();
                    fontValue->clear();
                }
            } else {
                textValue->clear();
                fontValue->clear();
            }

            // Image value (for image layers)
            if (item->type() == QPsdAbstractLayerItem::Image) {
                const auto img = item->image();
                if (!img.isNull()) {
                    // Scale image to fit in label (max 48px height)
                    QPixmap pixmap = QPixmap::fromImage(img);
                    if (pixmap.height() > 48) {
                        pixmap = pixmap.scaledToHeight(48, Qt::SmoothTransformation);
                    }
                    imageValue->setPixmap(pixmap);
                } else {
                    imageValue->clear();
                }
            } else {
                imageValue->clear();
            }
        }
    } else {
        emit q->selectionInfoChanged(QObject::tr("%1 items selected").arg(rows.size()));

        // Check if all selected items have the same values
        UniqueOrNot<bool> visibleUnique;
        UniqueOrNot<QPoint> positionUnique;
        UniqueOrNot<QSize> sizeUnique;
        UniqueOrNot<QColor> colorUnique;

        for (const auto &row : rows) {
            const auto *item = model.layerItem(row);
            if (item) {
                visibleUnique.add(item->isVisible());
                positionUnique.add(item->rect().topLeft());
                sizeUnique.add(item->rect().size());
                colorUnique.add(item->color());
            }
        }

        // Display values if unique, otherwise show "(multiple)" or clear
        if (visibleUnique.isUnique()) {
            visibleValue->setText(visibleUnique.value() ? QObject::tr("Yes") : QObject::tr("No"));
        } else {
            visibleValue->setText(QObject::tr("(multiple)"));
        }

        if (positionUnique.isUnique()) {
            positionValue->setText(QStringLiteral("%1, %2").arg(positionUnique.value().x()).arg(positionUnique.value().y()));
        } else {
            positionValue->clear();
        }

        if (sizeUnique.isUnique()) {
            sizeValue->setText(QStringLiteral("%1 × %2").arg(sizeUnique.value().width()).arg(sizeUnique.value().height()));
        } else {
            sizeValue->clear();
        }

        if (colorUnique.isUnique() && colorUnique.value().isValid()) {
            colorValue->setText(colorUnique.value().name(QColor::HexRgb));
        } else {
            colorValue->clear();
        }

        // Text, Font and Image are cleared for multiple selection
        textValue->clear();
        fontValue->clear();
        imageValue->clear();
    }

    attributes->setEnabled(true);

    // enable properties by default
    const auto propertyCheckBoxes = properties->findChildren<QCheckBox *>();
    for (auto *property : propertyCheckBoxes) {
        property->setEnabled(true);
    }
    static const QSet<QString> baseProperties = {
        "visible",
        "position",
        "size",
    };
    static const QHash<QPsdAbstractLayerItem::Type, QSet<QString>> additionalProperties = {
        { QPsdAbstractLayerItem::Text, { "color", "text", "font" } },
        { QPsdAbstractLayerItem::Shape, { "color" } },
        { QPsdAbstractLayerItem::Image, { "image" } }
    };

    UniqueOrNot<QPsdExporterTreeItemModel::ExportHint::Type> itemTypes(static_cast<QPsdExporterTreeItemModel::ExportHint::Type>(-1));
    UniqueOrNot<Qt::CheckState> itemWithTouch(Qt::PartiallyChecked);
    UniqueOrNot<QString> itemComponentName;
    UniqueOrNot<QString> itemCustom;
    UniqueOrNot<QPsdExporterTreeItemModel::ExportHint::NativeComponent> itemCustomBase;
    UniqueOrNot<QString> itemTextSource;
    QHash<QString, UniqueOrNot<Qt::CheckState>> itemProperties;

    // If any Merged layer is in multi-selection, disable editing entirely
    if (rows.size() > 1) {
        for (const auto &row : rows) {
            const auto hint = model.layerHint(row);
            if (hint.type == QPsdExporterTreeItemModel::ExportHint::Merged) {
                type->setEnabled(false);
                properties->setEnabled(false);
                buttonBox->button(QDialogButtonBox::Discard)->setEnabled(false);
                buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
                treeView->setEnabled(true);
                return;
            }
        }
    }

    for (const auto &row : rows) {
        const auto *item = model.layerItem(row);
        const auto hint = model.layerHint(row);
        itemTypes.add(hint.type);
        itemWithTouch.add(hint.interactive ? Qt::Checked : Qt::Unchecked);
        qDebug() << hint.interactive << itemWithTouch.isUnique() << itemWithTouch.value();
        itemComponentName.add(hint.componentName);
        itemCustomBase.add(hint.baseElement);
        itemTextSource.add(hint.textSource);

        for (auto *property : propertyCheckBoxes) {
            const auto name = property->objectName();
            if (baseProperties.contains(name) || additionalProperties.value(item->type()).contains(name)) {
                if (!itemProperties.contains(name)) {
                    itemProperties.insert(name, UniqueOrNot<Qt::CheckState>(Qt::PartiallyChecked));
                }
                itemProperties[name].add(hint.properties.contains(name) ? Qt::Checked : Qt::Unchecked);
            } else {
                property->setChecked(false);
                property->setEnabled(false);
            }
        }
    }

    if (itemTypes.isUnique() && types.button(itemTypes.value())) {
        types.setExclusive(true);
        types.button(itemTypes.value())->setChecked(true);
    } else {
        types.setExclusive(false);
        for (auto *radioButton : type->findChildren<QRadioButton *>()) {
            radioButton->setChecked(false);
        }
    }

    // reset all
    embedWithTouch->setCheckState(Qt::Unchecked);
    customEnabled->setChecked(rows.length() == 1);
    customEnabled->setEnabled(rows.length() > 1);
    custom->setText(QString());
    custom->setEnabled(false);
    customBase->setCurrentIndex(-1);
    customBase->setEnabled(false);
    nativeBase->setCurrentIndex(-1);
    textSourceCombo->clear();
    textSourceCombo->hide();
    textValue->show();

    // Merged layers: grey out type group and properties (not user-selectable)
    if (itemTypes.isUnique() && itemTypes.value() == QPsdExporterTreeItemModel::ExportHint::Merged) {
        type->setEnabled(false);
        properties->setEnabled(false);
    } else {
        type->setEnabled(true);
        properties->setEnabled(true);
    }

    switch (itemTypes.value()) {
    case QPsdExporterTreeItemModel::ExportHint::Embed:
        embedWithTouch->setEnabled(true);
        qDebug() << itemWithTouch.value() << embedWithTouch->checkState();
        embedWithTouch->setCheckState(itemWithTouch.value());
        qDebug() << itemWithTouch.value() << embedWithTouch->checkState();
        break;
    case QPsdExporterTreeItemModel::ExportHint::Merged:
        break;
    case QPsdExporterTreeItemModel::ExportHint::Component:
        customEnabled->setChecked(itemComponentName.isUnique());
        custom->setEnabled(customEnabled->isChecked());
        if (customEnabled->isChecked()) {
            custom->setText(itemComponentName.value());
            if (itemCustomBase.isUnique()) {
                customBase->setCurrentText(QPsdExporterTreeItemModel::ExportHint::nativeCode2Name(itemCustomBase.value()));
            }
        }
        break;
    case QPsdExporterTreeItemModel::ExportHint::Native:
        if (itemCustomBase.isUnique()) {
            nativeBase->setCurrentText(QPsdExporterTreeItemModel::ExportHint::nativeCode2Name(itemCustomBase.value()));
        }
        // Populate textSourceCombo for Button native elements
        if (rows.size() == 1 && itemCustomBase.isUnique()) {
            const auto base = itemCustomBase.value();
            if (base == QPsdExporterTreeItemModel::ExportHint::Button
                || base == QPsdExporterTreeItemModel::ExportHint::Button_Highlighted) {
                populateTextSourceCombo();
                if (itemTextSource.isUnique() && !itemTextSource.value().isEmpty()) {
                    for (int i = 0; i < textSourceCombo->count(); ++i) {
                        if (textSourceCombo->itemData(i).toString() == itemTextSource.value()) {
                            textSourceCombo->setCurrentIndex(i);
                            break;
                        }
                    }
                }
                textSourceCombo->show();
                textValue->hide();
                text->setEnabled(true);
            }
        }
        break;
    case QPsdExporterTreeItemModel::ExportHint::Skip:
        break;
    }

    for (auto *property : propertyCheckBoxes) {
        if (!property->isEnabled()) continue;
        if (itemProperties.contains(property->objectName())) {
            property->setCheckState(itemProperties.value(property->objectName()).value());
        } else {
            property->setCheckState(Qt::Unchecked);
        }
    }

    buttonBox->button(QDialogButtonBox::Discard)->setEnabled(false);
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    treeView->setEnabled(true);
}

void PsdWidget::Private::applyAttributes()
{
    const auto rows = treeView->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        attributes->setEnabled(false);
        return;
    }
    const auto type = types.checkedId();
    const auto withTouch = embedWithTouch->checkState();
    QSet<QString> propertiesChecked;
    for (auto *property : properties->findChildren<QCheckBox *>()) {
        if (property->checkState() == Qt::Checked) {
            propertiesChecked.insert(property->objectName());
        }
    }

    for (const auto &row : rows) {
        auto hint = model.layerHint(row);

        // Skip Merged layers — they are not user-editable
        if (hint.type == QPsdExporterTreeItemModel::ExportHint::Merged)
            continue;

        if (type > -1)
            hint.type = static_cast<QPsdExporterTreeItemModel::ExportHint::Type>(type);
        // Remember old textSource to revert Merged layer if changed
        const QString oldTextSource = hint.textSource;

        switch (type) {
        case QPsdExporterTreeItemModel::ExportHint::Embed:
            hint.type = QPsdExporterTreeItemModel::ExportHint::Embed;
            hint.textSource.clear();
            switch (withTouch) {
            case Qt::Checked:
                hint.interactive = true;
                break;
            case Qt::Unchecked:
                hint.interactive = false;
                break;
            default:
                break;
            }
            break;
        case QPsdExporterTreeItemModel::ExportHint::Component:
            hint.type = QPsdExporterTreeItemModel::ExportHint::Component;
            hint.textSource.clear();
            if (customEnabled->isEnabled()) {
                hint.componentName = custom->text();
                hint.baseElement = QPsdExporterTreeItemModel::ExportHint::nativeName2Code(customBase->currentText());
            }
            break;
        case QPsdExporterTreeItemModel::ExportHint::Native: {
            hint.type = QPsdExporterTreeItemModel::ExportHint::Native;
            hint.baseElement = QPsdExporterTreeItemModel::ExportHint::nativeName2Code(nativeBase->currentText());
            const bool isButton = (hint.baseElement == QPsdExporterTreeItemModel::ExportHint::Button
                                   || hint.baseElement == QPsdExporterTreeItemModel::ExportHint::Button_Highlighted);
            if (isButton)
                hint.textSource = textSourceCombo->currentData().toString();
            else
                hint.textSource.clear();
            break; }
        case QPsdExporterTreeItemModel::ExportHint::Skip:
            hint.type = QPsdExporterTreeItemModel::ExportHint::Skip;
            hint.textSource.clear();
            break;
        default:
            break;
        }

        hint.properties = propertiesChecked;
        model.setLayerHint(row, hint);

        // Auto-set/revert Merged type on text source layers
        if (oldTextSource != hint.textSource) {
            const QModelIndex parentIndex = row.parent();
            // Revert old text source layer from Merged to Embed
            if (!oldTextSource.isEmpty()) {
                for (int si = 0; si < model.rowCount(parentIndex); ++si) {
                    const QModelIndex gi = model.index(si, 0, parentIndex);
                    if (model.layerName(gi) == oldTextSource) {
                        auto srcHint = model.layerHint(gi);
                        if (srcHint.type == QPsdExporterTreeItemModel::ExportHint::Merged) {
                            srcHint.type = QPsdExporterTreeItemModel::ExportHint::Embed;
                            model.setLayerHint(gi, srcHint);
                        }
                    }
                }
            }
            // Set new text source layer to Merged
            if (!hint.textSource.isEmpty()) {
                for (int si = 0; si < model.rowCount(parentIndex); ++si) {
                    const QModelIndex gi = model.index(si, 0, parentIndex);
                    if (model.layerName(gi) == hint.textSource) {
                        auto srcHint = model.layerHint(gi);
                        srcHint.type = QPsdExporterTreeItemModel::ExportHint::Merged;
                        model.setLayerHint(gi, srcHint);
                    }
                }
            }
        }
    }
}

void PsdWidget::Private::populateTextSourceCombo()
{
    const QString oldSelection = textSourceCombo->currentData().toString();
    textSourceCombo->clear();
    textSourceCombo->addItem(QString()); // empty = no text source

    const auto rows = treeView->selectionModel()->selectedRows();
    if (rows.size() != 1)
        return;

    const QModelIndex parentIndex = rows.first().parent();
    for (int i = 0; i < model.rowCount(parentIndex); ++i) {
        const QModelIndex gi = model.index(i, 0, parentIndex);
        if (gi == rows.first())
            continue;
        const auto *layerItem = model.layerItem(gi);
        if (!layerItem || layerItem->type() != QPsdAbstractLayerItem::Text)
            continue;
        const auto *textItem = dynamic_cast<const QPsdTextLayerItem *>(layerItem);
        QString displayText;
        if (textItem) {
            QStringList texts;
            for (const auto &run : textItem->runs())
                texts.append(run.text);
            displayText = texts.join(QString()).trimmed();
            if (displayText.length() > 30)
                displayText = displayText.left(27) + "..."_L1;
            displayText.replace(u'\n', u' ');
        }
        const QString layerName = model.layerName(gi);
        const QString label = displayText.isEmpty() ? layerName : displayText;
        textSourceCombo->addItem(label, layerName);
        textSourceCombo->setItemData(textSourceCombo->count() - 1, layerName, Qt::ToolTipRole);
    }

    // Restore previous selection if possible
    if (!oldSelection.isEmpty()) {
        for (int i = 0; i < textSourceCombo->count(); ++i) {
            if (textSourceCombo->itemData(i).toString() == oldSelection) {
                textSourceCombo->setCurrentIndex(i);
                return;
            }
        }
    }
}

PsdWidget::PsdWidget(QWidget *parent)
    : QSplitter(parent)
    , d(new Private(this))
{}

bool PsdWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == d->fontValue && event->type() == QEvent::MouseButtonRelease) {
        showFontMappingDialog();
        return true;
    }
    return QSplitter::eventFilter(watched, event);
}

PsdWidget::~PsdWidget()
{
    d->settings.setValue("splitterState", saveState());
    d->settings.setValue("treeState", d->treeView->header()->saveState());
    d->settings.setValue("viewScale", viewScale());
}

void PsdWidget::load(const QString &fileName)
{
    d->model.load(fileName);
    d->treeView->reset();

    d->settings.beginGroup(QCryptographicHash::hash(fileName.toUtf8(), QCryptographicHash::Md5));
    restoreState(d->settings.value("splitterState").toByteArray());
    d->treeView->header()->restoreState(d->settings.value("treeState").toByteArray());

    // Ensure Use and Visible columns stay fixed after restoring state
    auto *header = d->treeView->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(PsdTreeItemModel::Name, QHeaderView::Stretch);
    header->setSectionResizeMode(PsdTreeItemModel::Use, QHeaderView::Fixed);
    header->resizeSection(PsdTreeItemModel::Use, 30);
    header->setSectionResizeMode(PsdTreeItemModel::Visible, QHeaderView::Fixed);
    header->resizeSection(PsdTreeItemModel::Visible, 30);

    std::function<void(const QModelIndex &index)> traverseTreeView;
    traverseTreeView = [&](const QModelIndex &index) {
        if (d->model.hasChildren(index)) {
            const auto lyid = d->model.layerId(index);
            d->treeView->setExpanded(index, d->settings.value(u"%1-x"_s.arg(lyid), false).toBool());

            for (int row = 0; row < d->model.rowCount(index); row++) {
                traverseTreeView(d->model.index(row, 0, index));
            }
        }
    };
    traverseTreeView(d->treeView->rootIndex());

    d->psdView->setModel(d->model.widgetModel());

    // Restore view scale (default to 1.0 = 100%)
    qreal scale = d->settings.value("viewScale", 1.0).toDouble();
    setViewScale(scale);
}

void PsdWidget::reload()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    save();
    d->settings.endGroup();
    load(d->model.fileName());
    d->treeView->reset();
    QApplication::restoreOverrideCursor();
}

void PsdWidget::save()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    d->model.save();
    QApplication::restoreOverrideCursor();

    setWindowModified(false);
    setWindowTitle(d->windowTitle);
}

void PsdWidget::copyViewToClipboard()
{
    auto *scene = d->psdView->scene();
    if (!scene)
        return;

    QRectF sceneRect = scene->sceneRect();
    QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    scene->render(&painter);
    painter.end();

    QApplication::clipboard()->setImage(image);
}

void PsdWidget::exportTo(QPsdExporterPlugin *exporter, QSettings *settings)
{
    QString to;
    QVariantMap hint;
    switch (exporter->exportType()) {
    case QPsdExporterPlugin::File: {
        settings->beginGroup("Menu");
        settings->beginGroup(exporter->name());
        QString dir = settings->value("ExportDir").toString();
        settings->endGroup();
        settings->endGroup();
        QString selected;
        to = QFileDialog::getSaveFileName(this, tr("Export as %1").arg(exporter->name().remove("&")), dir, exporter->filters().keys().join(";;"_L1), &selected, QFileDialog::DontConfirmOverwrite);
        if (to.isEmpty())
            return;
        QString suffix = exporter->filters().value(selected);
        if (!suffix.isEmpty() && !to.endsWith(suffix, Qt::CaseInsensitive))
            to += suffix;
        QFileInfo fi(to);
        if (fi.exists()) {
            const auto ret = QMessageBox::question(this, tr("Confirm Overwrite"), tr("The file %1 already exists. Do you want to overwrite it?").arg(fi.fileName()), QMessageBox::Yes | QMessageBox::No);
            if (ret != QMessageBox::Yes)
                return;
        }
        settings->beginGroup("Menu");
        settings->beginGroup(exporter->name());
        settings->setValue("ExportDir", QFileInfo(to).absoluteDir().absolutePath());
        settings->endGroup();
        settings->endGroup();
        break; }
    case QPsdExporterPlugin::Directory: {
        ExportDialog dialog(exporter, d->model.size(), exportHint(exporter->key()), this);
        const auto ret = dialog.exec();
        if (ret != QDialog::Accepted)
            return;
        to = dialog.directory();
        auto config = dialog.exportConfig();
        QVariantMap hintForStorage = config.toVariantMap();
        hintForStorage.insert("resolutionIndex"_L1, dialog.resolutionIndex());
        hintForStorage.insert("width"_L1, dialog.width());
        hintForStorage.insert("height"_L1, dialog.height());
        hint = hintForStorage;
        break; }
    }

    updateExportHint(exporter->key(), hint);
    save();

    auto config = QPsdExporterPlugin::ExportConfig::fromVariantMap(hint);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = exporter->exportTo(&d->model, to, config);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::critical(this, tr("Export Failed"),
            tr("Failed to export to %1").arg(QFileInfo(to).fileName()));
        return;
    }

    if (exporter->exportType() == QPsdExporterPlugin::File) {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setWindowTitle(tr("Export Complete"));
        msgBox.setText(tr("Exported successfully to:\n%1").arg(to));

        QPushButton *saveFigmaPluginBtn = nullptr;
        if (exporter->key() == "figma") {
            saveFigmaPluginBtn = msgBox.addButton(tr("Save Figma Plugin..."), QMessageBox::ActionRole);
        }

        auto *copyPathBtn = msgBox.addButton(tr("Copy Path"), QMessageBox::ActionRole);
        auto *showInFolderBtn = msgBox.addButton(tr("Show in Folder"), QMessageBox::ActionRole);
        msgBox.addButton(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();

        if (msgBox.clickedButton() == saveFigmaPluginBtn) {
            saveFigmaPlugin(this);
        } else if (msgBox.clickedButton() == copyPathBtn) {
            QApplication::clipboard()->setText(to);
        } else if (msgBox.clickedButton() == showInFolderBtn) {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(QFileInfo(to).absolutePath()));
        }
    }
}

bool PsdWidget::importFrom(QPsdImporterPlugin *importer, const QVariantMap options)
{
    const bool ok = importer->importFrom(&d->model, options);

    if (ok) {
        d->treeView->reset();
        d->psdView->setModel(d->model.widgetModel());

        d->windowTitle = d->model.fileName();
        setWindowTitle(d->windowTitle);
        setWindowIcon(importer->icon());
    }

    return ok;
}

QString PsdWidget::fileName() const
{
    return d->model.fileName();
}

QString PsdWidget::errorMessage() const
{
    return d->model.errorMessage();
}

QVariantMap PsdWidget::exportHint(const QString& exporterKey) const
{
    return d->model.exportHint(exporterKey);
}

void PsdWidget::updateExportHint(const QString &exporterKey, const QVariantMap &hint)
{
    d->model.updateExportHint(exporterKey, hint);
}

void PsdWidget::setErrorMessage(const QString &errorMessage)
{
    if (d->errorMessage == errorMessage) return;
    d->errorMessage = errorMessage;
    emit errorOccurred(errorMessage);
}

qreal PsdWidget::viewScale() const
{
    return d->psdView->transform().m11();
}

void PsdWidget::setViewScale(qreal scale)
{
    d->psdView->setTransform(QTransform::fromScale(scale, scale));
    emit viewScaleChanged(scale);
}

void PsdWidget::fitToView()
{
    auto *scene = d->psdView->scene();
    if (!scene)
        return;

    QRectF sceneRect = scene->sceneRect();
    QRectF viewRect = d->psdView->viewport()->rect();

    if (sceneRect.isEmpty() || viewRect.isEmpty())
        return;

    qreal scaleX = viewRect.width() / sceneRect.width();
    qreal scaleY = viewRect.height() / sceneRect.height();
    qreal scale = qMin(scaleX, scaleY);

    // Clamp to valid range (10% - 1000%)
    scale = qBound(0.1, scale, 10.0);

    setViewScale(scale);
}

QStringList PsdWidget::fontsUsed() const
{
    QStringList fonts;

    // Traverse all layers to find text layers and collect font names
    std::function<void(const QModelIndex &)> collectFonts = [&](const QModelIndex &index) {
        if (index.isValid()) {
            const auto *item = d->model.layerItem(index);
            if (item && item->type() == QPsdAbstractLayerItem::Text) {
                const auto *textItem = dynamic_cast<const QPsdTextLayerItem *>(item);
                if (textItem) {
                    for (const auto &run : textItem->runs()) {
                        // Get the original font name from the PSD
                        QString fontName = run.originalFontName;
                        if (!fontName.isEmpty() && !fonts.contains(fontName)) {
                            fonts.append(fontName);
                        }
                    }
                }
            }
        }
        for (int i = 0; i < d->model.rowCount(index); i++) {
            collectFonts(d->model.index(i, 0, index));
        }
    };
    collectFonts({});

    fonts.sort();
    return fonts;
}

void PsdWidget::showFontMappingDialog()
{
    QPsdFontMappingDialog dialog(fileName(), this);
    dialog.setFontsUsed(fontsUsed());

    if (dialog.exec() == QDialog::Accepted) {
        // Reload to apply new font mappings
        reload();
    }
}

void PsdWidget::saveFigmaPlugin(QWidget *parent)
{
    const QString dir = QFileDialog::getExistingDirectory(
        parent, tr("Save Figma Plugin to..."));
    if (dir.isEmpty())
        return;

    struct { const char *resource; const char *relative; } files[] = {
        { ":/figma/plugin/manifest.json", "manifest.json" },
        { ":/figma/plugin/dist/main.js",  "dist/main.js" },
        { ":/figma/plugin/dist/ui.html",  "dist/ui.html" },
    };

    QDir destDir(dir);
    if (!destDir.mkpath("dist"_L1)) {
        QMessageBox::critical(parent, tr("Error"),
            tr("Failed to create directory:\n%1").arg(destDir.filePath("dist"_L1)));
        return;
    }

    for (const auto &f : files) {
        QFile src(QString::fromLatin1(f.resource));
        if (!src.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(parent, tr("Error"),
                tr("Failed to read resource:\n%1").arg(QLatin1StringView(f.resource)));
            return;
        }
        const QString destPath = destDir.filePath(QString::fromLatin1(f.relative));
        QFile dest(destPath);
        if (!dest.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(parent, tr("Error"),
                tr("Failed to write file:\n%1").arg(destPath));
            return;
        }
        dest.write(src.readAll());
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
