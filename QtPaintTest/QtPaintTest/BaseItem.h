#pragma once
#include <QtWidgets>

class BaseItem : public QGraphicsPathItem {
public:
	BaseItem();
	virtual ~BaseItem() = default;

	virtual void setSelected(bool selected);
	bool isSelected() const { return m_isSelected; }

	virtual BaseItem* clone() const = 0;

protected:
	bool m_isSelected = false;
	// For the future, if layers are implemented, they should be handled here
};