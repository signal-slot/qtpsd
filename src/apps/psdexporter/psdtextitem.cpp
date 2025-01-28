// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "psdtextitem.h"

#include <QtGui/QPainter>
#include <QtGui/QWindow>

PsdTextItem::PsdTextItem(const QModelIndex &index, const QPsdTextLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QWidget *parent)
    : PsdAbstractItem(index, psdData, maskItem, group, parent)
{}

void PsdTextItem::paintEvent(QPaintEvent *event)
{
    PsdAbstractItem::paintEvent(event);

    const auto *layer = this->layer<QPsdTextLayerItem>();

    QPainter painter(this);
    // painter.drawImage(0, 0, layer->image());
    // painter.setOpacity(0.5);

    const auto runs = layer->runs();
    struct Chunk {
        QFont font;
        QColor color;
        QString text;
        Qt::Alignment alignment;
        QSizeF size;
    };

    QList<QList<Chunk>> lines;
    QList<Chunk> currentLine;
    for (const auto &run : runs) {
        bool isFirst = true;
        if (layer->name() == u"カテゴリ一覧"_s) {
            qDebug() << this << run.text;
        }

        for (const QString &line : run.text.split("\n"_L1)) {
            if (isFirst) {
                isFirst = false;
            } else {
                lines.append(currentLine);
                currentLine.clear();
            }
            Chunk chunk;
            chunk.font = run.font;
            chunk.font.setPointSizeF(chunk.font.pointSizeF() / 1.5);
            chunk.color = run.color;
            chunk.text = line;
            if (layer->name() == u"カテゴリ一覧"_s) {
                qDebug() << line;
            }
            chunk.alignment = run.alignment;
            painter.setFont(chunk.font);
            chunk.size = painter.boundingRect(QRectF(0, 0, width(), height()), run.alignment | Qt::AlignHCenter, line).size();
            currentLine.append(chunk);
        }
    }
    if (!currentLine.isEmpty()) {
        lines.append(currentLine);
    }

    qreal contentHeight = 0;
    for (const auto &line : lines) {
        qreal maxHeight = 0;
        for (const auto &chunk : line) {
            const auto size = chunk.size;
            if (size.height() > maxHeight)
                maxHeight = size.height();
        }
        contentHeight += maxHeight;
    }

    qreal y = (height() - contentHeight) / 2;
    for (const auto &line : lines) {
        QSizeF size;
        for (const auto &chunk : line) {
            const auto w = chunk.size.width();
            const auto h = chunk.size.height();
            if (size.isEmpty()) {
                size = QSize(w, h);
            } else {
                size.setWidth(size.width() + w);
                if (h > size.height())
                    size.setHeight(h);
            }
        }
        qreal x = (width() - size.width()) / 2;
        for (const auto &chunk : line) {
            painter.setFont(chunk.font);
            painter.setPen(chunk.color);
            // qDebug() << chunk.text << chunk.size << chunk.alignment;
            painter.drawText(x, y, chunk.size.width(), size.height(), chunk.alignment | Qt::AlignHCenter, chunk.text);
            x += chunk.size.width();
        }
        y += size.height();
    }
}
