#include "Layer.h"

Layer::Layer(const QString& name)
    : m_name(name)
{
}

Layer::~Layer()
{
    // Items are owned by the scene, not by the layer
}

void Layer::setName(const QString& name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged(m_name);
    }
}

void Layer::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;

        // Update visibility of all items in this layer
        for (BaseItem* item : m_items) {
            item->setVisible(m_visible);
        }

        emit visibilityChanged(m_visible);
    }
}

void Layer::setLocked(bool locked)
{
    if (m_locked != locked) {
        m_locked = locked;

        // Update movable/selectable state of all items in this layer
        for (BaseItem* item : m_items) {
            /*item->setFlag(QGraphicsItem::ItemIsMovable, !m_locked);
            item->setFlag(QGraphicsItem::ItemIsSelectable, !m_locked);*/
        }

        emit lockStateChanged(m_locked);
    }
}

void Layer::setZValue(int zValue)
{
    if (m_zValue != zValue) {
        m_zValue = zValue;

        // Update z-value of all items in this layer
        for (BaseItem* item : m_items) {
            item->setZValue(m_zValue);
        }

        emit zValueChanged(m_zValue);
    }
}

void Layer::addItem(BaseItem* item)
{
    if (!m_items.contains(item)) {
        m_items.append(item);
        item->setLayer(this);
        item->setZValue(m_zValue);
        item->setVisible(m_visible);
        //item->setFlag(QGraphicsItem::ItemIsMovable, !m_locked);
        //item->setFlag(QGraphicsItem::ItemIsSelectable, !m_locked);
    }
}

void Layer::removeItem(BaseItem* item)
{
    if (m_items.removeOne(item)) {
        item->setLayer(nullptr);
    }
}