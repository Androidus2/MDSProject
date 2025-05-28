#include "RasterItem.h"

RasterItem::RasterItem(const QImage& image) : BaseItem(), m_image(image) {
    // Create a rectangle path with the image's aspect ratio, centered at origin
    if (!m_image.isNull()) {
        QPainterPath path;
        QRectF rect(-m_image.width()/2, -m_image.height()/2, 
                    m_image.width(), m_image.height());
        path.addRect(rect);
        setPath(path);
    }
}

RasterItem::RasterItem(const QString& imagePath) : BaseItem() {
    // Load image from path
    m_image.load(imagePath);
    // Create a rectangle path with the image's aspect ratio, centered at origin
    if (!m_image.isNull()) {
        QPainterPath path;
        QRectF rect(-m_image.width()/2, -m_image.height()/2, 
                    m_image.width(), m_image.height());
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
    Q_UNUSED(option);

    // Draw the image at the origin of the item's coordinate system
    if (!m_image.isNull()) {
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        
        // Draw the image centered at the origin to ensure proper rotation
        QRectF targetRect(-m_image.width()/2, -m_image.height()/2, 
                          m_image.width(), m_image.height());
        painter->drawImage(targetRect, m_image);
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