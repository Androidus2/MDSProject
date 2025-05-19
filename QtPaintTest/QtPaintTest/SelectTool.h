#pragma once
#include "BaseTool.h"
#include "StrokeItem.h"
#include <QtWidgets>

class DrawingScene;

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

class SelectTool : public BaseTool {
public:
    SelectTool();
    ~SelectTool() override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    QString toolName() const override { return "Select"; }
    QIcon toolIcon() const override { return QIcon("icons/select.png"); }

    void resetSelectionState();

    void removeSelectionBox();
    void endTransform();

    void clearSelection();

	// Make a function to return the selected items
	QList<BaseItem*> getSelectedItems() const { return m_selectedItems; }

	void setSelectedItems(const QList<BaseItem*>& items) {
		m_selectedItems = items;
		highlightSelectedItems(true);

		if (!m_selectedItems.isEmpty()) {
			createSelectionBox();
		}
	}
public slots:
    void updateSelectionUI();

private:

    // Selection Implementation
    void startSelection(const QPointF& pos);
    void updateSelection(const QPointF& pos);
    void finalizeSelection();
    void moveSelectedItems(const QPointF& delta);
    void highlightSelectedItems(bool highlight);

    // NEW: Simplified Transform Implementation
    void createSelectionBox();
    TransformHandleType hitTestTransformHandle(const QPointF& pos);
    void startTransform(const QPointF& pos, TransformHandleType handleType);
    void updateTransform(const QPointF& pos);

    void rotateSelection(qreal angle);
    void scaleSelection(qreal sx, qreal sy, const QPointF& fixedPoint);
    void applyTransformToItems();

    // Selection and movement variables
    QList<BaseItem*> m_selectedItems;
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
        QMap<BaseItem*, ItemState> itemStates;
    } m_transform;

    // For undo stack
    QMap<BaseItem*, QPointF> m_startPositions;
};