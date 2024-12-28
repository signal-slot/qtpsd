// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef ITEMEXPORTSETTINGDIALOG_H
#define ITEMEXPORTSETTINGDIALOG_H

#include <QtWidgets/QDialog>

class QPsdAbstractLayerItem;

class ItemExportSettingDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ItemExportSettingDialog(QWidget *parent = nullptr);
    ~ItemExportSettingDialog() override;

public slots:
    void setItem(const QPsdAbstractLayerItem *item, const QMap<quint32, QString> groupMap);
    void accept() override;

private:
    class Private;
    QScopedPointer<Private> d;
};

#endif // ITEMEXPORTSETTINGDIALOG_H
