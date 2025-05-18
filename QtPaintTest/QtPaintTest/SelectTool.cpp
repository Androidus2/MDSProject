#include "SelectTool.h"
#include "DrawingScene.h"
#include "DrawingManager.h"
#include "MoveCommand.h"
#include "RemoveCommand.h"

SelectTool::SelectTool() {
    // Initialize key tracking
    m_moveSpeed = 1;

    // Initialize transform state
    m_transform.isTransforming = false;
    m_transform.activeHandle = HandleNone;
}
SelectTool::~SelectTool() {
    // Clean up selection rectangle
    if (m_selectionRect) {
        delete m_selectionRect;
        m_selectionRect = nullptr;
    }

    // Clean up transform handles
    removeSelectionBox();
}

void SelectTool::mousePressEvent(QGraphicsSceneMouseEvent* event) {
	if (event->button() != Qt::LeftButton) return;

    TransformHandleType handleType = hitTestTransformHandle(event->scenePos());
    if (handleType != HandleNone) {
        // Start transform operation
        startTransform(event->scenePos(), handleType);
        event->accept();
        return;
    }

	// Start selection or move operation
	startSelection(event->scenePos());
	event->accept();
}
void SelectTool::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    // Handle transform operations
    if (m_transform.isTransforming) {
        updateTransform(event->scenePos());
        event->accept();
        return;
    }

	if (event->buttons() & Qt::LeftButton) {
		// Update selection or move operation
		updateSelection(event->scenePos());
		event->accept();
	}
}
void SelectTool::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    // Handle transform operations
    if (m_transform.isTransforming) {
        endTransform();
        event->accept();
        return;
    }

	if (event->button() == Qt::LeftButton) {
		// Finalize selection or move operation
		finalizeSelection();
		event->accept();
	}
}

