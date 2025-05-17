#include "DrawingScene.h"
#include <fstream>

// Add these implementations to your existing file

// Set the undo stack
void DrawingScene::setUndoStack(QUndoStack* stack) {
    m_undoStack = stack;

    if (m_undoStack) {
        connect(m_undoStack, &QUndoStack::indexChanged, this, &DrawingScene::updateSelectionUI);
    }
}

// Helper to push command onto the stack
void DrawingScene::pushCommand(QUndoCommand* command) {
    if (m_undoStack) {
        m_undoStack->push(command);
    }
    else {
        command->redo();
        delete command;
    }
}

// AddCommand Implementation
DrawingScene::AddCommand::AddCommand(DrawingScene* scene, StrokeItem* item, QUndoCommand* parent) : QUndoCommand(parent), myScene(scene), myItem(item), firstExecution(true)
{
    setText(QString("Add Shape %1").arg(QString::number(reinterpret_cast<uintptr_t>(item), 16)));
}

DrawingScene::AddCommand::~AddCommand() {
    // If the command is destroyed and the item is still owned by it (i.e., was undone)
    // we need to delete the item to prevent memory leaks.
    if (!firstExecution && myItem) {
        // Check if item is still in the scene; if not, we own it.
        bool inScene = false;
        if (myScene) {
            for (QGraphicsItem* sceneItem : myScene->items()) {
                if (sceneItem == myItem) {
                    inScene = true;
                    break;
                }
            }
        }
        if (!inScene) {
            delete myItem;
            myItem = nullptr;
        }
    }
}

void DrawingScene::AddCommand::undo() {
    if (myScene && myItem) {
        myScene->removeItem(myItem);
        firstExecution = false;
    }
}

void DrawingScene::AddCommand::redo() {
    if (myScene && myItem) {
        myScene->addItem(myItem);
        myItem->update();
        firstExecution = false;
    }
}

// RemoveCommand Implementation
DrawingScene::RemoveCommand::RemoveCommand(DrawingScene* scene, StrokeItem* item, QUndoCommand* parent) : QUndoCommand(parent), myScene(scene), myItem(item)
{
    setText(QString("Remove Shape %1").arg(QString::number(reinterpret_cast<uintptr_t>(item), 16)));
}

DrawingScene::RemoveCommand::~RemoveCommand() {
    // QUndoStack manages command deletion. We assume the item's lifetime
    // is managed elsewhere (e.g., by the scene or another command like EraseCommand)
    // unless explicitly handled (like in AddCommand's destructor).
}

void DrawingScene::RemoveCommand::undo() {
    if (myScene && myItem) {
        myScene->addItem(myItem);
        myItem->update();
    }
}

void DrawingScene::RemoveCommand::redo() {
    if (myScene && myItem) {
        myScene->removeItem(myItem);
    }
}

// EraseCommand Implementation
DrawingScene::EraseCommand::EraseCommand(DrawingScene* scene,
    const QList<StrokeItem*>& originals,
    const QList<StrokeItem*>& results,
    QUndoCommand* parent) : QUndoCommand(parent), myScene(scene), originalItems(originals), resultItems(results), firstExecution(true)
{
    setText(QString("Erase %1 shape(s)").arg(originals.size()));
}

// Destructor needs to handle potential ownership of items if undone
DrawingScene::EraseCommand::~EraseCommand() {
    if (!firstExecution) { // If undone
        // Result items were removed from scene by undo(), we own them now.
        qDeleteAll(resultItems);
    }
    else {
        // Original items were removed by redo() or initial execution.
        // If stack is cleared, we might own them.
        // Check if originals are still in the scene.
        bool originalsInScene = false;
        if (myScene && !originalItems.isEmpty()) {
            QList<QGraphicsItem*> sceneItems = myScene->items();
            for (StrokeItem* item : originalItems) {
                if (sceneItems.contains(item)) {
                    originalsInScene = true;
                    break;
                }
            }
        }
        if (!originalsInScene) {
            qDeleteAll(originalItems);
        }
    }
    originalItems.clear();
    resultItems.clear();
}

void DrawingScene::EraseCommand::undo() {
    if (!myScene) return;
    for (StrokeItem* item : resultItems) {
        myScene->removeItem(item);
    }
    for (StrokeItem* item : originalItems) {
        myScene->addItem(item);
        item->update();
    }
    firstExecution = false;
}

void DrawingScene::EraseCommand::redo() {
    if (!myScene) return;
    for (StrokeItem* item : originalItems) {
        myScene->removeItem(item);
    }
    for (StrokeItem* item : resultItems) {
        myScene->addItem(item);
        item->update();
    }
    firstExecution = true;
}

// MoveCommand Implementation
DrawingScene::MoveCommand::MoveCommand(DrawingScene* scene,
    const QList<StrokeItem*>& items,
    const QPointF& moveDelta,
    QUndoCommand* parent)
    : QUndoCommand(parent), myScene(scene), movedItems(items), delta(moveDelta)
{
    setText(QString("Move %1 shape(s)").arg(items.size()));
    timestamp = QTime::currentTime(); 
}


DrawingScene::MoveCommand::~MoveCommand() {
    // Items are managed by the scene, nothing to delete here
}

void DrawingScene::MoveCommand::redo() {
    if (!myScene) return;
    for (StrokeItem* item : movedItems) {
        if (item->scene() == myScene) {
            item->moveBy(delta.x(), delta.y());
        }
    }
}

void DrawingScene::MoveCommand::undo() {
    if (!myScene) return;
    for (StrokeItem* item : movedItems) {
        if (item->scene() == myScene) {
            item->moveBy(-delta.x(), -delta.y());
        }
    }
}

