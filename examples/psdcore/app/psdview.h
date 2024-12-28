// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PSDVIEW_H
#define PSDVIEW_H

#include <psdtreeitemmodel.h>

#include <QtWidgets/QWidget>
#include <QtPsdGui/QPsdFolderLayerItem>

class PsdView : public QWidget
{
    Q_OBJECT
public:
    PsdView(QWidget *parent = nullptr);
    ~PsdView() override;

public slots:
    QT_DEPRECATED
    void setPsdTree(const QPsdFolderLayerItem *root);
    void setModel(PsdTreeItemModel *model);
    void setItemVisible(quint32 id, bool visible);

signals:
    void updateText(const QPsdAbstractLayerItem *item);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

#endif // PSDVIEW_H