void SelectTool::keyPressEvent(QKeyEvent* event) {
    if (!m_selectedItems.isEmpty()) {
        int key = event->key();

        // Check if this is a new key press
        bool isNewKeyPress = !m_keysPressed.contains(key) || !m_keysPressed[key];

        // Store the key press state
        if (isNewKeyPress) {
            m_keysPressed[key] = true;

            // If this is the first arrow key press, store starting positions
            bool isArrowKey = (key == Qt::Key_Left || key == Qt::Key_Right ||
                key == Qt::Key_Up || key == Qt::Key_Down);

            if (isArrowKey && m_startPositions.isEmpty()) {
                for (StrokeItem* selectedItem : m_selectedItems) {
                    m_startPositions[selectedItem] = selectedItem->pos();
                }
            }
        }

        // Start or restart timer for arrow keys with any arrow press
        bool anyArrowPressed = m_keysPressed.value(Qt::Key_Left) ||
            m_keysPressed.value(Qt::Key_Right) ||
            m_keysPressed.value(Qt::Key_Up) ||
            m_keysPressed.value(Qt::Key_Down);

        if (anyArrowPressed) {
            // Ensure timer is running whenever arrows are pressed
            if (!m_keyPressTimer.isValid()) {
                m_keyPressTimer.start();
                m_moveSpeed = 1;
            }

            // Calculate acceleration based on time since first arrow press
            if (m_keyPressTimer.elapsed() > 300) {
                // More responsive acceleration curve
                int elapsedMs = m_keyPressTimer.elapsed();
                if (elapsedMs > 2000) {
                    m_moveSpeed = 10; // Maximum speed
                }
                else if (elapsedMs > 1500) {
                    m_moveSpeed = 7;
                }
                else if (elapsedMs > 1000) {
                    m_moveSpeed = 5;
                }
                else if (elapsedMs > 600) {
                    m_moveSpeed = 3;
                }
                else {
                    m_moveSpeed = 2;
                }
            }
        }

        int dx = 0;
        int dy = 0;

        // Calculate movement based on which keys are pressed
        if (m_keysPressed.value(Qt::Key_Left)) dx -= m_moveSpeed;
        if (m_keysPressed.value(Qt::Key_Right)) dx += m_moveSpeed;
        if (m_keysPressed.value(Qt::Key_Up)) dy -= m_moveSpeed;
        if (m_keysPressed.value(Qt::Key_Down)) dy += m_moveSpeed;

        if (dx != 0 || dy != 0) {
            // Apply the movement
            for (StrokeItem* item : m_selectedItems) {
                item->moveBy(dx, dy);
            }

            // Prevent event from propagating to parent (stops canvas movement)
            event->accept();
            return;
        }

        // Handle delete key for selected items
        if (key == Qt::Key_Delete) {
            QList<StrokeItem*> itemsToRemove = m_selectedItems;
            /*for (StrokeItem* item : m_selectedItems) {
                DrawingManager::getInstance().getScene()->removeItem(item);
                delete item;
            }*/
            for (StrokeItem* item : itemsToRemove) {
                RemoveCommand* cmd = new RemoveCommand(DrawingManager::getInstance().getScene(), item);
                DrawingManager::getInstance().pushCommand(cmd);
            }
            // Clean up selection UI elements
            //removeSelectionBox();
            m_selectedItems.clear();
            event->accept();
            return;
        }
    }

    //QGraphicsScene::keyPressEvent(event);
}
void SelectTool::keyReleaseEvent(QKeyEvent* event) {
    int key = event->key();
    bool wasArrowKey = (key == Qt::Key_Left || key == Qt::Key_Right ||
        key == Qt::Key_Up || key == Qt::Key_Down);

    // Mark key as released
    if (m_keysPressed.contains(key)) {
        m_keysPressed[key] = false;

        // If an arrow key was released and we have starting positions,
        // and there are no more arrow keys pressed, create a move command
        if (wasArrowKey && !m_startPositions.isEmpty()) {
            bool anyArrowStillPressed =
                m_keysPressed.value(Qt::Key_Left) ||
                m_keysPressed.value(Qt::Key_Right) ||
                m_keysPressed.value(Qt::Key_Up) ||
                m_keysPressed.value(Qt::Key_Down);

            if (!anyArrowStillPressed) {
                // Create move command for the keyboard movement
                QList<StrokeItem*> movedItems;
                QPointF totalDelta;

                // Calculate the average delta from start positions
                int itemCount = 0;
                for (StrokeItem* item : m_selectedItems) {
                    if (m_startPositions.contains(item)) {
                        QPointF startPos = m_startPositions[item];
                        QPointF endPos = item->pos();
                        QPointF itemDelta = endPos - startPos;

                        // Only consider items that moved
                        if (itemDelta.manhattanLength() > 0.01) {
                            movedItems.append(item);
                            totalDelta += itemDelta;
                            itemCount++;
                        }
                    }
                }

                // If any items moved, create a command
                if (!movedItems.isEmpty()) {
                    totalDelta /= itemCount; // Average delta
                    MoveCommand* cmd = new MoveCommand(DrawingManager::getInstance().getScene(), movedItems, totalDelta);

                    // Move items back to start positions first
                    for (StrokeItem* item : movedItems) {
                        item->setPos(m_startPositions[item]);
                    }

                    // Push command to apply the move
                    DrawingManager::getInstance().pushCommand(cmd);
                }

                // Clear start positions
                m_startPositions.clear();
            }
        }

        // Only reset the timer if all arrow keys are released
        // AND there's no new arrow key pressed within a short timeframe
        if (!m_keysPressed.value(Qt::Key_Left) &&
            !m_keysPressed.value(Qt::Key_Right) &&
            !m_keysPressed.value(Qt::Key_Up) &&
            !m_keysPressed.value(Qt::Key_Down)) {

            // Use a small delay before actually invalidating the timer
            // This allows for quick direction changes without resetting acceleration
            QTimer::singleShot(50, this, [this]() {
                // Check again after delay to make sure no new arrow key was pressed
                if (!m_keysPressed.value(Qt::Key_Left) &&
                    !m_keysPressed.value(Qt::Key_Right) &&
                    !m_keysPressed.value(Qt::Key_Up) &&
                    !m_keysPressed.value(Qt::Key_Down)) {
                    m_keyPressTimer.invalidate();
                    m_moveSpeed = 1;
                }
                });
        }
    }

    //QGraphicsScene::keyReleaseEvent(event);
}


