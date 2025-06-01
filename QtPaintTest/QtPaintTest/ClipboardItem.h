#pragma once
#include <QtWidgets>

enum class ClipboardItemType {
    Stroke,
    Raster
};

struct ClipboardItem {
    ClipboardItemType type;
    
    // For stroke items
    QPainterPath path;
    QColor color;
    qreal width;
    bool outlined;
    
    // For raster items
    QImage image;
    
    // Constructor for stroke items
    ClipboardItem(const QPainterPath& p, const QColor& c, qreal w, bool o)
        : type(ClipboardItemType::Stroke), path(p), color(c), width(w), outlined(o) {}
    
    // Constructor for raster items
    ClipboardItem(const QImage& img)
        : type(ClipboardItemType::Raster), image(img) {}
};