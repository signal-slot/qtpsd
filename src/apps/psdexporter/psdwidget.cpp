// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "psdwidget.h"
#include "ui_psdwidget.h"

#include "psdtreeitemmodel.h"
#include "exportdialog.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>

#include <QtGui/QClipboard>
#include <QtGui/QPainter>

#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
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

    QPsdWidgetTreeItemModel widgetModel;
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
    types.setId(typeMerge, QPsdExporterTreeItemModel::ExportHint::Merge);
    types.setId(typeComponent, QPsdExporterTreeItemModel::ExportHint::Component);
    types.setId(typeNative, QPsdExporterTreeItemModel::ExportHint::Native);
    types.setId(typeSkip, QPsdExporterTreeItemModel::ExportHint::Skip);

#define ADDITEM(x) \
    customBase->addItem(QPsdExporterTreeItemModel::ExportHint::nativeCode2Name(QPsdExporterTreeItemModel::ExportHint::x), QPsdExporterTreeItemModel::ExportHint::NativeComponent::x); \
        nativeBase->addItem(QPsdExporterTreeItemModel::ExportHint::nativeCode2Name(QPsdExporterTreeItemModel::ExportHint::x), QPsdExporterTreeItemModel::ExportHint::NativeComponent::x)

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
        return;
    }

    // Emit selection info for status bar
    if (rows.size() == 1) {
        const auto *item = model.layerItem(rows.first());
        if (item) {
            const auto rect = item->rect();
            emit q->selectionInfoChanged(QObject::tr("Position: %1, %2  Size: %3 × %4")
                .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
        }
    } else {
        emit q->selectionInfoChanged(QObject::tr("%1 items selected").arg(rows.size()));
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

    UniqueOrNot<QPsdExporterTreeItemModel::ExportHint::Type> itemTypes(QPsdExporterTreeItemModel::ExportHint::None);
    UniqueOrNot<Qt::CheckState> itemWithTouch(Qt::PartiallyChecked);
    UniqueOrNot<QList<QPersistentModelIndex>> itemMergeGroup;
    QSet<const QPersistentModelIndex> excludeFromMergeGroup;
    UniqueOrNot<QString> itemComponentName;
    UniqueOrNot<QString> itemCustom;
    UniqueOrNot<QPsdExporterTreeItemModel::ExportHint::NativeComponent> itemCustomBase;
    QHash<QString, UniqueOrNot<Qt::CheckState>> itemProperties;

    for (const auto &row : rows) {
        const auto *item = model.layerItem(row);
        const auto groupVariantList = model.groupIndexes(row);
        const auto hint = model.layerHint(row);
        itemTypes.add(hint.type);
        itemWithTouch.add(hint.baseElement == QPsdExporterTreeItemModel::ExportHint::TouchArea ? Qt::Checked : Qt::Unchecked);
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
    customEnabled->setEnabled(rows.length() > 1);
    custom->setText(QString());
    custom->setEnabled(false);
    customBase->setCurrentIndex(-1);
    customBase->setEnabled(false);
    nativeBase->setCurrentIndex(-1);

    switch (itemTypes.value()) {
    case QPsdExporterTreeItemModel::ExportHint::None:
        break;
    case QPsdExporterTreeItemModel::ExportHint::Embed:
        embedWithTouch->setEnabled(true);
        qDebug() << itemWithTouch.value() << embedWithTouch->checkState();
        embedWithTouch->setCheckState(itemWithTouch.value());
        qDebug() << itemWithTouch.value() << embedWithTouch->checkState();
        break;
    case QPsdExporterTreeItemModel::ExportHint::Merge:
        merge->setCurrentText(itemComponentName.value());
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
        if (type > -1)
            hint.type = static_cast<QPsdExporterTreeItemModel::ExportHint::Type>(type);
        switch (type) {
        case QPsdExporterTreeItemModel::ExportHint::Embed:
            hint.type = QPsdExporterTreeItemModel::ExportHint::Embed;
            switch (withTouch) {
            case Qt::Checked:
                hint.baseElement = QPsdExporterTreeItemModel::ExportHint::TouchArea;
                break;
            case Qt::Unchecked:
                hint.baseElement = QPsdExporterTreeItemModel::ExportHint::Container;
                break;
            default:
                break;
            }
            break;
        case QPsdExporterTreeItemModel::ExportHint::Merge:
            hint.type = QPsdExporterTreeItemModel::ExportHint::Merge;
            hint.componentName = merge->currentText();
            break;
        case QPsdExporterTreeItemModel::ExportHint::Component:
            hint.type = QPsdExporterTreeItemModel::ExportHint::Component;
            if (customEnabled->isEnabled()) {
                hint.componentName = custom->text();
                hint.baseElement = QPsdExporterTreeItemModel::ExportHint::nativeName2Code(customBase->currentText());
            }
            break;
        case QPsdExporterTreeItemModel::ExportHint::Native:
            hint.type = QPsdExporterTreeItemModel::ExportHint::Native;
            hint.baseElement = QPsdExporterTreeItemModel::ExportHint::nativeName2Code(nativeBase->currentText());
            break;
        case QPsdExporterTreeItemModel::ExportHint::Skip:
            hint.type = QPsdExporterTreeItemModel::ExportHint::Skip;
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
        hint.insert("licenseText", dialog.licenseText());
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