void SelectTool::updateSelectionUI() {
    if (!m_selectedItems.isEmpty()) {
        removeSelectionBox();
        createSelectionBox();
        highlightSelectedItems(true);
    }
    else {
        removeSelectionBox();
    }
}


void SelectTool::startSelection(const QPointF& pos) {
    // If clicking on a selected item, start moving
    QList<QGraphicsItem*> itemsAtPos = DrawingManager::getInstance().getScene()-> items(pos);

    bool clickedOnAnyItem = false;
    bool clickedOnSelected = false;

    for (QGraphicsItem* item : itemsAtPos) {
        if (auto stroke = dynamic_cast<StrokeItem*>(item)) {
            // Skip items that are part of onion skin groups
            if (stroke->parentItem()) {
                continue;
            }

            clickedOnAnyItem = true;

            if (m_selectedItems.contains(stroke)) {
                clickedOnSelected = true;
                m_isMovingSelection = true;
                m_lastMousePos = pos;

                // Store starting positions of all selected items
                m_startPositions.clear();
                for (StrokeItem* selectedItem : m_selectedItems) {
                    m_startPositions[selectedItem] = selectedItem->pos();
                }
                break;
            }
            else if (!(QApplication::keyboardModifiers() & Qt::ShiftModifier)) {
                // If clicking on an unselected item without Shift, select only this item
                clearSelection();
                m_selectedItems.append(stroke);
                highlightSelectedItems(true);
                m_isMovingSelection = true;
                m_lastMousePos = pos;

                // Store starting position
                m_startPositions.clear();
                m_startPositions[stroke] = stroke->pos();
                return;
            }
            else {
                // If clicking with Shift, add this item to the selection
                m_selectedItems.append(stroke);
                highlightSelectedItems(true);
                m_isMovingSelection = true;
                m_lastMousePos = pos;

                // Store starting positions of all selected items
                m_startPositions.clear();
                for (StrokeItem* selectedItem : m_selectedItems) {
                    m_startPositions[selectedItem] = selectedItem->pos();
                }
                return;
            }
        }
    }

    // If not clicking on any item, start new selection rectangle (or clear selection)
    if (!clickedOnAnyItem) {
        // Clear previous selection if not using Shift
        if (!(QApplication::keyboardModifiers() & Qt::ShiftModifier)) {
            clearSelection();
        }

        // Start new selection rectangle
        m_isSelecting = true;
        m_selectionStartPos = pos;

        if (!m_selectionRect) {
            m_selectionRect = new QGraphicsRectItem();
            m_selectionRect->setPen(QPen(Qt::DashLine));
            m_selectionRect->setBrush(QBrush(QColor(0, 0, 255, 30)));
            DrawingManager::getInstance().getScene()-> addItem(m_selectionRect);
        }

        m_selectionRect->setRect(QRectF(pos, QSizeF(0, 0)));
        m_selectionRect->show();
    }
}
void SelectTool::updateSelection(const QPointF& pos) {
    if (m_isSelecting && m_selectionRect) {
        // Update selection rectangle
        QRectF rect = QRectF(
            QPointF(qMin(m_selectionStartPos.x(), pos.x()), qMin(m_selectionStartPos.y(), pos.y())),
            QPointF(qMax(m_selectionStartPos.x(), pos.x()), qMax(m_selectionStartPos.y(), pos.y()))
        );
        m_selectionRect->setRect(rect);
    }
    else if (m_isMovingSelection) {
        // Move selected items
        QPointF delta = pos - m_lastMousePos;
        if (!m_selectedItems.isEmpty() && delta.manhattanLength() > 0.01) {
            // Just update the visual positions during dragging - no commands yet
            for (StrokeItem* item : m_selectedItems) {
                if (item->scene() == DrawingManager::getInstance().getScene()) {
                    item->moveBy(delta.x(), delta.y());
                }
            }
            m_lastMousePos = pos;
        }
        else if (!m_selectedItems.isEmpty()) {
            m_lastMousePos = pos;
        }
    }
}
void SelectTool::finalizeSelection() {
    if (m_isSelecting && m_selectionRect) {
        // Items within selection rectangle
        QList<QGraphicsItem*> itemsInRect = DrawingManager::getInstance().getScene()-> items(m_selectionRect->rect());

        for (QGraphicsItem* item : itemsInRect) {
            if (auto stroke = dynamic_cast<StrokeItem*>(item)) {
                // Only select items that aren't part of onion skin groups
                if (!stroke->parentItem() && !m_selectedItems.contains(stroke)) {
                    m_selectedItems.append(stroke);
                }
            }
        }

        // Hide selection rectangle
        m_selectionRect->hide();
        m_isSelecting = false;

        // Highlight selected items
        highlightSelectedItems(true);

        // Create transform handles
        if (!m_selectedItems.isEmpty()) {
            createSelectionBox();
        }
    }
    else if (m_isMovingSelection) {
        // Create a final move command for the entire movement
        if (!m_selectedItems.isEmpty() && !m_startPositions.isEmpty()) {
            // Calculate final move delta for each item
            QList<StrokeItem*> movedItems;
            QPointF totalDelta;

            // Calculate the average delta of all moved items
            int itemCount = 0;
            for (StrokeItem* item : m_selectedItems) {
                if (m_startPositions.contains(item)) {
                    QPointF startPos = m_startPositions[item];
                    QPointF endPos = item->pos();
                    QPointF itemDelta = endPos - startPos;

                    // Only consider items that actually moved
                    if (itemDelta.manhattanLength() > 0.01) {
                        movedItems.append(item);
                        totalDelta += itemDelta;
                        itemCount++;
                    }
                }
            }

            // If any items were moved, create a command
            if (!movedItems.isEmpty()) {
                totalDelta /= itemCount; // Average delta
                MoveCommand* cmd = new MoveCommand(DrawingManager::getInstance().getScene(), movedItems, totalDelta);

                // Important: Move items back to start positions first
                for (StrokeItem* item : movedItems) {
                    item->setPos(m_startPositions[item]);
                }

                // Then push command (which will call redo() and apply the move)
                DrawingManager::getInstance().pushCommand(cmd);
            }

            m_startPositions.clear();
        }

        m_isMovingSelection = false;

        // Create transform handles
        if (!m_selectedItems.isEmpty()) {
            createSelectionBox();
        }
    }
}