// Merge consecutive moves of the same items
bool DrawingScene::MoveCommand::mergeWith(const QUndoCommand* other) {
    const MoveCommand* otherMove = dynamic_cast<const MoveCommand*>(other);
    if (!otherMove) return false;

    // Ensure we're merging with the last command on the stack
    if (otherMove->movedItems.size() != this->movedItems.size()) return false;

    QSet<StrokeItem*> mySet(movedItems.begin(), movedItems.end());
    QSet<StrokeItem*> otherSet(otherMove->movedItems.begin(), otherMove->movedItems.end());
    if (mySet != otherSet) return false;

    // Only merge if commands were created within 300ms of each other
    // (This groups moves that are part of the same operation)
    int msSinceLastMove = timestamp.msecsTo(otherMove->timestamp);
    if (msSinceLastMove > 300) return false;

    // Merge the deltas
    delta += otherMove->delta;
    timestamp = otherMove->timestamp; 
    return true;
}

// Clipboard Operations
void DrawingScene::copySelection() {
    m_clipboard.clear();
    for (StrokeItem* item : m_selectedItems) {
        if (!item->isOutlined()) {
            item->convertToFilledPath();
        }
        m_clipboard.append({ item->path(), item->color(), item->width(), item->isOutlined() });
    }
}

void DrawingScene::cutSelection() {
    copySelection();

    // We need to create a copy of the selected items since RemoveCommand will modify the scene
    QList<StrokeItem*> itemsToRemove = m_selectedItems;

    for (StrokeItem* item : itemsToRemove) {
        RemoveCommand* cmd = new RemoveCommand(this, item);
        pushCommand(cmd);
    }

    clearSelection();
}

void DrawingScene::pasteClipboard() {
    if (m_clipboard.isEmpty()) return;

    clearSelection();

    // Calculate the bounding rect of all clipboard items
    QRectF clipboardBounds;
    for (const auto& ci : m_clipboard) {
        if (clipboardBounds.isNull()) {
            clipboardBounds = ci.path.boundingRect();
        }
        else {
            clipboardBounds = clipboardBounds.united(ci.path.boundingRect());
        }
    }

    // Calculate the offset to apply to all items 
    QPointF clipboardCenter = clipboardBounds.center();
    QPointF offsetToApply = m_lastSceneMousePos - clipboardCenter;

    QList<StrokeItem*> pastedItems;

    for (const auto& ci : m_clipboard) {
        StrokeItem* item = new StrokeItem(ci.color, ci.width);
        item->setOutlined(ci.outlined);

        QPainterPath movedPath = ci.path;
        movedPath.translate(offsetToApply);
        item->setPath(movedPath);

        if (ci.outlined) {
            item->setBrush(QBrush(ci.color));
            item->setPen(QPen(ci.color.darker(120), 0.5));
        }
        else {
            QPen pen(ci.color, ci.width);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            item->setPen(pen);
            item->setBrush(Qt::NoBrush);
        }

        AddCommand* cmd = new AddCommand(this, item);
        pushCommand(cmd);
        pastedItems.append(item);
    }

    // Select the newly pasted items
    m_selectedItems = pastedItems;
    highlightSelectedItems(true);

    // Create selection box around the pasted items
    if (!pastedItems.isEmpty()) {
        createSelectionBox();
    }
}


DrawingScene::DrawingScene(QObject* parent)
    : QGraphicsScene(parent), m_currentTool(Brush),
    m_brushColor(Qt::black), m_brushWidth(15),
    m_cooldownInterval(100), m_tangentStrength(0.33) {

    m_cooldownTimer.setInterval(m_cooldownInterval);
    connect(&m_cooldownTimer, &QTimer::timeout, this, &DrawingScene::commitBrushSegment);

    // Initialize key tracking
    m_moveSpeed = 1;

    // Initialize transform state
    m_transform.isTransforming = false;
    m_transform.activeHandle = HandleNone;
}

// Set the current tool
void DrawingScene::setTool(ToolType tool) {
    // If switching away from Select tool, clean up
    if (m_currentTool == Select && tool != Select) {
        if (m_transform.isTransforming) {
            endTransform();
        }
        removeSelectionBox();
    }

    m_currentTool = tool;

    // Clear selection if switching to a non-select tool
    if (tool != Select) {
        clearSelection();
    }
}

// Add color getter and setter
void DrawingScene::setColor(const QColor& color) { m_brushColor = color; }
QColor DrawingScene::currentColor() const { return m_brushColor; }

// Add brush width setter
void DrawingScene::setBrushWidth(qreal width) { m_brushWidth = width; }
qreal DrawingScene::brushWidth() const { return m_brushWidth; }

// Handle mouse press event
void DrawingScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;

    // When in Select mode, check for transform handles
    if (m_currentTool == Select) {
        TransformHandleType handleType = hitTestTransformHandle(event->scenePos());
        if (handleType != HandleNone) {
            // Start transform operation
            startTransform(event->scenePos(), handleType);
            event->accept();
            return;
        }
    }

    // Normal tool behavior
    switch (m_currentTool) {
    case Brush: startBrushStroke(event->scenePos()); break;
    case Eraser: startEraserStroke(event->scenePos()); break;
    case Fill: applyFill(event->scenePos()); break;
    case Select: startSelection(event->scenePos()); break;
    }
    QGraphicsScene::mousePressEvent(event);
}

void DrawingScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    m_lastSceneMousePos = event->scenePos(); // Update last known scene position

    // Handle transform operations
    if (m_transform.isTransforming) {
        updateTransform(event->scenePos());
        event->accept();
        return;
    }

    // Normal tool behavior
    switch (m_currentTool) {
    case Brush: updateBrushStroke(event->scenePos()); break;
    case Eraser: updateEraserStroke(event->scenePos()); break;
    case Select: updateSelection(event->scenePos()); break;
    }
    QGraphicsScene::mouseMoveEvent(event);
}


void DrawingScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    // Handle transform operations
    if (m_transform.isTransforming) {
        endTransform();
        event->accept();
        return;
    }

    // Normal tool behavior
    switch (m_currentTool) {
    case Brush: finalizeBrushStroke(); break;
    case Eraser: finalizeEraserStroke(); break;
    case Select: finalizeSelection(); break;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

// Commit the current brush or eraser segment
void DrawingScene::commitBrushSegment() {
    // Handle both brush and eraser with separate paths
    if (m_currentTool == Brush) {
        commitSegment(m_currentPath, m_tempPathItem, m_realPath);
    }
    else if (m_currentTool == Eraser) {
        commitSegment(m_currentEraserPath, m_tempEraserPathItem, m_eraserRealPath);
    }
}
// Commit the current segment to the path
void DrawingScene::commitSegment(StrokeItem* pathItem, QGraphicsPathItem* tempItem, QPainterPath& realPath) {
    if (!pathItem || m_points.size() < 2) return;

    QPointF start = realPath.currentPosition();
    QPointF end = m_points.last();

    // Calculate tangent directions for smooth curves
    QVector2D startDir = calculateTangent(0, qMin(3, m_points.size() - 1));
    QVector2D endDir = calculateTangent(qMax(0, m_points.size() - 3), qMin(3, m_points.size() - 1));

    // Calculate control points for cubic Bezier curve
    QPointF c1 = start + (startDir * m_tangentStrength * QVector2D(end - start).length()).toPointF();
    QPointF c2 = end - (endDir * m_tangentStrength * QVector2D(end - start).length()).toPointF();

    // Add cubic curve to the path
    realPath.cubicTo(c1, c2, end);
    pathItem->setPath(realPath);

    // Keep only the last point for the next segment
    m_points = { end };

    // Update the temporary path
    updateTemporaryPath(tempItem);
}
// Calculate the tangent direction based on the surrounding points
QVector2D DrawingScene::calculateTangent(int startIndex, int count) {
    startIndex = qBound(0, startIndex, m_points.size() - 2);
    const int availablePoints = m_points.size() - startIndex - 1;
    count = qMin(count, availablePoints);

    if (count < 1) return QVector2D(1, 0);

    QVector2D avgDirection(0, 0);
    for (int i = 0; i < count; i++) {
        const int idx = startIndex + i;
        if (idx >= 0 && idx < m_points.size() - 1) {
            avgDirection += QVector2D(m_points[idx + 1] - m_points[idx]);
        }
    }

    return avgDirection.normalized();
}
// Optimize the path by reducing control points
void DrawingScene::optimizePath(QPainterPath& path, StrokeItem* pathItem) {
    // Skip optimization if there aren't enough points
    if (path.elementCount() < 4) return;

    QPainterPath newPath;
    const QPainterPath::Element& firstEl = path.elementAt(0);
    QPointF lastPoint(firstEl.x, firstEl.y);
    newPath.moveTo(lastPoint);

    // Calculate average segment length for adaptive simplification
    qreal totalLength = 0;
    int segments = 0;

    for (int i = 1; i < path.elementCount(); i += 3) {
        if (i + 2 >= path.elementCount()) break;

        QPointF p0(path.elementAt(i - 1).x, path.elementAt(i - 1).y);
        QPointF p3(path.elementAt(i + 2).x, path.elementAt(i + 2).y);
        totalLength += QLineF(p0, p3).length();
        segments++;
    }

    // Adaptive threshold based on brush width and segment density
    qreal avgSegmentLength = segments > 0 ? totalLength / segments : m_brushWidth * 3;
    qreal simplifyThreshold = qMin(m_brushWidth * 0.75, avgSegmentLength * 0.3);

    // Process each cubic segment
    for (int i = 1; i < path.elementCount(); i += 3) {
        if (i + 2 >= path.elementCount()) break;

        QPointF c1(path.elementAt(i).x, path.elementAt(i).y);
        QPointF c2(path.elementAt(i + 1).x, path.elementAt(i + 1).y);
        QPointF end(path.elementAt(i + 2).x, path.elementAt(i + 2).y);

        // Calculate control point influence
        qreal c1Influence = QLineF(lastPoint, c1).length();
        qreal c2Influence = QLineF(end, c2).length();
        qreal segmentLength = QLineF(lastPoint, end).length();

        // Estimate curve flatness by comparing straight line to control point path
        QLineF directLine(lastPoint, end);
        qreal midPointT = 0.5;
        QPointF bezierMidPoint = lastPoint * (1 - midPointT) * (1 - midPointT) * (1 - midPointT) +
            c1 * 3 * (1 - midPointT) * (1 - midPointT) * midPointT +
            c2 * 3 * (1 - midPointT) * midPointT * midPointT +
            end * midPointT * midPointT * midPointT;
        QPointF lineMidPoint = lastPoint + (end - lastPoint) * 0.5;
        qreal deviation = QLineF(bezierMidPoint, lineMidPoint).length();

        // Decision logic based on curve characteristics
        if (deviation < simplifyThreshold && segmentLength < m_brushWidth * 3) {
            // Nearly straight segment, use quadratic curve or line
            if (deviation < simplifyThreshold * 0.3) {
                newPath.lineTo(end); // Very straight, just use line
            }
            else {
                // Use quadratic with calculated control point to maintain slight curve
                QPointF qc = bezierMidPoint + (bezierMidPoint - lineMidPoint) * 0.5;
                newPath.quadTo(qc, end);
            }
        }
        else if (c1Influence < simplifyThreshold && c2Influence < simplifyThreshold) {
            // Control points very close to endpoints, simplify
            QPointF midControl = (c1 + c2) * 0.5;
            newPath.quadTo(midControl, end);
        }
        else {
            // Preserve the original cubic curve
            newPath.cubicTo(c1, c2, end);
        }

        lastPoint = end;
    }

    path = newPath;
    pathItem->setPath(path);
}
// Update the temporary path for visual feedback
void DrawingScene::updateTemporaryPath(QGraphicsPathItem* tempItem) {
    if (!tempItem) return;

    QPainterPath tempPath;
    if (m_points.isEmpty()) return;

    tempPath.moveTo(m_points.first());
    for (int i = 1; i < m_points.size(); ++i) {
        tempPath.lineTo(m_points[i]);
    }

    tempItem->setPath(tempPath);
}

// Brush Implementation
// Start the brush stroke
void DrawingScene::startBrushStroke(const QPointF& pos) {
    // Create the real path item
    m_currentPath = new StrokeItem(m_brushColor, m_brushWidth);
    // DON'T add to scene yet - only show temporarily
    addItem(m_currentPath);

    // Create the temporary path item for visual feedback
    m_tempPathItem = new QGraphicsPathItem();
    QPen tempPen(m_brushColor, m_brushWidth);
    m_tempPathItem->setPen(tempPen);
    addItem(m_tempPathItem);

    // Reset point collection and paths
    m_points.clear();
    m_points << pos;
    m_realPath = QPainterPath();
    m_realPath.moveTo(pos);
    m_currentPath->setPath(m_realPath);

    // Start the cooldown timer
    m_cooldownTimer.start();
}


// Update the brush stroke
void DrawingScene::updateBrushStroke(const QPointF& pos) {
    if (!m_currentPath) return;

    m_points << pos;
    updateTemporaryPath(m_tempPathItem);
}
// Finalize the brush stroke
void DrawingScene::finalizeBrushStroke() {
    // Stop the timer
    m_cooldownTimer.stop();

    if (!m_currentPath) return;

    // Commit any remaining points
    if (m_points.size() > 1) {
        commitSegment(m_currentPath, m_tempPathItem, m_realPath);
    }
    else if (m_points.size() == 1 && m_realPath.elementCount() <= 1) {
        // For single clicks, create a circle
        QPainterPath circlePath;
        circlePath.addEllipse(m_points.first(), m_brushWidth / 2, m_brushWidth / 2);
        m_currentPath->setPath(circlePath);
    }

    // Perform final path optimization
    optimizePath(m_realPath, m_currentPath);

    // Convert to filled path
    m_currentPath->convertToFilledPath();

    // Important: Remove from scene before creating command
    removeItem(m_currentPath);

    // Create Add command (will add to scene in redo())
    AddCommand* cmd = new AddCommand(this, m_currentPath);

    // Clean up temporary display items
    if (m_tempPathItem) {
        removeItem(m_tempPathItem);
        delete m_tempPathItem;
        m_tempPathItem = nullptr;
    }

    // Store and reset state BEFORE pushing command
    StrokeItem* itemToAdd = m_currentPath;
    m_currentPath = nullptr;
    m_points.clear();

    // Push command (will call redo() which adds item)
    pushCommand(cmd);
}


// Eraser Implementation
// Start the eraser stroke
void DrawingScene::startEraserStroke(const QPointF& pos) {
    // Create a visible eraserPath item
    m_currentEraserPath = new StrokeItem(Qt::red, m_brushWidth);
    m_currentEraserPath->setOpacity(0.5); // Semi-transparent
    addItem(m_currentEraserPath);

    // Create temporary path for visual feedback
    m_tempEraserPathItem = new QGraphicsPathItem();
    QPen tempPen(Qt::red, m_brushWidth);
    //tempPen.setStyle(Qt::DotLine);
    QColor tempColor = Qt::red;
    tempColor.lighter(150); // Make temp path slightly lighter
    tempPen.setColor(tempColor); // Make temp path slightly lighter
    m_tempEraserPathItem->setPen(tempPen);
    m_tempEraserPathItem->setOpacity(0.5);
    addItem(m_tempEraserPathItem);

    // Reset point collection and paths
    m_points.clear();
    m_points << pos;
    m_eraserRealPath = QPainterPath();
    m_eraserRealPath.moveTo(pos);
    m_currentEraserPath->setPath(m_eraserRealPath);

    // Start the cooldown timer
    m_cooldownTimer.start();
}
// Update the eraser stroke
void DrawingScene::updateEraserStroke(const QPointF& pos) {
    if (!m_currentEraserPath) return;

    m_points << pos;
    updateTemporaryPath(m_tempEraserPathItem);
}


void DrawingScene::finalizeEraserStroke() {
    // Stop the timer
    m_cooldownTimer.stop();

    if (!m_currentEraserPath) return;

    // Commit any remaining points
    if (m_points.size() > 1) {
        commitSegment(m_currentEraserPath, m_tempEraserPathItem, m_eraserRealPath);
    }
    else if (m_points.size() == 1 && m_eraserRealPath.elementCount() <= 1) {
        // For single clicks, create a circle
        QPainterPath circlePath;
        circlePath.addEllipse(m_points.first(), m_brushWidth / 2, m_brushWidth / 2);
        m_currentEraserPath->setPath(circlePath);
    }

    // Perform final path optimization
    optimizePath(m_eraserRealPath, m_currentEraserPath);

    // Convert to filled path
    m_currentEraserPath->convertToFilledPath();

    // Get the path and convert to Clipper format
    Clipper2Lib::Path64 eraserClipperPath = DrawingEngineUtils::convertPathToClipper(m_currentEraserPath->path());
    QPainterPath eraserQtPath = m_currentEraserPath->path();

    // Cleanup the temporary items
    if (m_tempEraserPathItem) {
        removeItem(m_tempEraserPathItem);
        delete m_tempEraserPathItem;
        m_tempEraserPathItem = nullptr;
    }

    // Remove the eraser path but keep its data for processing
    removeItem(m_currentEraserPath);
    delete m_currentEraserPath;
    m_currentEraserPath = nullptr;

    // Get items that intersect with the eraser
    QList<QGraphicsItem*> intersectingItems = items(eraserQtPath);

    QList<StrokeItem*> originalItemsAffected;
    QList<StrokeItem*> resultingItems;

    // Process only the strokes that the eraser actually intersects, excluding onion skins
    for (QGraphicsItem* item : intersectingItems) {
        if (auto stroke = dynamic_cast<StrokeItem*>(item)) {
            // Skip items that are part of onion skin groups
            if (stroke->parentItem()) {
                continue;
            }

            // Make sure the stroke is converted to filled path if not already
            if (!stroke->isOutlined()) {
                stroke->convertToFilledPath();
            }

            // Record original item
            originalItemsAffected.append(stroke);

            // Get the stroke path with transform applied
            QPainterPath strokeScenePath = stroke->path();

            // Apply any transform to get scene coordinates
            if (!stroke->transform().isIdentity() || stroke->pos() != QPointF(0, 0)) {
                QTransform sceneTransform = stroke->sceneTransform();
                strokeScenePath = sceneTransform.map(strokeScenePath);
            }

            // Use eraserQtPath directly
            QPainterPath eraserPath = eraserQtPath;

            // Save original color
            QColor originalColor = stroke->color();

            // APPROACH: Use Qt's subtracted function and then find separate components
            QPainterPath resultPath = strokeScenePath.subtracted(eraserPath);

            // If nothing remains, continue (item will be deleted by EraseCommand)
            if (resultPath.isEmpty()) {
                continue;
            }

            // Find disconnected components using floodfill-like algorithm
            QList<QPainterPath> separatePaths = findDisconnectedComponents(resultPath);

            // Create a new StrokeItem for each separate component
            for (const QPainterPath& path : separatePaths) {
                if (!path.isEmpty()) {
                    StrokeItem* newStroke = new StrokeItem(originalColor, 0);
                    newStroke->setPath(path);
                    newStroke->setBrush(QBrush(originalColor));
                    newStroke->setPen(QPen(originalColor.darker(120), 0.5));
                    newStroke->setOutlined(true);
                    resultingItems.append(newStroke);
                    // Don't add to scene yet - EraseCommand will do that
                }
            }
        }
    }

    // Reset state
    m_points.clear();

    // Only create command if something changed
    if (!originalItemsAffected.isEmpty()) {
        // Create Erase command - this will handle removing original items and adding new ones
        EraseCommand* cmd = new EraseCommand(this, originalItemsAffected, resultingItems);
        pushCommand(cmd);
    }
}


// Process the eraser on a single stroke
// This function should be removed or updated to use the command pattern
void DrawingScene::processEraserOnStroke(StrokeItem* stroke, const Clipper2Lib::Path64& eraserPath) {
    // Get the stroke path with transform applied
    QPainterPath strokeScenePath = stroke->path();

    // Apply any transform to get scene coordinates
    if (!stroke->transform().isIdentity() || stroke->pos() != QPointF(0, 0)) {
        QTransform sceneTransform = stroke->sceneTransform();
        strokeScenePath = sceneTransform.map(strokeScenePath);
    }

    // Convert eraser path to Qt directly
    QPainterPath eraserQtPath = DrawingEngineUtils::convertSingleClipperPath(eraserPath);

    // Save original color
    QColor originalColor = stroke->color();

    // APPROACH: Use Qt's subtracted function and then find separate components
    QPainterPath resultPath = strokeScenePath.subtracted(eraserQtPath);

    // If nothing remains, we're done
    if (resultPath.isEmpty()) {
        return;
    }

    // Find disconnected components
    QList<QPainterPath> separatePaths = findDisconnectedComponents(resultPath);

    // Create list of new items but DON'T add them directly to scene
    QList<StrokeItem*> resultItems;

    for (const QPainterPath& path : separatePaths) {
        if (!path.isEmpty()) {
            StrokeItem* newStroke = new StrokeItem(originalColor, 0);
            newStroke->setPath(path);
            newStroke->setBrush(QBrush(originalColor));
            newStroke->setPen(QPen(originalColor.darker(120), 0.5));
            newStroke->setOutlined(true);
            resultItems.append(newStroke);
        }
    }

    // Create an EraseCommand to handle the change
    QList<StrokeItem*> originalItems;
    originalItems.append(stroke);
    EraseCommand* cmd = new EraseCommand(this, originalItems, resultItems);
    pushCommand(cmd);
}

// Add this helper function to find disconnected components in a complex path
QList<QPainterPath> DrawingScene::findDisconnectedComponents(const QPainterPath& complexPath) {
    QList<QPainterPath> components;

    // Use QPainterPath elementCount and elementAt to analyze the path
    int elementCount = complexPath.elementCount();
    if (elementCount <= 1) {
        if (!complexPath.isEmpty()) {
            components.append(complexPath);
        }
        return components;
    }

    // Track subpaths by their starting positions
    QMap<QPair<qreal, qreal>, int> subpathIndices;
    QList<QPainterPath> subpaths;

    QPainterPath currentSubpath;
    bool subpathStarted = false;

    // Extract individual subpaths
    for (int i = 0; i < elementCount; i++) {
        QPainterPath::Element element = complexPath.elementAt(i);

        switch (element.type) {
        case QPainterPath::MoveToElement:
            // End previous subpath if any
            if (subpathStarted && !currentSubpath.isEmpty()) {
                subpaths.append(currentSubpath);
                currentSubpath = QPainterPath();
            }

            // Start new subpath
            currentSubpath.moveTo(element.x, element.y);
            subpathStarted = true;
            break;

        case QPainterPath::LineToElement:
            if (subpathStarted) {
                currentSubpath.lineTo(element.x, element.y);
            }
            break;

        case QPainterPath::CurveToElement:
            if (subpathStarted && i + 2 < elementCount) {
                QPainterPath::Element ctrl2 = complexPath.elementAt(i + 1);
                QPainterPath::Element endPoint = complexPath.elementAt(i + 2);

                if (ctrl2.type == QPainterPath::CurveToDataElement &&
                    endPoint.type == QPainterPath::CurveToDataElement) {
                    currentSubpath.cubicTo(
                        element.x, element.y,
                        ctrl2.x, ctrl2.y,
                        endPoint.x, endPoint.y
                    );
                    i += 2; // Skip the control points we just processed
                }
            }
            break;

        case QPainterPath::CurveToDataElement:
            // These are handled in the CurveToElement case
            break;
        }
    }

    // Add the last subpath if any
    if (subpathStarted && !currentSubpath.isEmpty()) {
        subpaths.append(currentSubpath);
    }

    // Group subpaths into connected components based on containment
    QList<QList<int>> connectedGroups;
    QVector<bool> processed(subpaths.size(), false);

    for (int i = 0; i < subpaths.size(); i++) {
        if (processed[i]) continue;

        // Start a new group with this subpath
        QList<int> group;
        group.append(i);
        processed[i] = true;

        // Check all other unprocessed subpaths
        bool addedMore;
        do {
            addedMore = false;

            for (int j = 0; j < subpaths.size(); j++) {
                if (processed[j]) continue;

                // Check against all subpaths in the current group
                for (int groupIdx : group) {
                    // If one contains the other OR they intersect, they're connected
                    QRectF rect1 = subpaths[groupIdx].boundingRect();
                    QRectF rect2 = subpaths[j].boundingRect();

                    if (rect1.intersects(rect2)) {
                        QPointF testPoint = subpaths[j].pointAtPercent(0.0);
                        if (subpaths[groupIdx].contains(testPoint) ||
                            subpaths[j].contains(subpaths[groupIdx].pointAtPercent(0.0)) ||
                            subpathsIntersect(subpaths[groupIdx], subpaths[j])) {

                            group.append(j);
                            processed[j] = true;
                            addedMore = true;
                            break;
                        }
                    }
                }
            }
        } while (addedMore);

        connectedGroups.append(group);
    }

    // Create a path for each connected group
    for (const QList<int>& group : connectedGroups) {
        QPainterPath connectedPath;

        // First, handle outer subpaths (those not contained in any other)
        for (int i : group) {
            bool isOuter = true;
            for (int j : group) {
                if (i != j && subpaths[j].contains(subpaths[i].pointAtPercent(0.0))) {
                    isOuter = false;
                    break;
                }
            }

            if (isOuter) {
                if (connectedPath.isEmpty()) {
                    connectedPath = subpaths[i];
                }
                else {
                    connectedPath = connectedPath.united(subpaths[i]);
                }
            }
        }

        // Then handle inner subpaths (holes)
        for (int i : group) {
            bool isInner = false;
            for (int j : group) {
                if (i != j && subpaths[j].contains(subpaths[i].pointAtPercent(0.0))) {
                    isInner = true;
                    break;
                }
            }

            if (isInner) {
                // Only subtract if we have an outer path already
                if (!connectedPath.isEmpty()) {
                    connectedPath = connectedPath.subtracted(subpaths[i]);
                }
            }
        }

        if (!connectedPath.isEmpty()) {
            // Ensure consistent fill rule
            connectedPath.setFillRule(Qt::WindingFill);
            components.append(connectedPath);
        }
    }

    return components;
}

// Helper to determine if two subpaths intersect
bool DrawingScene::subpathsIntersect(const QPainterPath& path1, const QPainterPath& path2) {
    // First do a fast check with bounding rects
    if (!path1.boundingRect().intersects(path2.boundingRect())) {
        return false;
    }

    // A simple approach is to check if their intersection is non-empty
    QPainterPath intersection = path1.intersected(path2);
    return !intersection.isEmpty();
}


// Fill Implementation
// Apply fill at the clicked position
void DrawingScene::applyFill(const QPointF& pos) {
    // Create a temporary image of the current scene, excluding onion skin items
    QRectF sceneRect = this->sceneRect();
    QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    // Save the current visibility state of onion skin items
    QList<QGraphicsItem*> hiddenItems;
    for (QGraphicsItem* item : items()) {
        if (item->parentItem()) {
            // This is likely an onion skin item
            if (item->isVisible()) {
                item->setVisible(false);
                hiddenItems.append(item);
            }
        }
    }

    // Render the scene without onion skin items
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    render(&painter);
    painter.end();

    // Restore visibility of hidden items
    for (QGraphicsItem* item : hiddenItems) {
        item->setVisible(true);
    }

    // Convert scene position to image coordinates
    int x = qRound(pos.x() - sceneRect.left());
    int y = qRound(pos.y() - sceneRect.top());

    // Bounds check
    if (x < 0 || x >= image.width() || y < 0 || y >= image.height()) {
        return;
    }

    // Get target color
    QRgb targetColor = image.pixel(x, y);

    // Simple flood fill algorithm
    QVector<QPoint> fillPoints;
    QVector<bool> visited(image.width() * image.height(), false);
    QQueue<QPoint> queue;

    // Color tolerance
    int tolerance = 30;

    // Start with the seed point
    queue.enqueue(QPoint(x, y));

    // Process the queue
    while (!queue.isEmpty()) {
        QPoint p = queue.dequeue();

        // Skip if out of bounds or already visited
        if (p.x() < 0 || p.x() >= image.width() || p.y() < 0 || p.y() >= image.height() ||
            visited[p.y() * image.width() + p.x()]) {
            continue;
        }

        // Check color similarity
        QRgb currentColor = image.pixel(p.x(), p.y());
        bool similar =
            qAbs(qRed(currentColor) - qRed(targetColor)) <= tolerance &&
            qAbs(qGreen(currentColor) - qGreen(targetColor)) <= tolerance &&
            qAbs(qBlue(currentColor) - qBlue(targetColor)) <= tolerance;

        if (!similar) {
            continue;
        }

        // Mark as visited and add to fill points
        visited[p.y() * image.width() + p.x()] = true;
        fillPoints.append(p);

        // Add neighbors to queue (4-connected)
        queue.enqueue(QPoint(p.x() + 1, p.y()));
        queue.enqueue(QPoint(p.x() - 1, p.y()));
        queue.enqueue(QPoint(p.x(), p.y() + 1));
        queue.enqueue(QPoint(p.x(), p.y() - 1));
    }


    if (fillPoints.isEmpty()) {
        return;
    }

    // OPTIMIZATION: Convert filled pixels to horizontal spans
    struct Span {
        int y, x1, x2;
        bool operator<(const Span& other) const {
            if (y != other.y) return y < other.y;
            return x1 < other.x1;
        }
    };

    // Sort points by y, then x for efficient span creation
    std::sort(fillPoints.begin(), fillPoints.end(),
        [](const QPoint& a, const QPoint& b) {
            if (a.y() != b.y()) return a.y() < b.y();
            return a.x() < b.x();
        });

    // Create horizontal spans from the fill points
    QVector<Span> spans;
    int currentY = -1;
    int spanStart = -1;

    for (int i = 0; i < fillPoints.size(); ++i) {
        QPoint p = fillPoints[i];

        // Start new row
        if (p.y() != currentY) {
            currentY = p.y();
            spanStart = p.x();
        }
        // Continue current span
        else if (p.x() == fillPoints[i - 1].x() + 1) {
            // Still in the same span, continue
        }
        // End previous span and start new one
        else {
            spans.append({ currentY, spanStart, fillPoints[i - 1].x() });
            spanStart = p.x();
        }

        // End of row or last point
        if (i == fillPoints.size() - 1 || fillPoints[i + 1].y() != currentY) {
            spans.append({ currentY, spanStart, p.x() });
        }
    }


    // Convert spans to Clipper2 paths
    Clipper2Lib::Paths64 paths;
    const double padding = 0.1; // Small padding to ensure connectivity

    // For very large fills, limit the number of spans we process
    const int maxSpans = 5000;
    if (spans.size() > maxSpans) {
        spans.resize(maxSpans);
    }

    for (const Span& span : spans) {
        double sceneY1 = span.y + sceneRect.top() - padding;
        double sceneY2 = span.y + sceneRect.top() + 1.0 + padding;
        double sceneX1 = span.x1 + sceneRect.left() - padding;
        double sceneX2 = span.x2 + sceneRect.left() + 1.0 + padding;

        Clipper2Lib::Path64 rect;
        rect.push_back(Clipper2Lib::Point64(static_cast<int64_t>(sceneX1 * CLIPPER_SCALING),
            static_cast<int64_t>(sceneY1 * CLIPPER_SCALING)));
        rect.push_back(Clipper2Lib::Point64(static_cast<int64_t>(sceneX2 * CLIPPER_SCALING),
            static_cast<int64_t>(sceneY1 * CLIPPER_SCALING)));
        rect.push_back(Clipper2Lib::Point64(static_cast<int64_t>(sceneX2 * CLIPPER_SCALING),
            static_cast<int64_t>(sceneY2 * CLIPPER_SCALING)));
        rect.push_back(Clipper2Lib::Point64(static_cast<int64_t>(sceneX1 * CLIPPER_SCALING),
            static_cast<int64_t>(sceneY2 * CLIPPER_SCALING)));
        paths.push_back(rect);
    }

    // Use Clipper2 to union the spans
    Clipper2Lib::Clipper64 clipper;
    clipper.PreserveCollinear(true);
    clipper.AddSubject(paths);

    Clipper2Lib::Paths64 solution;
    clipper.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, solution);

    if (!solution.empty()) {
        // Convert the Clipper paths to a QPainterPath
        QPainterPath fillPath;

        for (const auto& path : solution) {
            if (path.size() < 3) continue;

            QPainterPath subPath = DrawingEngineUtils::convertSingleClipperPath(path);

            if (fillPath.isEmpty()) {
                fillPath = subPath;
            }
            else {
                // Handle inner and outer contours correctly
                if (Clipper2Lib::Area(path) < 0) {
                    fillPath = fillPath.subtracted(subPath);
                }
                else {
                    fillPath = fillPath.united(subPath);
                }
            }
        }

        // Create a filled shape
        StrokeItem* fill = new StrokeItem(m_brushColor, 0);
        fill->setPath(fillPath);
        fill->setBrush(QBrush(m_brushColor));
        fill->setPen(QPen(m_brushColor.darker(120), 0.5));
        fill->setOutlined(true);

        // Create Add command - don't add fill to scene directly
        AddCommand* cmd = new AddCommand(this, fill);
        pushCommand(cmd);
    }
}



