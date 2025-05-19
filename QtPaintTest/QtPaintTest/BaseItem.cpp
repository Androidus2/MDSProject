#include "BaseItem.h"

BaseItem::BaseItem() : m_isSelected(false) {}

void BaseItem::setSelected(bool selected) {
    m_isSelected = selected;
    update();
}