void SelectTool::moveSelectedItems(const QPointF& delta) {
    if (m_selectedItems.isEmpty()) return;

    for (auto item : m_selectedItems) {
        item->moveBy(delta.x(), delta.y());
    }

    // Update selection box
    if (m_transform.box) {
        removeSelectionBox();
        createSelectionBox();
    }
}
void SelectTool::clearSelection() {
    highlightSelectedItems(false);
    m_selectedItems.clear();
    removeSelectionBox();
}
void SelectTool::highlightSelectedItems(bool highlight) {
    for (StrokeItem* item : m_selectedItems) {
        if (highlight) {
            item->setSelected(true);
            item->setZValue(item->zValue() + 0.1);
        }
        else {
            item->setSelected(false);
            item->setZValue(item->zValue() - 0.1);
        }
    }
}

void SelectTool::resetSelectionState() {
    // Clear selection
    clearSelection();

    // End any ongoing transform
    if (m_transform.isTransforming) {
        endTransform();
    }

    // Remove selection box
    removeSelectionBox();

    // Reset selection rectangle
    if (m_selectionRect) {
        DrawingManager::getInstance().getScene()-> removeItem(m_selectionRect);
        delete m_selectionRect;
        m_selectionRect = nullptr;
    }

    // Reset flags
    m_isSelecting = false;
    m_isMovingSelection = false;

    // Clear key tracking
    m_keysPressed.clear();
    m_keyPressTimer.invalidate();
    m_moveSpeed = 1;

    // Clear transform state
    m_transform.isTransforming = false;
    m_transform.activeHandle = HandleNone;
    m_transform.itemStates.clear();
}

