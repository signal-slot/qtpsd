// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "psdwidget.h"
#include "ui_psdwidget.h"

#include <QtPsdExporter/psdtreeitemmodel.h>
#include "exportdialog.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>

#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

class PsdWidget::Private : public Ui::PsdWidget
{
public:
    Private(::PsdWidget *parent);

    void updateAttributes();
    void applyAttributes();

    PsdTreeItemModel model;

    QString errorMessage;
    QSettings settings;
    QString windowTitle;

private:
    ::PsdWidget *q;
    QButtonGroup types;
};

PsdWidget::Private::Private(::PsdWidget *parent)
    : q(parent)
{
    setupUi(q);
    treeView->setModel(&model);

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
        psdView->clearSelection();
    });

    connect(psdView, &PsdView::itemSelected, q, [this](const QModelIndex &index) {
        treeView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    });

    connect(&model, &PsdTreeItemModel::fileInfoChanged, q, [this](const QFileInfo &fileInfo) {
        windowTitle = fileInfo.fileName();
        q->setWindowTitle(windowTitle);
    });

    updateAttributes();
    settings.beginGroup("Files");

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
    types.setId(typeEmbed, QPsdFolderLayerItem::ExportHint::Embed);
    types.setId(typeMerge, QPsdFolderLayerItem::ExportHint::Merge);
    types.setId(typeCustom, QPsdFolderLayerItem::ExportHint::Custom);
    types.setId(typeNative, QPsdFolderLayerItem::ExportHint::Native);
    types.setId(typeSkip, QPsdFolderLayerItem::ExportHint::Skip);

