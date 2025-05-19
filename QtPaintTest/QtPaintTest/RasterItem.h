#pragma once
#include <QtWidgets>
#include "BaseItem.h"

class RasterItem : public BaseItem {
public:
    RasterItem(const QImage& image);
    RasterItem(const QString& imagePath);
    RasterItem(const RasterItem& other);
    ~RasterItem() = default;

    // BaseItem interface implementation
    BaseItem* clone() const override;

    // QGraphicsItem interface override
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

private:
    QImage m_image;
};