// Create selection box with transform handles
void SelectTool::createSelectionBox() {
    // Remove any existing selection box
    removeSelectionBox();

    if (m_selectedItems.isEmpty()) {
        return;
    }

    // Calculate bounds of all selected items
    QRectF bounds;
    for (auto item : m_selectedItems) {
        QPainterPath path = item->path();
        QTransform t = item->transform();
        QPainterPath mappedPath = t.map(path);
        mappedPath.translate(item->pos());

        if (bounds.isNull()) {
            bounds = mappedPath.boundingRect();
        }
        else {
            bounds = bounds.united(mappedPath.boundingRect());
        }
    }

    if (bounds.isNull()) {
        return;
    }

    m_transform.initialBounds = bounds;
    m_transform.center = bounds.center();

    // Create selection box
    m_transform.box = new QGraphicsRectItem(bounds);
    m_transform.box->setPen(QPen(Qt::blue, 1, Qt::DashLine));
    m_transform.box->setZValue(999);
    DrawingManager::getInstance().getScene()-> addItem(m_transform.box);

    // Create handles
    qreal handleSize = 8;
    QPen handlePen(Qt::blue);
    QBrush handleBrush(Qt::white);

    // Function to create a handle
    auto createRectHandle = [&](const QPointF& pos) -> QGraphicsRectItem* {
        QGraphicsRectItem* handle = new QGraphicsRectItem(
            QRectF(pos.x() - handleSize / 2, pos.y() - handleSize / 2, handleSize, handleSize));
        handle->setPen(handlePen);
        handle->setBrush(handleBrush);
        handle->setZValue(1000);
        DrawingManager::getInstance().getScene()-> addItem(handle);
        m_transform.handles.append(handle);
        return handle;
        };

    // Corner handles
    createRectHandle(bounds.topLeft());     // HandleTopLeft
    createRectHandle(bounds.topRight());    // HandleTopRight
    createRectHandle(bounds.bottomRight()); // HandleBottomRight
    createRectHandle(bounds.bottomLeft());  // HandleBottomLeft

    // Edge handles
    createRectHandle(QPointF(bounds.center().x(), bounds.top()));    // HandleTop
    createRectHandle(QPointF(bounds.right(), bounds.center().y()));  // HandleRight
    createRectHandle(QPointF(bounds.center().x(), bounds.bottom())); // HandleBottom
    createRectHandle(QPointF(bounds.left(), bounds.center().y()));   // HandleLeft

    // Rotation handle
    QPointF rotHandlePos(bounds.center().x(), bounds.center().y() - 30); // Corrected position
    m_transform.rotationHandle = new QGraphicsEllipseItem(
        QRectF(rotHandlePos.x() - handleSize / 2, rotHandlePos.y() - handleSize / 2, handleSize, handleSize));
    m_transform.rotationHandle->setPen(handlePen);
    m_transform.rotationHandle->setBrush(Qt::green);
    m_transform.rotationHandle->setZValue(1000);
    DrawingManager::getInstance().getScene()-> addItem(m_transform.rotationHandle);
    m_transform.handles.append(m_transform.rotationHandle);

    // Line from center to rotation handle
    m_transform.rotationLine = new QGraphicsLineItem(
        QLineF(bounds.center(), rotHandlePos));
    m_transform.rotationLine->setPen(QPen(Qt::blue, 1, Qt::DashLine));
    m_transform.rotationLine->setZValue(999);
    DrawingManager::getInstance().getScene()-> addItem(m_transform.rotationLine);
    m_transform.handles.append(m_transform.rotationLine);

    // Center point
    m_transform.centerPoint = new QGraphicsEllipseItem(
        QRectF(bounds.center().x() - 3, bounds.center().y() - 3, 6, 6));
    m_transform.centerPoint->setPen(QPen(Qt::red));
    m_transform.centerPoint->setBrush(Qt::red);
    m_transform.centerPoint->setZValue(1001);
    DrawingManager::getInstance().getScene()-> addItem(m_transform.centerPoint);
    m_transform.handles.append(m_transform.centerPoint);
}
// Remove selection box and handles
void SelectTool::removeSelectionBox() {
    if (m_transform.box) {
        DrawingManager::getInstance().getScene()-> removeItem(m_transform.box);
        delete m_transform.box;
        m_transform.box = nullptr;
    }

    for (auto handle : m_transform.handles) {
        DrawingManager::getInstance().getScene()-> removeItem(handle);
        delete handle;
    }
    m_transform.handles.clear();
    m_transform.rotationHandle = nullptr;
    m_transform.rotationLine = nullptr;
    m_transform.centerPoint = nullptr;
}

