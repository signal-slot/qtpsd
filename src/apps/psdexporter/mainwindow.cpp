// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "psdwidget.h"
#include "exportdialog.h"

#include <QtCore/QSettings>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtPsdExporter/QPsdExporterPlugin>

class MainWindow::Private : public Ui::MainWindow
{
public:
    Private(::MainWindow *parent);
    ~Private();

    QMessageBox::StandardButton checkModifiedAndSaved(int index = -1, bool confirm = true, bool all = false);
private:
    void openFile(const QString &fileName);
    void updateRecentFiles(const QString &fileName = QString());
    void updateFileMenus();

private:
    ::MainWindow *q;

public:
    QString applicationName;
    QSettings settings;
};

MainWindow::Private::Private(::MainWindow *parent)
    : q(parent)
{
    settings.beginGroup("MainWindow");

    setupUi(q);

    applicationName = q->windowTitle();

    q->centralWidget()->deleteLater();
    for (int i = 0; i < tabWidget->count(); i++)
        tabWidget->widget(i)->deleteLater();
    tabWidget->clear();
    q->setCentralWidget(tabWidget);

    connect(open, &QAction::triggered, q, [this]() {
        settings.beginGroup("Menu");
        QString dir = settings.value("OpenDir").toString();
        settings.endGroup();
        QString fileName = QFileDialog::getOpenFileName(q, tr("Open PSD File"), dir, tr("PSD Files (*.psd *.psdt)"));
        if (!fileName.isEmpty()) {
            const auto currentIndex = tabWidget->currentIndex();
            openFile(fileName);
            if (currentIndex != tabWidget->currentIndex()) {
                settings.beginGroup("Menu");
                settings.setValue("OpenDir", QFileInfo(fileName).absoluteDir().absolutePath());
                settings.endGroup();
            }
        }
    });

    connect(openRecentFile, &QMenu::triggered, q, [this](QAction *action) {
        openFile(action->data().toString());
    });
    updateRecentFiles();

    connect(clearRecentFiles, &QAction::triggered, q, [this]() {
        settings.beginGroup("Menu");
        settings.setValue("Recent Files", QStringList());
        settings.endGroup();
        updateRecentFiles();
    });

    connect(reload, &QAction::triggered, q, [this] () {
        int index = tabWidget->currentIndex();
        auto psdWidget = qobject_cast<PsdWidget *>(tabWidget->widget(index));
        psdWidget->reload();
    });

    auto keys = QPsdExporterPlugin::keys();
    QList<QPsdExporterPlugin *> plugins;
    for (const auto &key : keys) {
        auto plugin = QPsdExporterPlugin::plugin(key);
        plugins.append(plugin);
    }
    std::sort(plugins.begin(), plugins.end(), [](const auto *a, const auto *b) {
        return a->priority() == b->priority() ? a->name() < b->name() : a->priority() < b->priority();;
    });
    for (auto *plugin : plugins) {
        QString name = plugin->name();
        QIcon icon = plugin->icon();
        auto action = new QAction(icon, name + "..."_L1);
        action->setData(QVariant::fromValue(plugin));
        exports->addAction(action);
        if (!icon.isNull())
            toolBar->addAction(action);
    }
    connect(exports, &QMenu::triggered, q, [this](QAction *action) {
        QPsdExporterPlugin *exporter = action->data().value<QPsdExporterPlugin *>();
        Q_ASSERT(exporter);
        int index = tabWidget->currentIndex();
        auto psdWidget = qobject_cast<PsdWidget *>(tabWidget->widget(index));
        const auto *tree = psdWidget->layerTree();

        QString to;
        QVariantMap hint;
        switch (exporter->exportType()) {
        case QPsdExporterPlugin::File: {
            settings.beginGroup("Menu");
            settings.beginGroup(exporter->name());
            QString dir = settings.value("ExportDir").toString();
            settings.endGroup();
            settings.endGroup();
            QString selected;
            to = QFileDialog::getSaveFileName(q, tr("Export as %1").arg(exporter->name().remove("&")), dir, exporter->filters().keys().join(";;"_L1), &selected, QFileDialog::DontConfirmOverwrite);
            if (to.isEmpty())
                return;
            QString suffix = exporter->filters().value(selected);
            if (!suffix.isEmpty() && !to.endsWith(suffix, Qt::CaseInsensitive))
                to += suffix;
            QFileInfo fi(to);
            if (fi.exists()) {
                const auto ret = QMessageBox::question(q, tr("Confirm Overwrite"), tr("The file %1 already exists. Do you want to overwrite it?").arg(fi.fileName()), QMessageBox::Yes | QMessageBox::No);
                if (ret != QMessageBox::Yes)
                    return;
            }
            settings.beginGroup("Menu");
            settings.beginGroup(exporter->name());
            settings.setValue("ExportDir", QFileInfo(to).absoluteDir().absolutePath());
            settings.endGroup();
            settings.endGroup();
            break; }
        case QPsdExporterPlugin::Directory:
            ExportDialog dialog(exporter, tree->rect().size(), psdWidget->exportHint(exporter->key()), q);
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

        psdWidget->updateExportHint(exporter->key(), hint);
        psdWidget->save();
        exporter->exportTo(tree, to, hint);
    });

    connect(save, &QAction::triggered, q, [this]() {
        int index = tabWidget->currentIndex();
        auto psdWidget = qobject_cast<PsdWidget *>(tabWidget->widget(index));
        psdWidget->save();
    });

    connect(q, &QWidget::windowTitleChanged, q, [this]() {
        const auto modified = q->isWindowModified();
        save->setEnabled(modified);
    });

    connect(close, &QAction::triggered, q, [this]() {
        const auto index = tabWidget->currentIndex();
        if (index < 0)
            return;
        const auto ret = checkModifiedAndSaved(index);
        if (ret == QMessageBox::Cancel)
            return;
        tabWidget->widget(index)->deleteLater();
        tabWidget->removeTab(index);
        updateFileMenus();
    });

    connect(quit, &QAction::triggered, q, [this]() {
        q->close();
    });

    connect(aboutQt, &QAction::triggered, q, []() {
        qApp->aboutQt();
    });

    connect(tabWidget, &QTabWidget::currentChanged, q, [this](int index) {
        if (index < 0) {
            q->setWindowModified(false);
            q->setWindowTitle(applicationName);
            return;
        }
        q->setWindowModified(tabWidget->currentWidget()->isWindowModified());
        q->setWindowTitle(tabWidget->currentWidget()->windowTitle().replace("*", "[*]") + " - " + applicationName);
    }, Qt::QueuedConnection); // first addTab changes its index before tooltip is set

    connect(tabWidget, &QTabWidget::tabCloseRequested, q, [this](int index) {
        auto ret = checkModifiedAndSaved(index);
        if (ret == QMessageBox::Cancel)
            return;
        tabWidget->widget(index)->deleteLater();
        tabWidget->removeTab(index);
        updateFileMenus();
    });

    updateFileMenus();

    settings.beginGroup("Session");
    const QStringList files = settings.value("files").toStringList();
    for (const QString &file : files) {
        openFile(file);
    }
    if (!files.isEmpty()) {
        const auto index = settings.value("index", 0).toInt();
        if (0 <= index && index < tabWidget->count())
            tabWidget->setCurrentIndex(index);
    }
    settings.endGroup();
}

MainWindow::Private::~Private()
{
    settings.beginGroup("Session");
    QStringList files;
    for (int i = 0; i < tabWidget->count(); i++) {
        auto viewer = qobject_cast<PsdWidget *>(tabWidget->widget(i));
        files.append(viewer->fileName());
    }
    settings.setValue("files", files);
    settings.setValue("index", tabWidget->currentIndex());
    settings.endGroup();
    settings.endGroup();
}

QMessageBox::StandardButton MainWindow::Private::checkModifiedAndSaved(int index, bool confirm, bool all)
{
    QMessageBox::StandardButton ret = QMessageBox::NoButton;
    if (index < 0) {
        for (int i = 0; i < tabWidget->count(); i++) {
            ret = checkModifiedAndSaved(i, confirm, true);
            switch (ret) {
            case QMessageBox::Cancel:
                return ret;
            case QMessageBox::Save:
            case QMessageBox::Discard:
                break;
            case QMessageBox::SaveAll:
                confirm = false;
                break;
            default:
                break;
            }
        }
    } else {
        auto viewer = qobject_cast<PsdWidget *>(tabWidget->widget(index));
        if (viewer->isWindowModified()) {
            tabWidget->setCurrentIndex(index);
            if (confirm) {
                QMessageBox::StandardButtons buttons = QMessageBox::Save | QMessageBox::Cancel | QMessageBox::Discard;
                if (all)
                    buttons |= QMessageBox::SaveAll;
                ret = QMessageBox::question(q, tr("Save changes?"), tr("The document has been modified. Do you want to save your changes?"), buttons, QMessageBox::Save);
                switch (ret) {
                case QMessageBox::SaveAll:
                case QMessageBox::Save:
                    break;
                default:
                    return ret;
                }
            }
        }
        viewer->save();
    }
    return ret;
}

void MainWindow::Private::openFile(const QString &fileName)
{
    // check if the file is already opened
    for (int i = 0; i < tabWidget->count(); i++) {
        if (tabWidget->tabToolTip(i) == fileName) {
            tabWidget->setCurrentIndex(i);
            return;
        }
    }

    // open the file as a new tab
    auto viewer = new PsdWidget(q);
    viewer->load(fileName);
    if (viewer->errorMessage().isEmpty()) {
        int index = tabWidget->addTab(viewer, viewer->windowIcon(), viewer->windowTitle());
        connect(viewer, &PsdWidget::windowTitleChanged, q, [this, viewer](const QString &title) {
            for (int i = 0; i < tabWidget->count(); i++) {
                if (tabWidget->widget(i) == viewer) {
                    tabWidget->setTabText(i, title);
                }
            }
            if (tabWidget->currentWidget() == viewer) {
                q->setWindowModified(viewer->isWindowModified());
                q->setWindowTitle(title + " - " + applicationName);
            }
        });
        tabWidget->setTabToolTip(index, fileName);
        tabWidget->setCurrentIndex(index);
        updateRecentFiles(fileName);
        updateFileMenus();
    } else {
        delete viewer;
    }
}

void MainWindow::Private::updateRecentFiles(const QString &fileName)
{
    settings.beginGroup("Menu");
    QStringList files = settings.value("Recent Files").toStringList();
    if (!fileName.isEmpty()) {
        files.removeAll(fileName);
        files.prepend(fileName);
        while (files.size() > 10)
            files.removeLast();
    }

    auto actions = openRecentFile->actions();
    static QAction *separator = actions.first();
    for (QAction *action : actions) {
        if (action == separator) break;
        openRecentFile->removeAction(action);
        delete action;
    }

    for (const QString &file : files) {
        auto action = new QAction(file);
        action->setData(file);
        openRecentFile->insertAction(separator, action);
    }
    openRecentFile->setEnabled(!files.isEmpty());
    settings.setValue("Recent Files", files);
    settings.endGroup();
}

void MainWindow::Private::updateFileMenus()
{
    bool enabled = tabWidget->count() > 0;
    exports->setEnabled(enabled);
    reload->setEnabled(enabled);
    save->setEnabled(enabled && q->isWindowModified());
    close->setEnabled(enabled);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , d(new Private(this))
{}

MainWindow::~MainWindow() = default;


void MainWindow::showEvent(QShowEvent *event)
{
    Q_UNUSED(event);
    restoreGeometry(d->settings.value("geometry").toByteArray());
    restoreState(d->settings.value("state").toByteArray());
}

void MainWindow::hideEvent(QHideEvent *event)
{
    Q_UNUSED(event);
    d->settings.setValue("geometry", saveGeometry());
    d->settings.setValue("state", saveState());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    auto ret = d->checkModifiedAndSaved();
    if (ret == QMessageBox::Cancel)
        event->ignore();
}
