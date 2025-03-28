// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QtWidgets/QDialog>

class QPsdExporterPlugin;

class ExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExportDialog(QPsdExporterPlugin *plugin, const QSize &size, const QVariantMap &hint, QWidget *parent = nullptr);
    ~ExportDialog() override;

    QString directory() const;
    QSize resolution() const;
    qreal fontScaleFactor() const;
    enum ImageScaling {
        Original,
        Scaled,
    };
    ImageScaling imageScaling() const;
    bool makeCompact() const;
    int resolutionIndex() const;
    int width() const;
    int height() const;
    QString licenseText() const;

public slots:
    void accept() override;

private:
    class Private;
    QScopedPointer<Private> d;
};

#endif // EXPORTDIALOG_H
