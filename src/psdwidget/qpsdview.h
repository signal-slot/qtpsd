// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef QPSDVIEW_H
#define QPSDVIEW_H

#include <QtPsdWidget/qpsdwidgetglobal.h>
#include <QtPsdWidget/qpsdwidgettreeitemmodel.h>

#include <QtWidgets/QGraphicsView>

QT_BEGIN_NAMESPACE

class QPsdScene;

class Q_PSDWIDGET_EXPORT QPsdView : public QGraphicsView
{
    Q_OBJECT
    Q_PROPERTY(bool showChecker READ showChecker WRITE setShowChecker NOTIFY showCheckerChanged)
public:
    QPsdView(QWidget *parent = nullptr);
    ~QPsdView() override;

    QPsdWidgetTreeItemModel *model() const;
    bool showChecker() const;

public slots:
    void setModel(QPsdWidgetTreeItemModel *model);
    void setPsdScene(QPsdScene *scene);
    void setItemVisible(quint32 id, bool visible);
    void selectItem(const QModelIndex &index);
    void reset();
    void clearSelection();
    void setShowChecker(bool showChecker);

signals:
    void itemSelected(const QModelIndex &index);
    void itemsSelected(const QModelIndexList &indexes);
    void sceneChanged(QPsdScene *scene);
    void modelChanged(QPsdWidgetTreeItemModel *model);
    void showCheckerChanged(bool show);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDVIEW_H
