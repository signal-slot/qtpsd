// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDFONTMAPPINGDIALOG_H
#define QPSDFONTMAPPINGDIALOG_H

#include <QtPsdWidget/qpsdwidgetglobal.h>

#include <QtWidgets/QDialog>

QT_BEGIN_NAMESPACE

class Q_PSDWIDGET_EXPORT QPsdFontMappingDialog : public QDialog
{
    Q_OBJECT
public:
    explicit QPsdFontMappingDialog(const QString &psdPath, QWidget *parent = nullptr);
    ~QPsdFontMappingDialog() override;

    // Set the list of fonts used in the PSD file
    void setFontsUsed(const QStringList &fonts);

    // Get fonts that were mapped (original name -> target family)
    QHash<QString, QString> mappings() const;

    // Get which mappings should be global
    QHash<QString, bool> globalFlags() const;

public slots:
    void accept() override;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDFONTMAPPINGDIALOG_H
