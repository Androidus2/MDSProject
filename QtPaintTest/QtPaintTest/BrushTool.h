#pragma once
#include "BaseTool.h"
#include <QtWidgets>
#include <clipper2/clipper.h>
#include "StrokeItem.h"
#include "DrawingEngineUtils.h"

class BrushTool : public BaseTool {
public:
	BrushTool();
	~BrushTool() override;

	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;

	QString toolName() const override { return "Brush"; }
	QIcon toolIcon() const override { return QIcon(":/icons/brush.png"); }

private slots:
	void commitBrushSegment();

private:
	void commitSegment(StrokeItem* pathItem, QGraphicsPathItem* tempItem, QPainterPath& realPath);
	QVector2D calculateTangent(int startIndex, int count);
	void optimizePath(QPainterPath& path, StrokeItem* pathItem);
	void updateTemporaryPath(QGraphicsPathItem* tempItem);

	// Brush Implementation
	void startBrushStroke(const QPointF& pos);
	void updateBrushStroke(const QPointF& pos);
	void finalizeBrushStroke();


	StrokeItem* m_currentPath = nullptr;
	QGraphicsPathItem* m_tempPathItem = nullptr;
	QPainterPath m_realPath;

	QVector<QPointF> m_points;

	// Curve optimization parameters
	QTimer m_cooldownTimer;
	int m_cooldownInterval;
	float m_tangentStrength;
};