#define ADDITEM(x) \
    customBase->addItem(QPsdAbstractLayerItem::ExportHint::nativeCode2Name(QPsdAbstractLayerItem::ExportHint::x), QPsdAbstractLayerItem::ExportHint::NativeComponent::x); \
        nativeBase->addItem(QPsdAbstractLayerItem::ExportHint::nativeCode2Name(QPsdAbstractLayerItem::ExportHint::x), QPsdAbstractLayerItem::ExportHint::NativeComponent::x)

    ADDITEM(Container);
    ADDITEM(TouchArea);
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

    connect(typeMerge, &QRadioButton::toggled, q, [=](bool checked) {
        merge->setEnabled(checked);
        if (isReset())
            changed();
    });
    merge->setEnabled(typeMerge->isChecked());
    connect(merge, &QComboBox::currentIndexChanged, q, [=]() {
        if (isReset())
            changed();
    });

    connect(typeCustom, &QRadioButton::toggled, q, [=](bool checked) {
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

    custom->setEnabled(typeCustom->isChecked());
    connect(custom, &QLineEdit::textChanged, q, [=]() {
        if (isReset())
            changed();
    });
    customBase->setEnabled(typeCustom->isChecked());
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
        return;
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
        { QPsdAbstractLayerItem::Text, { "color", "text" } },
        { QPsdAbstractLayerItem::Shape, { "color" } },
        { QPsdAbstractLayerItem::Image, { "image" } }
    };

    UniqueOrNot<QPsdAbstractLayerItem::ExportHint::Type> itemTypes(QPsdAbstractLayerItem::ExportHint::None);
    UniqueOrNot<Qt::CheckState> itemWithTouch(Qt::PartiallyChecked);
    UniqueOrNot<QList<QPersistentModelIndex>> itemMergeGroup;
    QSet<const QPersistentModelIndex> excludeFromMergeGroup;
    UniqueOrNot<QString> itemComponentName;
    UniqueOrNot<QString> itemCustom;
    UniqueOrNot<QPsdAbstractLayerItem::ExportHint::NativeComponent> itemCustomBase;
    QHash<QString, UniqueOrNot<Qt::CheckState>> itemProperties;

    for (const auto &row : rows) {
        const auto *item = model.layerItem(row);
        const auto groupVariantList = model.groupIndexes(row);
        const auto hint = item->exportHint();
        itemTypes.add(hint.type);
        itemWithTouch.add(hint.baseElement == QPsdAbstractLayerItem::ExportHint::TouchArea ? Qt::Checked : Qt::Unchecked);
        qDebug() << hint.baseElement << itemWithTouch.isUnique() << itemWithTouch.value();
        QList<QPersistentModelIndex> groupIndexList;
        for (auto &v : groupVariantList) {
            groupIndexList.append(v);
        }
        itemMergeGroup.add(groupIndexList);
        excludeFromMergeGroup.insert(row);
        itemComponentName.add(hint.componentName);
        itemCustomBase.add(hint.baseElement);

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

    if (itemTypes.isUnique()) {
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
    merge->clear();
    if (itemMergeGroup.isUnique()) {
        merge->setEnabled(true);
        for (auto item : itemMergeGroup.value()) {
            if (excludeFromMergeGroup.contains(item)) {
                continue;
            }
            QString name = model.layerName(item);
            merge->addItem(name);
        }
        merge->setCurrentIndex(-1);
        typeMerge->setEnabled(merge->count() > 0);
    } else {
        merge->setEnabled(false);
        typeMerge->setEnabled(false);
    }
    customEnabled->setChecked(rows.length() == 1);
    customEnabled->setEnabled(false);
    custom->setText(QString());
    custom->setEnabled(false);
    customBase->setCurrentIndex(-1);
    customBase->setEnabled(false);
    nativeBase->setCurrentIndex(-1);

    switch (itemTypes.value()) {
    case QPsdAbstractLayerItem::ExportHint::None:
        break;
    case QPsdAbstractLayerItem::ExportHint::Embed:
        embedWithTouch->setEnabled(true);
        qDebug() << itemWithTouch.value() << embedWithTouch->checkState();
        embedWithTouch->setCheckState(itemWithTouch.value());
        qDebug() << itemWithTouch.value() << embedWithTouch->checkState();
        break;
    case QPsdAbstractLayerItem::ExportHint::Merge:
        merge->setCurrentText(itemComponentName.value());
        break;
    case QPsdAbstractLayerItem::ExportHint::Custom:
        customEnabled->setChecked(itemComponentName.isUnique());
        if (customEnabled->isChecked()) {
            custom->setText(itemComponentName.value());
            if (itemCustomBase.isUnique()) {
                customBase->setCurrentText(QPsdAbstractLayerItem::ExportHint::nativeCode2Name(itemCustomBase.value()));
            }
        }
        break;
    case QPsdAbstractLayerItem::ExportHint::Native:
        if (itemCustomBase.isUnique()) {
            nativeBase->setCurrentText(QPsdAbstractLayerItem::ExportHint::nativeCode2Name(itemCustomBase.value()));
        }
        break;
    case QPsdAbstractLayerItem::ExportHint::Skip:
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
        const auto *item = model.layerItem(row);
        auto hint = item->exportHint();
        if (type > -1)
            hint.type = static_cast<QPsdAbstractLayerItem::ExportHint::Type>(type);
        switch (type) {
        case QPsdAbstractLayerItem::ExportHint::Embed:
            hint.type = QPsdAbstractLayerItem::ExportHint::Embed;
            switch (withTouch) {
            case Qt::Checked:
                hint.baseElement = QPsdAbstractLayerItem::ExportHint::TouchArea;
                break;
            case Qt::Unchecked:
                hint.baseElement = QPsdAbstractLayerItem::ExportHint::Container;
                break;
            default:
                break;
            }
            break;
        case QPsdAbstractLayerItem::ExportHint::Merge:
            hint.type = QPsdAbstractLayerItem::ExportHint::Merge;
            hint.componentName = merge->currentText();
            break;
        case QPsdAbstractLayerItem::ExportHint::Custom:
            hint.type = QPsdAbstractLayerItem::ExportHint::Custom;
            if (customEnabled->isEnabled()) {
                hint.componentName = custom->text();
                hint.baseElement = QPsdAbstractLayerItem::ExportHint::nativeName2Code(customBase->currentText());
            }
            break;
        case QPsdAbstractLayerItem::ExportHint::Native:
            hint.type = QPsdAbstractLayerItem::ExportHint::Native;
            hint.baseElement = QPsdAbstractLayerItem::ExportHint::nativeName2Code(nativeBase->currentText());
            break;
        case QPsdAbstractLayerItem::ExportHint::Skip:
            hint.type = QPsdAbstractLayerItem::ExportHint::Skip;
            break;
        }

        hint.properties = propertiesChecked;

        model.setLayerHint(row, hint);
    }
}

PsdWidget::PsdWidget(QWidget *parent)
    : QSplitter(parent)
    , d(new Private(this))
{}

PsdWidget::~PsdWidget()
{
    d->settings.setValue("splitterState", saveState());
    d->settings.setValue("treeState", d->treeView->header()->saveState());
}

void PsdWidget::load(const QString &fileName)
{
    d->model.load(fileName);
    d->treeView->reset();

    d->settings.beginGroup(QCryptographicHash::hash(fileName.toUtf8(), QCryptographicHash::Md5));
    restoreState(d->settings.value("splitterState").toByteArray());
    d->treeView->header()->restoreState(d->settings.value("treeState").toByteArray());

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

    d->psdView->setModel(&d->model);
}

void PsdWidget::reload()
{
    save();
    d->settings.endGroup();
    load(d->model.fileName());
    d->treeView->reset();
}

void PsdWidget::save()
{
    d->model.save();

    setWindowModified(false);
    setWindowTitle(d->windowTitle);
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
    case QPsdExporterPlugin::Directory:
        ExportDialog dialog(exporter, d->model.size(), exportHint(exporter->key()), this);
        const auto ret = dialog.exec();
        if (ret != QDialog::Accepted)
            return;
        to = dialog.directory();
        hint.insert("resolution", dialog.resolution());
        hint.insert("resolutionIndex", dialog.resolutionIndex());
        hint.insert("width", dialog.width());
        hint.insert("height", dialog.height());
        hint.insert("fontScaleFactor", dialog.fontScaleFactor());
        hint.insert("imageScaling", dialog.imageScaling() == ExportDialog::Scaled);
        hint.insert("makeCompact", dialog.makeCompact());
        break;
    }

    updateExportHint(exporter->key(), hint);
    save();
    exporter->exportTo(&d->model, to, hint);
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