// Selection Implementation
void DrawingScene::startSelection(const QPointF& pos) {
    // If clicking on a selected item, start moving
    QList<QGraphicsItem*> itemsAtPos = items(pos);

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
            addItem(m_selectionRect);
        }

        m_selectionRect->setRect(QRectF(pos, QSizeF(0, 0)));
        m_selectionRect->show();
    }
}

void DrawingScene::updateSelection(const QPointF& pos) {
    if (m_isSelecting && m_selectionRect) {
        // Update selection rectangle
        QRectF rect = QRectF(
            QPointF(qMin(m_selectionStartPos.x(), pos.x()), qMin(m_selectionStartPos.y(), pos.y())),
            QPointF(qMax(m_selectionStartPos.x(), pos.x()), qMax(m_selectionStartPos.y(), pos.y()))
        );
        m_selectionRect->setRect(rect);
    }
    else if (m_isMovingSelection) {
        QPointF delta = pos - m_lastMousePos;

        if (!m_selectedItems.isEmpty() && delta.manhattanLength() > 0.01) {
            // Just update the visual positions during dragging - no commands yet
            for (StrokeItem* item : m_selectedItems) {
                if (item->scene() == this) {
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

void DrawingScene::finalizeSelection() {
    if (m_isSelecting && m_selectionRect) {
        // Items within selection rectangle
        QList<QGraphicsItem*> itemsInRect = items(m_selectionRect->rect());

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
                MoveCommand* cmd = new MoveCommand(this, movedItems, totalDelta);

                // Important: Move items back to start positions first
                for (StrokeItem* item : movedItems) {
                    item->setPos(m_startPositions[item]);
                }

                // Then push command (which will call redo() and apply the move)
                pushCommand(cmd);
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

void DrawingScene::moveSelectedItems(const QPointF& delta) {
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

void DrawingScene::clearSelection() {
    highlightSelectedItems(false);
    m_selectedItems.clear();
    removeSelectionBox();
}

void DrawingScene::highlightSelectedItems(bool highlight) {
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

void DrawingScene::keyPressEvent(QKeyEvent* event) {
    // Handle copy, cut, paste
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_C) {
            if (!m_selectedItems.isEmpty()) {
                copySelection();
                event->accept();
                return;
            }
        }
        if (event->key() == Qt::Key_X) {
            if (!m_selectedItems.isEmpty()) {
                cutSelection();
                event->accept();
                return;
            }
        }
        if (event->key() == Qt::Key_V) {
            pasteClipboard();
            event->accept();
            return;
        }
        // Add Ctrl+Z and Ctrl+Y for undo/redo
        if (event->key() == Qt::Key_Z && m_undoStack) {
            m_undoStack->undo();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Y && m_undoStack) {
            m_undoStack->redo();
            event->accept();
            return;
        }
    }

    if (m_currentTool == Select && !m_selectedItems.isEmpty()) {
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
            // Apply the movement visually (without commands yet)
            for (StrokeItem* item : m_selectedItems) {
                if (item->scene() == this) {
                    item->moveBy(dx, dy);
                }
            }

            event->accept();
            return;
        }

        // Handle delete key for selected items
        if (key == Qt::Key_Delete) {
            QList<StrokeItem*> itemsToRemove = m_selectedItems;

            for (StrokeItem* item : itemsToRemove) {
                RemoveCommand* cmd = new RemoveCommand(this, item);
                pushCommand(cmd);
            }

            m_selectedItems.clear();
            event->accept();
            return;
        }
    }

    QGraphicsScene::keyPressEvent(event);
}

void DrawingScene::keyReleaseEvent(QKeyEvent* event) {
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
                    MoveCommand* cmd = new MoveCommand(this, movedItems, totalDelta);

                    // Move items back to start positions first
                    for (StrokeItem* item : movedItems) {
                        item->setPos(m_startPositions[item]);
                    }

                    // Push command to apply the move
                    pushCommand(cmd);
                }

                // Clear start positions
                m_startPositions.clear();
            }
        }

        // Only reset the timer if all arrow keys are released
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

    QGraphicsScene::keyReleaseEvent(event);
}

DrawingScene::~DrawingScene() {
    // Clean up selection rectangle
    if (m_selectionRect) {
        delete m_selectionRect;
        m_selectionRect = nullptr;
    }

    // Clean up transform handles
    removeSelectionBox();
}

void DrawingScene::resetSelectionState() {
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
        removeItem(m_selectionRect);
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
void DrawingScene::createSelectionBox() {
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
    addItem(m_transform.box);

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
        addItem(handle);
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
    addItem(m_transform.rotationHandle);
    m_transform.handles.append(m_transform.rotationHandle);

    // Line from center to rotation handle
    m_transform.rotationLine = new QGraphicsLineItem(
        QLineF(bounds.center(), rotHandlePos));
    m_transform.rotationLine->setPen(QPen(Qt::blue, 1, Qt::DashLine));
    m_transform.rotationLine->setZValue(999);
    addItem(m_transform.rotationLine);
    m_transform.handles.append(m_transform.rotationLine);

    // Center point
    m_transform.centerPoint = new QGraphicsEllipseItem(
        QRectF(bounds.center().x() - 3, bounds.center().y() - 3, 6, 6));
    m_transform.centerPoint->setPen(QPen(Qt::red));
    m_transform.centerPoint->setBrush(Qt::red);
    m_transform.centerPoint->setZValue(1001);
    addItem(m_transform.centerPoint);
    m_transform.handles.append(m_transform.centerPoint);
}

// Remove selection box and handles
void DrawingScene::removeSelectionBox() {
    if (m_transform.box) {
        removeItem(m_transform.box);
        delete m_transform.box;
        m_transform.box = nullptr;
    }

    for (auto handle : m_transform.handles) {
        removeItem(handle);
        delete handle;
    }
    m_transform.handles.clear();
    m_transform.rotationHandle = nullptr;
    m_transform.rotationLine = nullptr;
    m_transform.centerPoint = nullptr;
}

// Test if a point hits a transform handle and return the handle type
TransformHandleType DrawingScene::hitTestTransformHandle(const QPointF& pos) {
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
void DrawingScene::startTransform(const QPointF& pos, TransformHandleType handleType) {
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
void DrawingScene::updateTransform(const QPointF& pos) {
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
void DrawingScene::endTransform() {
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
void DrawingScene::rotateSelection(qreal angle) {
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
void DrawingScene::scaleSelection(qreal sx, qreal sy, const QPointF& fixedPoint) {
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
void DrawingScene::applyTransformToItems() {
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

void DrawingScene::handleKeyPress(QKeyEvent* event) {
    keyPressEvent(event);
}

void DrawingScene::handleKeyRelease(QKeyEvent* event) {
    keyReleaseEvent(event);
}

void DrawingScene::updateSelectionUI() {
    if (!m_selectedItems.isEmpty()) {
        removeSelectionBox();
        createSelectionBox();
        highlightSelectedItems(true);
    }
    else {
        removeSelectionBox();
    }
}