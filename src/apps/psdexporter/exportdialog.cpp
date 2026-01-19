// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "exportdialog.h"
#include "ui_exportdialog.h"

#include <QtCore/QSettings>
#include <QtCore/QMetaMethod>
#include <QtPsdExporter/QPsdExporterPlugin>
#include <QtWidgets/QFileDialog>

class ExportDialog::Private : public Ui::ExportDialog
{
public:
    Private(QPsdExporterPlugin *plugin, const QSize &size, const QVariantMap &hint, ::ExportDialog *parent)
        : q(parent)
        , plugin(plugin)
    {
        setupUi(q);
        q->setWindowIcon(plugin->icon());
        q->setWindowTitle(tr("Export as %1").arg(plugin->name().remove("&")));
        settings.beginGroup("ExportDialog");
        settings.beginGroup(plugin->key());
        directory->setText(settings.value("directory").toString());
        licenseText->setPlainText(settings.value("licenseText").toString());
        resolution->addItem(tr("Original\t(%1x%2)").arg(size.width()).arg(size.height()), size);
        resolution->addItem(tr("4K\t(3840x2160)"), QSize(3840, 2160));
        resolution->addItem(tr("FHD\t(1920x1080)"), QSize(1920, 1080));
        resolution->addItem(tr("HD\t(1280x720)"), QSize(1280, 720));
        resolution->addItem(tr("XGA\t(1024x768)"), QSize(1024, 768));
        resolution->addItem(tr("SVGA\t(800x600)"), QSize(800, 600));
        resolution->addItem(tr("VGA\t(640x480)"), QSize(640, 480));
        resolution->addItem(tr("QVGA\t(320x240)"), QSize(320, 240));
        resolution->addItem(tr("Custom"));
        int resolutionIndex = hint.value("resolutionIndex", settings.value("resolution", 0)).toInt();
        if (resolutionIndex < 0 || resolutionIndex >= resolution->count())
            resolutionIndex = 0;
        resolution->setCurrentIndex(resolutionIndex);
        connect(resolution, &QComboBox::currentIndexChanged, q, [=](int index) {
            const auto data = resolution->currentData();
            width->setEnabled(!data.isValid());
            height->setEnabled(!data.isValid());
        });
        width->setValue(hint.value("width", settings.value("width", 800).toInt()).toInt());
        height->setValue(hint.value("height", settings.value("height", 600).toInt()).toInt());
        const auto data = resolution->currentData();
        width->setEnabled(!data.isValid());
        height->setEnabled(!data.isValid());

        fontScaleFactor->setValue(hint.value("fontScaleFactor", settings.value("fontScaleFactor", 1.0).toDouble()).toDouble());
        ImageScaling imageScaling = static_cast<ImageScaling>(hint.value("imageScaling", settings.value("imageScaling", Original).toInt()).toInt());
        switch (imageScaling) {
        case Original:
            original->setChecked(true);
            break;
        case Scaled:
            scaled->setChecked(true);
            break;
        }
        compact->setChecked(hint.value("makeCompact", settings.value("makeCompact", false).toBool()).toBool());

        const auto mo = plugin->metaObject();
        for (int i = mo->propertyOffset(); i < mo->propertyCount(); i++) {
            const auto prop = mo->property(i);
            const auto key = prop.name();
            switch (prop.typeId()) {
            case QMetaType::Bool: {
                auto checkBox = new QCheckBox;
                checkBox->setObjectName(key);
                checkBox->setChecked(settings.value(u"%1_%2"_s.arg(plugin->name()).arg(key), prop.read(plugin)).toBool());
                formLayout->addRow(plugin->toUpperCamelCase(key, " "), checkBox);
                break; }
            default:
                formLayout->addRow(key, new QLabel(u"%1(%2) not supported"_s.arg(prop.typeName()).arg(prop.typeId())));
                break;
            }
        }
        connect(selectDirectory, &QToolButton::clicked, q, [=]() {
            const QString ret = QFileDialog::getExistingDirectory(q, q->windowTitle(), directory->text());
            if (ret.isEmpty())
                return;
            directory->setText(ret);
        });
    }

private:
    ::ExportDialog *q;
public:
    QPsdExporterPlugin *plugin;
    QSettings settings;
};

ExportDialog::ExportDialog(QPsdExporterPlugin *plugin, const QSize &size, const QVariantMap &hint, QWidget *parent)
    : QDialog(parent)
    , d(new Private(plugin, size, hint, this))
{}

ExportDialog::~ExportDialog() = default;

void ExportDialog::accept()
{
    d->settings.setValue("directory", d->directory->text());
    d->settings.setValue("licenseText", d->licenseText->toPlainText());

    const auto mo = d->plugin->metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); i++) {
        const auto prop = mo->property(i);
        const auto key = prop.name();
        switch (prop.typeId()) {
        case QMetaType::Bool: {
            const auto widget = findChild<QCheckBox *>(key);
            Q_ASSERT(widget);
            prop.write(d->plugin, widget->isChecked());
            d->settings.setValue(u"%1_%2"_s.arg(d->plugin->name()).arg(key), widget->isChecked());
            break; }
        }
    }
    QDialog::accept();
}

QString ExportDialog::directory() const
{
    return d->directory->text();
}

QSize ExportDialog::resolution() const
{
    auto ret = d->resolution->currentData().toSize();
    if (ret.isEmpty())
        ret = QSize(d->width->value(), d->height->value());
    return ret;
}

qreal ExportDialog::fontScaleFactor() const
{
    return d->fontScaleFactor->value();
}

ExportDialog::ImageScaling ExportDialog::imageScaling() const
{
    return d->original->isChecked() ? ImageScaling::Original : ImageScaling::Scaled;
}

bool ExportDialog::makeCompact() const
{
    return d->compact->isChecked();
}

int ExportDialog::resolutionIndex() const
{
    return d->resolution->currentIndex();
}

int ExportDialog::width() const
{
    return d->width->value();
}

int ExportDialog::height() const
{
    return d->height->value();
}

QString ExportDialog::licenseText() const
{
    return d->licenseText->toPlainText();
}
