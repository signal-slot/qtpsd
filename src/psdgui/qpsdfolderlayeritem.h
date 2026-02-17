// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDFOLDERLAYERITEM_H
#define QPSDFOLDERLAYERITEM_H

#include <QtPsdGui/qpsdabstractlayeritem.h>

QT_BEGIN_NAMESPACE

class Q_PSDGUI_EXPORT QPsdFolderLayerItem : public QPsdAbstractLayerItem
{
public:
    QPsdFolderLayerItem(int width, int height);
    QPsdFolderLayerItem(const QPsdLayerRecord &record, bool opened);
    QPsdFolderLayerItem();
    ~QPsdFolderLayerItem() override;
    Type type() const override { return Folder; }

    bool isOpened() const;
    void setIsOpened(bool opened);

    QRect artboardRect() const;
    void setArtboardRect(const QRect &rect);

    QString artboardPresetName() const;
    QColor artboardBackground() const;
    void setArtboardBackground(const QColor &color);

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDFOLDERLAYERITEM_H
