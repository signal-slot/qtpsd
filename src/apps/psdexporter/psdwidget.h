// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PSDWIDGET_H
#define PSDWIDGET_H

#include <QtCore/QSettings>
#include <QtWidgets/QSplitter>

#include <QtPsdGui/QPsdFolderLayerItem>
#include <QtPsdExporter/QPsdExporterPlugin>

class PsdWidget : public QSplitter
{
    Q_OBJECT
public:
    explicit PsdWidget(QWidget *parent = nullptr);
    ~PsdWidget() override;

    QString fileName() const;

    QString errorMessage() const;
    QVariantMap exportHint(const QString& exporterKey) const;
    void updateExportHint(const QString &key, const QVariantMap &hint);

    qreal viewScale() const;

public slots:
    void load(const QString &fileName);
    void reload();
    void save();
    void exportTo(QPsdExporterPlugin *exporter, QSettings *settings);
    void copyViewToClipboard();
    void setViewScale(qreal scale);

private slots:
    void setErrorMessage(const QString &errorMessage);

signals:
    void errorOccurred(const QString &errorMessage);
    void selectionInfoChanged(const QString &info);

private:
    class Private;
    QScopedPointer<Private> d;
};

#endif // PSDWIDGET_H
