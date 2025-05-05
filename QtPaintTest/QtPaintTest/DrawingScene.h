#pragma once
#include <QtWidgets>
#include <clipper2/clipper.h>
#include "DrawingEngineUtils.h"
#include "StrokeItem.h"

enum TransformHandleType {
    HandleNone = -1,
    HandleTopLeft,
    HandleTopRight,
    HandleBottomRight,
    HandleBottomLeft,
    HandleTop,
    HandleRight,
    HandleBottom,
    HandleLeft,
    HandleRotation
};

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

    // Reset all selection-related state (for file operations)
    void resetSelectionState();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void commitBrushSegment();

private:
    // Basic drawing functionality
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

    // Selection Implementation
    void startSelection(const QPointF& pos);
    void updateSelection(const QPointF& pos);
    void finalizeSelection();
    void moveSelectedItems(const QPointF& delta);
    void clearSelection();
    void highlightSelectedItems(bool highlight);

    // NEW: Simplified Transform Implementation
    void createSelectionBox();
    void removeSelectionBox();
    TransformHandleType hitTestTransformHandle(const QPointF& pos);
    void startTransform(const QPointF& pos, TransformHandleType handleType);
    void updateTransform(const QPointF& pos);
    void endTransform();

    void rotateSelection(qreal angle);
    void scaleSelection(qreal sx, qreal sy, const QPointF& fixedPoint);
    void applyTransformToItems();

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

    // Selection and movement variables
    QList<StrokeItem*> m_selectedItems;
    QGraphicsRectItem* m_selectionRect = nullptr;
    QPointF m_selectionStartPos;
    bool m_isSelecting = false;
    bool m_isMovingSelection = false;
    QPointF m_lastMousePos;
    QMap<int, bool> m_keysPressed;
    QElapsedTimer m_keyPressTimer;
    int m_moveSpeed = 1;

    struct {
        QGraphicsRectItem* box = nullptr;
        QList<QGraphicsItem*> handles;
        QGraphicsEllipseItem* rotationHandle = nullptr;
        QGraphicsLineItem* rotationLine = nullptr;
        QGraphicsEllipseItem* centerPoint = nullptr;

        TransformHandleType activeHandle = HandleNone;
        QPointF startPos;
        QPointF center;
        QRectF initialBounds;
        qreal startAngle = 0;
        bool isTransforming = false;

        struct ItemState {
            QPointF pos;
            QTransform transform;
            QPainterPath originalPath;
        };
        QMap<StrokeItem*, ItemState> itemStates;
    } m_transform;
};