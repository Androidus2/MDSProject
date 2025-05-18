#pragma once
#include <QtWidgets>

struct ClipboardItem {
    QPainterPath path = QPainterPath();
    QColor color = Qt::black;
    qreal width = 1.0;
    bool outlined = false;
};