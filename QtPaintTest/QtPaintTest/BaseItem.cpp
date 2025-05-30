#include "BaseItem.h"
#include "Layer.h"

BaseItem::BaseItem() : m_isSelected(false) {}

void BaseItem::setSelected(bool selected) {
    m_isSelected = selected;
    update();
}

void BaseItem::setLayer(Layer* layer)
{
    // Remove from old layer if needed
    if (m_layer && m_layer != layer) {
        m_layer->removeItem(this);
    }

    m_layer = layer;

    // Apply the layer's properties to this item
    if (layer) {
        setZValue(layer->getZValue());
        setVisible(layer->isVisible());
        /*setFlag(QGraphicsItem::ItemIsMovable, !layer->isLocked());
        setFlag(QGraphicsItem::ItemIsSelectable, !layer->isLocked());*/
    }
}