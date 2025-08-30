// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef QPSDSCENE_H
#define QPSDSCENE_H

#include <QtPsdWidget/qpsdwidgetglobal.h>

#include <QtWidgets/QGraphicsScene>

QT_BEGIN_NAMESPACE

class QPsdWidgetTreeItemModel;

class Q_PSDWIDGET_EXPORT QPsdScene : public QGraphicsScene
{
    Q_OBJECT

public:
    QPsdScene(QObject *parent = nullptr);
    virtual ~QPsdScene();

    QPsdWidgetTreeItemModel *model() const;
    bool showChecker() const;

public slots:
    void setModel(QPsdWidgetTreeItemModel *model);
    void setItemVisible(quint32 id, bool visible);
    void reset();
    void setShowChecker(bool show);

signals:
    void itemSelected(const QModelIndex &index);
    void modelChanged(QPsdWidgetTreeItemModel *model);
    void showCheckerChanged(bool show);

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDSCENE_H
