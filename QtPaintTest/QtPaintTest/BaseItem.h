#pragma once
#include <QtWidgets>

class Layer;

class BaseItem : public QGraphicsPathItem {
public:
    BaseItem();
    virtual ~BaseItem() = default;

    virtual void setSelected(bool selected);
    bool isSelected() const { return m_isSelected; }

    virtual BaseItem* clone() const = 0;

    // Layer support
    Layer* getLayer() const { return m_layer; }
    void setLayer(Layer* layer);

protected:
    bool m_isSelected = false;
    Layer* m_layer = nullptr;
};