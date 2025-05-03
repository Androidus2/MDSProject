#pragma once
#include <QtWidgets>
#include <clipper2/clipper.h>
#include "DrawingEngineUtils.h"
#include "StrokeItem.h"

class DrawingScene : public QGraphicsScene {
    Q_OBJECT
public:
    DrawingScene(QObject* parent = nullptr);
    ~DrawingScene();

    void setTool(ToolType tool);

	// Color getter and setter
    void setColor(const QColor& color);
    QColor currentColor() const;

	// Brush width setter
    void setBrushWidth(qreal width);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

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

    // Eraser Implementation
    void startEraserStroke(const QPointF& pos);
    void updateEraserStroke(const QPointF& pos);
    void finalizeEraserStroke();
    void processEraserOnStroke(StrokeItem* stroke, const Clipper2Lib::Path64& eraserPath);

    // Fill Implementation
    void applyFill(const QPointF& pos);

    // Member variables
    ToolType m_currentTool;
    QColor m_brushColor;
    qreal m_brushWidth;

    // Path-related variables - separate for brush and eraser
    StrokeItem* m_currentPath = nullptr;
    QGraphicsPathItem* m_tempPathItem = nullptr;
    QPainterPath m_realPath;

    StrokeItem* m_currentEraserPath = nullptr;
    QGraphicsPathItem* m_tempEraserPathItem = nullptr;
    QPainterPath m_eraserRealPath;

    QVector<QPointF> m_points;

    // Curve optimization parameters
    QTimer m_cooldownTimer;
    int m_cooldownInterval;
    float m_tangentStrength;

    // Selection tool variables
    QList<StrokeItem*> m_selectedItems;
    QGraphicsRectItem* m_selectionRect = nullptr;
    QPointF m_selectionStartPos;
    bool m_isSelecting = false;
    bool m_isMovingSelection = false;
    QPointF m_lastMousePos;

    // Selection Implementation
    void startSelection(const QPointF& pos);
    void updateSelection(const QPointF& pos);
    void finalizeSelection();
    void moveSelectedItems(const QPointF& newPos);
    void clearSelection();
    void highlightSelectedItems(bool highlight);
    void keyPressEvent(QKeyEvent* event);
};