// Test if a point hits a transform handle and return the handle type
TransformHandleType SelectTool::hitTestTransformHandle(const QPointF& pos) {
    if (!m_transform.box) {
        return HandleNone;
    }

    // Check rotation handle first
    if (m_transform.rotationHandle && m_transform.rotationHandle->contains(
        m_transform.rotationHandle->mapFromScene(pos))) {
        return HandleRotation;
    }

    // Check other handles
    for (int i = 0; i < 8 && i < m_transform.handles.size(); i++) {
        QGraphicsItem* handle = m_transform.handles[i];
        if (handle && handle->contains(handle->mapFromScene(pos))) {
            return static_cast<TransformHandleType>(i);
        }
    }

    return HandleNone;
}

// Start a transform operation
void SelectTool::startTransform(const QPointF& pos, TransformHandleType handleType) {
    m_transform.activeHandle = handleType;
    m_transform.startPos = pos;
    m_transform.isTransforming = true;

    // Make sure we store the current bounds as reference
    if (m_transform.box) {
        m_transform.initialBounds = m_transform.box->rect();
    }

    // Store original state of all selected items
    m_transform.itemStates.clear();
    for (auto item : m_selectedItems) {
        m_transform.itemStates[item] = {
            item->pos(),
            item->transform(),
            item->path()
        };
    }

    // For rotation, store starting angle
    if (handleType == HandleRotation) {
        QLineF line(m_transform.center, pos);
        m_transform.startAngle = line.angle();
    }
}

// Update transform during drag
void SelectTool::updateTransform(const QPointF& pos) {
    if (!m_transform.isTransforming) return;

    QPointF delta = pos - m_transform.startPos;

    // Handle rotation
    if (m_transform.activeHandle == HandleRotation) {
        QLineF line(m_transform.center, pos);
        qreal currentAngle = line.angle();
        qreal angleDelta = currentAngle - m_transform.startAngle;

        // Apply rotation
        rotateSelection(angleDelta);

        // Update rotation line position (handle stays in place)
        if (m_transform.rotationLine) {
            m_transform.rotationLine->setLine(QLineF(m_transform.center, pos));
        }

        // Store new start angle
        m_transform.startAngle = currentAngle;
        return;
    }

    // Handle scaling
    QPointF fixedPoint;
    qreal sx = 1.0, sy = 1.0;

    // Determine fixed point and scale factors based on handle type
    QRectF bounds = m_transform.initialBounds;

    switch (m_transform.activeHandle) {
    case HandleTopLeft:
        fixedPoint = bounds.bottomRight();
        if (pos.x() < fixedPoint.x()) sx = (fixedPoint.x() - pos.x()) / bounds.width();
        if (pos.y() < fixedPoint.y()) sy = (fixedPoint.y() - pos.y()) / bounds.height();
        break;

    case HandleTopRight:
        fixedPoint = bounds.bottomLeft();
        if (pos.x() > fixedPoint.x()) sx = (pos.x() - fixedPoint.x()) / bounds.width();
        if (pos.y() < fixedPoint.y()) sy = (fixedPoint.y() - pos.y()) / bounds.height();
        break;

    case HandleBottomRight:
        fixedPoint = bounds.topLeft();
        if (pos.x() > fixedPoint.x()) sx = (pos.x() - fixedPoint.x()) / bounds.width();
        if (pos.y() > fixedPoint.y()) sy = (pos.y() - fixedPoint.y()) / bounds.height();
        break;

    case HandleBottomLeft:
        fixedPoint = bounds.topRight();
        if (pos.x() < fixedPoint.x()) sx = (fixedPoint.x() - pos.x()) / bounds.width();
        if (pos.y() > fixedPoint.y()) sy = (pos.y() - fixedPoint.y()) / bounds.height();
        break;

    case HandleTop:
        fixedPoint = QPointF(bounds.center().x(), bounds.bottom());
        if (pos.y() < fixedPoint.y()) sy = (fixedPoint.y() - pos.y()) / bounds.height();
        sx = 1.0;
        break;

    case HandleBottom:
        fixedPoint = QPointF(bounds.center().x(), bounds.top());
        if (pos.y() > fixedPoint.y()) sy = (pos.y() - fixedPoint.y()) / bounds.height();
        sx = 1.0;
        break;

    case HandleLeft:
        fixedPoint = QPointF(bounds.right(), bounds.center().y());
        if (pos.x() < fixedPoint.x()) sx = (fixedPoint.x() - pos.x()) / bounds.width();
        sy = 1.0;
        break;

    case HandleRight:
        fixedPoint = QPointF(bounds.left(), bounds.center().y());
        if (pos.x() > fixedPoint.x()) sx = (pos.x() - fixedPoint.x()) / bounds.width();
        sy = 1.0;
        break;

    default:
        return;
    }

    // Apply scaling
    scaleSelection(sx, sy, fixedPoint);

    // Update selection box and handles
    createSelectionBox();
}
// End transform operation
void SelectTool::endTransform() {
    if (!m_transform.isTransforming) return;

    // Apply final transforms to path data
    applyTransformToItems();

    // Reset transform state
    m_transform.isTransforming = false;
    m_transform.activeHandle = HandleNone;
    m_transform.itemStates.clear();

    // Update selection box with new bounds
    createSelectionBox();
}

