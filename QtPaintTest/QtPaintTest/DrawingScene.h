#pragma once
#include <QtWidgets>
#include <clipper2/clipper.h>
#include <QUndoCommand>
#include <QUndoStack>
#include <QList>

#include "DrawingEngineUtils.h"
#include "StrokeItem.h"

// Forward declarations
class QUndoStack;
class StrokeItem;

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

    void setUndoStack(QUndoStack* stack);

    // --- Command Classes for Undo/Redo ---
    class AddCommand : public QUndoCommand {
    public:
        AddCommand(DrawingScene* scene, StrokeItem* item, QUndoCommand* parent = nullptr);
        ~AddCommand();
        void undo() override;
        void redo() override;
    private:
        DrawingScene* myScene;
        StrokeItem* myItem;
        bool firstExecution;
    };

    class RemoveCommand : public QUndoCommand {
    public:
        // Used primarily for the inverse of AddCommand during undo
        RemoveCommand(DrawingScene* scene, StrokeItem* item, QUndoCommand* parent = nullptr);
        ~RemoveCommand();
        void undo() override;
        void redo() override;
    private:
        DrawingScene* myScene;
        StrokeItem* myItem;
    };

    class EraseCommand : public QUndoCommand {
    public:
        EraseCommand(DrawingScene* scene,
                     const QList<StrokeItem*>& originals,
                     const QList<StrokeItem*>& results,
                     QUndoCommand* parent = nullptr);
        ~EraseCommand();
        void undo() override;
        void redo() override;
    private:
        DrawingScene* myScene;
        QList<StrokeItem*> originalItems; // Items before erase
        QList<StrokeItem*> resultItems;   // Items after erase
        bool firstExecution;
    };

    class MoveCommand : public QUndoCommand {
    public:
        MoveCommand(DrawingScene* scene,
                    const QList<StrokeItem*>& items,
                    const QPointF& moveDelta,
                    QUndoCommand* parent = nullptr);
        ~MoveCommand();
        void undo() override;
        void redo() override;
        bool mergeWith(const QUndoCommand* other) override;
        int id() const override { return 1; } // ID for merging moves
        
        // Add a public accessor for getting number of items
        int itemCount() const { return movedItems.size(); }

    private:
        DrawingScene* myScene;
        QList<StrokeItem*> movedItems;
        QPointF delta;
    };

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

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
    QMap<int, bool> m_keysPressed;
    QElapsedTimer m_keyPressTimer;
    int m_moveSpeed = 1;

    // Selection Implementation
    void startSelection(const QPointF& pos);
    void updateSelection(const QPointF& pos);
    void finalizeSelection();
    void moveSelectedItems(const QPointF& newPos);
    void clearSelection();
    void highlightSelectedItems(bool highlight);
    void keyPressEvent(QKeyEvent* event);

    // --- Command Helper Function ---
    void pushCommand(QUndoCommand* command);

    QUndoStack* m_undoStack = nullptr; // Pointer to the undo stack
};