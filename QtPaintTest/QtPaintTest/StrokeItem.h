#pragma once
#include <QtWidgets>
#include <clipper2/clipper.h>
#include "DrawingEngineUtils.h"


class StrokeItem : public QGraphicsPathItem {
public:
	StrokeItem(const QColor& color, qreal width);
	void setOutlined(bool outlined);
	void convertToFilledPath();
	QColor color() const;
	qreal width() const;
	bool isOutlined() const;
	void setSelected(bool selected);
	bool isSelected() const { return m_isSelected; }

protected:
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

private:
	QColor m_color;
	qreal m_width;
	bool m_isOutlined;
	bool m_isSelected = false;
	QPen m_originalPen;
};