// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "itemexportsettingdialog.h"
#include "ui_itemexportsettingdialog.h"

#include <QtPsdGui/QPsdAbstractLayerItem>
#include <QtPsdGui/QPsdTextLayerItem>

class ItemExportSettingDialog::Private : public Ui::ItemExportSettingDialog
{
public:
    Private(::ItemExportSettingDialog *parent);

private:
    ::ItemExportSettingDialog *q;

public:
    const QPsdAbstractLayerItem *item = nullptr;
};

ItemExportSettingDialog::Private::Private(::ItemExportSettingDialog *parent)
    : q(parent)
{
    setupUi(q);

    customBase->addItem("Container", QPsdAbstractLayerItem::ExportHint::NativeComponent::Container);
    nativeBase->addItem("Container", QPsdAbstractLayerItem::ExportHint::NativeComponent::Container);

#define ADDITEM(x) \
    customBase->addItem(QPsdAbstractLayerItem::ExportHint::nativeCode2Name(QPsdAbstractLayerItem::ExportHint::x), QPsdAbstractLayerItem::ExportHint::NativeComponent::x); \
    nativeBase->addItem(QPsdAbstractLayerItem::ExportHint::nativeCode2Name(QPsdAbstractLayerItem::ExportHint::x), QPsdAbstractLayerItem::ExportHint::NativeComponent::x)

    ADDITEM(TouchArea);
    ADDITEM(Button);
    ADDITEM(Button_Highlighted);
#undef ADDITEM
    customBase->setCurrentText("Button");
    nativeBase->setCurrentText("Button");

    connect(typeEmbed, &QRadioButton::toggled, q, [this](bool checked) {
        embedWithTouch->setEnabled(checked);
    });
    embedWithTouch->setEnabled(typeEmbed->isChecked());

    connect(typeMerge, &QRadioButton::toggled, q, [this](bool checked) {
        merge->setEnabled(checked);
    });
    merge->setEnabled(typeMerge->isChecked());

    connect(typeCustom, &QRadioButton::toggled, q, [this](bool checked) {
        custom->setEnabled(checked);
        customBase->setEnabled(checked);
    });
    custom->setEnabled(typeCustom->isChecked());
    customBase->setEnabled(typeCustom->isChecked());

    connect(typeNative, &QRadioButton::toggled, q, [this](bool checked) {
        nativeBase->setEnabled(checked);
    });
    nativeBase->setEnabled(typeNative->isChecked());
}

ItemExportSettingDialog::ItemExportSettingDialog(QWidget *parent)
    : QDialog(parent)
    , d(new Private(this))
{}

ItemExportSettingDialog::~ItemExportSettingDialog() = default;

void ItemExportSettingDialog::setItem(const QPsdAbstractLayerItem *item, QMap<quint32, QString> groupMap)
{
    d->item = item;

    if (!item)
        return;

    switch(item->type()) {
    case QPsdAbstractLayerItem::Text:
        setWindowTitle(u"Text: %1"_s.arg(item->name()));
        break;
    case QPsdAbstractLayerItem::Shape:
        setWindowTitle(u"Shape: %1"_s.arg(item->name()));
        break;
    case QPsdAbstractLayerItem::Image:
        setWindowTitle(u"Image: %1"_s.arg(item->name()));
        break;
    case QPsdAbstractLayerItem::Folder:
        setWindowTitle(u"Folder: %1"_s.arg(item->name()));
        break;
    }

    d->name->setText(item->name());
    const auto exportHint = item->exportHint();
    d->id->setText(exportHint.id);
    switch (exportHint.type) {
    case QPsdAbstractLayerItem::ExportHint::Embed:
        d->typeEmbed->setChecked(true);
        d->embedWithTouch->setChecked(exportHint.baseElement == QPsdAbstractLayerItem::ExportHint::TouchArea);
        break;
    case QPsdAbstractLayerItem::ExportHint::Merge:
        d->typeMerge->setChecked(true);
        d->merge->setCurrentText(exportHint.componentName);
        break;
    case QPsdAbstractLayerItem::ExportHint::Custom:
        d->typeCustom->setChecked(true);
        d->custom->setText(exportHint.componentName);
        d->customBase->setCurrentText(QPsdAbstractLayerItem::ExportHint::nativeCode2Name(exportHint.baseElement));
        break;
    case QPsdAbstractLayerItem::ExportHint::Native:
        d->typeNative->setChecked(true);
        d->nativeBase->setCurrentText(QPsdAbstractLayerItem::ExportHint::nativeCode2Name(exportHint.baseElement));
        break;
    case QPsdAbstractLayerItem::ExportHint::Skip:
        d->typeSkip->setChecked(true);
        break;
    }

    for (const auto i : groupMap.keys()) {
        if (i == item->id())
            continue;
        d->merge->addItem(groupMap.value(i));
    }

    d->typeMerge->setEnabled(groupMap.size() > 1);

    static const QSet<QString> baseProperties = {
        "visible",
        "position",
        "size",
    };
    static const QHash<QPsdAbstractLayerItem::Type, QSet<QString>> properties = {
        { QPsdAbstractLayerItem::Text, { "color", "text" } },
        { QPsdAbstractLayerItem::Shape, { "color" } },
        { QPsdAbstractLayerItem::Image, { "image" } }
    };
    const auto checkboxes = d->properties->findChildren<QCheckBox *>();
    for (auto *checkbox : checkboxes) {
        const auto name = checkbox->objectName();
        if (baseProperties.contains(name) || properties.value(item->type()).contains(name)) {
            checkbox->setChecked(exportHint.properties.contains(name));
            checkbox->setEnabled(true);
        } else {
            checkbox->setChecked(false);
            checkbox->setEnabled(false);
        }
    }
}

void ItemExportSettingDialog::accept()
{
    if (d->item) {
        if (d->typeCustom->isChecked() && d->custom->text().isEmpty()) {
            d->custom->setFocus();
            return;
        }
        QPsdAbstractLayerItem::ExportHint exportHint = d->item->exportHint();
        exportHint.id = d->id->text();
        if (d->typeEmbed->isChecked()) {
            exportHint.type = QPsdAbstractLayerItem::ExportHint::Embed;
            if (d->embedWithTouch->isChecked())
                exportHint.baseElement = QPsdAbstractLayerItem::ExportHint::TouchArea;
        } else if (d->typeMerge->isChecked()) {
            exportHint.type = QPsdAbstractLayerItem::ExportHint::Merge;
            exportHint.componentName = d->merge->currentText();
        } else if (d->typeCustom->isChecked()) {
            exportHint.type = QPsdAbstractLayerItem::ExportHint::Custom;
            exportHint.componentName = d->custom->text();
            exportHint.baseElement = static_cast<QPsdAbstractLayerItem::ExportHint::NativeComponent>(d->customBase->currentData().toInt());
        } else if (d->typeNative->isChecked()) {
            exportHint.type = QPsdAbstractLayerItem::ExportHint::Native;
            exportHint.baseElement = static_cast<QPsdAbstractLayerItem::ExportHint::NativeComponent>(d->nativeBase->currentData().toInt());
        } else if (d->typeSkip->isChecked()) {
            exportHint.type = QPsdAbstractLayerItem::ExportHint::Skip;
        }
        exportHint.properties.clear();
        const auto checkboxes = d->properties->findChildren<QCheckBox *>();
        for (const auto *checkbox : checkboxes) {
            if (checkbox->isChecked())
                exportHint.properties.insert(checkbox->objectName());
        }
        d->item->setExportHint(exportHint);
    }

    QDialog::accept();
}
