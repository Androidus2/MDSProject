#pragma once
#include "BaseTool.h"
#include <QtWidgets>
#include <clipper2/clipper.h>
#include "StrokeItem.h"
#include "DrawingEngineUtils.h"

class EraserTool : public BaseTool {
public:
	EraserTool();
	~EraserTool() override;

	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;

	QString toolName() const override { return "Eraser"; }
	QIcon toolIcon() const override { return QIcon(":/icons/eraser.png"); }

private slots:
	void commitEraserSegment();

private:
	void commitSegment(StrokeItem* pathItem, QGraphicsPathItem* tempItem, QPainterPath& realPath);
	QVector2D calculateTangent(int startIndex, int count);
	void optimizePath(QPainterPath& path, StrokeItem* pathItem);
	void updateTemporaryPath(QGraphicsPathItem* tempItem);

	// Eraser Implementation
	void startEraserStroke(const QPointF& pos);
	void updateEraserStroke(const QPointF& pos);
	void finalizeEraserStroke();
	void processEraserOnStroke(StrokeItem* stroke, const Clipper2Lib::Path64& eraserPath);
	QList<QPainterPath> findDisconnectedComponents(const QPainterPath& complexPath);
	bool subpathsIntersect(const QPainterPath& path1, const QPainterPath& path2);


	StrokeItem* m_currentEraserPath = nullptr;
	QGraphicsPathItem* m_tempEraserPathItem = nullptr;
	QPainterPath m_eraserRealPath;

	QVector<QPointF> m_points;

	// Curve optimization parameters
	QTimer m_cooldownTimer;
	int m_cooldownInterval;
	float m_tangentStrength;
};