// Rotate selected items
void SelectTool::rotateSelection(qreal angle) {
    if (qFuzzyIsNull(angle)) return;

    for (auto item : m_selectedItems) {
        if (!m_transform.itemStates.contains(item)) continue;

        const auto& state = m_transform.itemStates[item];
        QPointF origPos = state.pos;

        // Calculate offset from selection center to item's original position
        QPointF offset = origPos - m_transform.center;

        // Rotate the offset by the angle
        QTransform rotationTransform;
        rotationTransform.rotate(angle);
        QPointF rotatedOffset = rotationTransform.map(offset);

        // Calculate new position around the center
        QPointF newPos = m_transform.center + rotatedOffset;

        // Apply new position
        item->setPos(newPos);

        // Apply rotation to the item's transform (if visual rotation is desired)
        QTransform itemTransform = state.transform;
        itemTransform.rotate(angle);
        item->setTransform(itemTransform);

        // Update stored state
        m_transform.itemStates[item].pos = newPos;
        m_transform.itemStates[item].transform = itemTransform;
    }
}
// Scale selected items
void SelectTool::scaleSelection(qreal sx, qreal sy, const QPointF& fixedPoint) {
    sx = qBound(0.05, sx, 20.0);
    sy = qBound(0.05, sy, 20.0);

    for (auto item : m_selectedItems) {
        if (!m_transform.itemStates.contains(item)) continue;

        const auto& state = m_transform.itemStates[item];

        // 1. Get original state components
        const QPointF origPos = state.pos;
        const QTransform origTransform = state.transform;

        // 2. Calculate fixed point in item's LOCAL coordinates
        const QPointF sceneOffset = fixedPoint - origPos;
        const QPointF localPivot = origTransform.inverted().map(sceneOffset);

        // 3. Create scaling transform relative to local pivot
        QTransform scaling;
        scaling.translate(localPivot.x(), localPivot.y());
        scaling.scale(sx, sy);
        scaling.translate(-localPivot.x(), -localPivot.y());

        // 4. Apply scaling to original transform
        QTransform newTransform = origTransform * scaling;

        // 5. Calculate new position to keep fixed point stable
        const QPointF newScenePivot = newTransform.map(localPivot) + origPos;
        const QPointF posCorrection = fixedPoint - newScenePivot;
        const QPointF newPos = origPos + posCorrection;

        // 6. Update item properties
        item->setPos(newPos);
        item->setTransform(newTransform);

        // 7. Update stored state
        m_transform.itemStates[item].pos = newPos;
        m_transform.itemStates[item].transform = newTransform;
    }
}

// Apply all transforms to the actual path data
void SelectTool::applyTransformToItems() {
    for (auto item : m_selectedItems) {
        if (!m_transform.itemStates.contains(item)) continue;

        // Get current transformation state
        const QPointF currentPos = item->pos();
        const QTransform currentTransform = item->transform();
        // Apply to original path
        QPainterPath newPath = currentTransform.map(m_transform.itemStates[item].originalPath);
        newPath.translate(currentPos);

        item->setPath(newPath);
        item->setPos(0, 0);
        item->setTransform(QTransform());
    }
    createSelectionBox();
}

