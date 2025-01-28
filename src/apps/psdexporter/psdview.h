// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PSDVIEW_H
#define PSDVIEW_H

#include <psdtreeitemmodel.h>

#include <QtWidgets/QWidget>

class PsdView : public QWidget
{
    Q_OBJECT
public:
    PsdView(QWidget *parent = nullptr);
    ~PsdView() override;

    PsdTreeItemModel *model() const;

public slots:
    void setModel(PsdTreeItemModel *model);
    void setItemVisible(quint32 id, bool visible);
    void reset();
    void clearSelection();

signals:
    void itemSelected(const QModelIndex &index);
    void modelChanged(PsdTreeItemModel *model);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

#endif // PSDVIEW_H
