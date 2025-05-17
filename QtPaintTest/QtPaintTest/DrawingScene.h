#pragma once
#include <QtWidgets>
#include <clipper2/clipper.h>
#include <QUndoCommand>
#include <QUndoStack>
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
    qreal brushWidth() const;

    // Undo/Redo functionality
    void setUndoStack(QUndoStack* stack);

    // Reset all selection-related state (for file operations)
    void resetSelectionState();

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
        QTime timestamp;
        DrawingScene* myScene;
        QList<StrokeItem*> movedItems;
        QPointF delta;
    };

    // Clipboard operations
    void copySelection();
    void cutSelection();
    void pasteClipboard();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

public slots:
    void handleKeyPress(QKeyEvent* event);
    void handleKeyRelease(QKeyEvent* event);
    void updateSelectionUI();

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
    QList<QPainterPath> findDisconnectedComponents(const QPainterPath& complexPath);
    bool subpathsIntersect(const QPainterPath& path1, const QPainterPath& path2);

    // Fill Implementation
    void applyFill(const QPointF& pos);

    // Selection Implementation
    void startSelection(const QPointF& pos);
    void updateSelection(const QPointF& pos);
    void finalizeSelection();
    void moveSelectedItems(const QPointF& delta);
    void clearSelection();
    void highlightSelectedItems(bool highlight);

    // Simplified Transform Implementation
    void createSelectionBox();
    void removeSelectionBox();
    TransformHandleType hitTestTransformHandle(const QPointF& pos);
    void startTransform(const QPointF& pos, TransformHandleType handleType);
    void updateTransform(const QPointF& pos);
    void endTransform();

    void rotateSelection(qreal angle);
    void scaleSelection(qreal sx, qreal sy, const QPointF& fixedPoint);
    void applyTransformToItems();

    // Command helper function
    void pushCommand(QUndoCommand* command);

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
    QPointF m_lastSceneMousePos; // Store last known mouse position for paste operation
    QMap<int, bool> m_keysPressed;
    QElapsedTimer m_keyPressTimer;
    int m_moveSpeed = 1;

    // Undo stack reference
    QUndoStack* m_undoStack = nullptr;
    QMap<StrokeItem*, QPointF> m_startPositions;


    // Clipboard data structure
    struct ClipboardItem {
        QPainterPath path = QPainterPath();
        QColor color = Qt::black;
        qreal width = 1.0;
        bool outlined = false;
    };
    QList<ClipboardItem> m_clipboard;

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