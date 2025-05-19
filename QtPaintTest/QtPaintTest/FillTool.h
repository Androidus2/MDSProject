#pragma once
#include "BaseTool.h"

class DrawingScene;

class FillTool : public BaseTool {
public:
	FillTool();
	~FillTool() override;
	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;
	QString toolName() const override { return "Fill"; }
	QIcon toolIcon() const override { return QIcon("icons/bucket.png"); }

private:
	void applyFill(const QPointF& pos);
};