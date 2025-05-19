#pragma once
#include <QtWidgets>
#include <clipper2/clipper.h>
#include "DrawingEngineUtils.h"
#include "BaseItem.h"


class StrokeItem : public BaseItem {
public:
	StrokeItem(const QColor& color, qreal width);
	StrokeItem(const QColor& fillColor);
	StrokeItem(const StrokeItem& other);
	void setOutlined(bool outlined);
	void convertToFilledPath();
	QColor color() const;
	qreal width() const;
	bool isOutlined() const;
	void setSelected(bool selected) override;

	StrokeItem* clone() const override;

protected:
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

private:
	QColor m_color;
	qreal m_width;
	bool m_isOutlined;
	QPen m_originalPen;
};