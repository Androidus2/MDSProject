#include "RasterItem.h"

RasterItem::RasterItem(const QImage& image) : BaseItem(), m_image(image) {
    // Create a rectangle path with the image's aspect ratio
    if (!m_image.isNull()) {
        QPainterPath path;
        QRectF rect(0, 0, m_image.width(), m_image.height());
        path.addRect(rect);
        setPath(path);
    }
}

RasterItem::RasterItem(const QString& imagePath) : BaseItem() {
    // Load image from path
    m_image.load(imagePath);
    // Create a rectangle path with the image's aspect ratio
    if (!m_image.isNull()) {
        QPainterPath path;
        QRectF rect(0, 0, m_image.width(), m_image.height());
        path.addRect(rect);
        setPath(path);
    }
}

RasterItem::RasterItem(const RasterItem& other) : BaseItem(), m_image(other.m_image) {
    // Copy the path from the other item
    setPath(other.path());
    setPos(other.pos());
    setRotation(other.rotation());
    setScale(other.scale());
    setTransform(other.transform());
    setZValue(other.zValue());
    setSelected(other.isSelected());
}

BaseItem* RasterItem::clone() const {
    return new RasterItem(*this);
}

// Override paint method to display the image on the path
void RasterItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    Q_UNUSED(widget);

    // Draw the image filling the path's bounding rect
    if (!m_image.isNull()) {
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        painter->drawImage(boundingRect(), m_image);
    }

    // Show selection outline if selected
    if (m_isSelected) {
        QPen pen(Qt::DashLine);
        pen.setColor(Qt::blue);
        pen.setWidth(2);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path());
    }
}