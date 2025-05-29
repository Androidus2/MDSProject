#pragma once
#include "BaseTool.h"
#include <QtWidgets>
#include <clipper2/clipper.h>
#include "StrokeItem.h"
#include "DrawingEngineUtils.h"
#include <deque>

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
    QIcon toolIcon() const override { return QIcon("icons/brush.png"); }

private slots:
    void commitBrushSegment();

private:
    void commitSegment(StrokeItem* pathItem, QGraphicsPathItem* tempItem, QPainterPath& realPath);
    QVector2D calculateTangent(int startIndex, int count);
    void optimizePath(QPainterPath& path, StrokeItem* pathItem);
    void updateTemporaryPath(QGraphicsPathItem* tempItem, const QPointF& cursorPos);
    void ensureCursorIndicator(); // New method to manage cursor indicator

    // Brush Implementation
    void startBrushStroke(const QPointF& pos);
    void updateBrushStroke(const QPointF& pos);
    void finalizeBrushStroke();

    // Smoothing function
    QPointF smoothPoint(const QPointF& newPoint);

    StrokeItem* m_currentPath = nullptr;
    QGraphicsPathItem* m_tempPathItem = nullptr;
    QGraphicsEllipseItem* m_cursorIndicator = nullptr; // Visual cursor indicator
    QPainterPath m_realPath;

    QVector<QPointF> m_points;
    std::deque<QPointF> m_pointHistory; // For input smoothing
    QPointF m_lastCursorPos; // Tracks actual cursor position

    // Curve parameters
    QTimer m_cooldownTimer;
    int m_cooldownInterval;
    float m_tangentStrength;
    float m_smoothingFactor;
    int m_smoothingWindowSize;
    float m_minDistance;
    bool m_preserveCurves;
    bool m_showPrediction;
};