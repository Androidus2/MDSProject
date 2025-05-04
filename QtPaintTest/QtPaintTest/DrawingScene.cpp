#include "DrawingScene.h"
#include <QUndoStack>

DrawingScene::DrawingScene(QObject* parent)
    : QGraphicsScene(parent), m_currentTool(Brush),
    m_brushColor(Qt::black), m_brushWidth(15),
    m_cooldownInterval(100), m_tangentStrength(0.33) {

    m_cooldownTimer.setInterval(m_cooldownInterval);
    connect(&m_cooldownTimer, &QTimer::timeout, this, &DrawingScene::commitBrushSegment);

    // Initialize key tracking
    m_moveSpeed = 1;
}

// Set the current tool
void DrawingScene::setTool(ToolType tool) { m_currentTool = tool; }

// Add color getter and setter
void DrawingScene::setColor(const QColor& color) { m_brushColor = color; }
QColor DrawingScene::currentColor() const { return m_brushColor; }

// Add brush width setter
void DrawingScene::setBrushWidth(qreal width) { m_brushWidth = width; }

// Setter for Undo Stack
void DrawingScene::setUndoStack(QUndoStack* stack) {
    m_undoStack = stack;
}

// Helper to push command onto the stack
void DrawingScene::pushCommand(QUndoCommand* command) {
    if (m_undoStack) {
        m_undoStack->push(command);
    } else {
        // If no undo stack, execute directly and delete
        command->redo();
        delete command;
    }
}

// AddCommand Implementation
DrawingScene::AddCommand::AddCommand(DrawingScene* scene, StrokeItem* item, QUndoCommand* parent)
    : QUndoCommand(parent), myScene(scene), myItem(item), firstExecution(true)
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
        firstExecution = false; // Item is no longer in the scene initially
    }
}

void DrawingScene::AddCommand::redo() {
    if (myScene && myItem) {
        myScene->addItem(myItem);
        myItem->update();
        firstExecution = false; // Item is now in the scene
    }
}

// RemoveCommand Implementation
DrawingScene::RemoveCommand::RemoveCommand(DrawingScene* scene, StrokeItem* item, QUndoCommand* parent)
    : QUndoCommand(parent), myScene(scene), myItem(item)
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
// EraseCommand
DrawingScene::EraseCommand::EraseCommand(DrawingScene* scene,
                                         const QList<StrokeItem*>& originals,
                                         const QList<StrokeItem*>& results,
                                         QUndoCommand* parent)
    : QUndoCommand(parent), myScene(scene), originalItems(originals), resultItems(results), firstExecution(true)
{
     setText(QString("Erase %1 shape(s)").arg(originals.size()));
}

// Destructor needs to handle potential ownership of items if undone
DrawingScene::EraseCommand::~EraseCommand() {
    if (!firstExecution) { // If undone
        // Result items were removed from scene by undo(), we own them now.
        qDeleteAll(resultItems);
    } else {
        // Original items were removed by redo() or initial execution.
        // If stack is cleared, we might own them.
        // Check if originals are still in the scene.
        bool originalsInScene = false;
        if (myScene && !originalItems.isEmpty()) {
             QList<QGraphicsItem*> sceneItems = myScene->items();
             for(StrokeItem* item : originalItems) {
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


// --- MoveCommand Implementation ---
DrawingScene::MoveCommand::MoveCommand(DrawingScene* scene,
                                       const QList<StrokeItem*>& items,
                                       const QPointF& moveDelta,
                                       QUndoCommand* parent)
    : QUndoCommand(parent), myScene(scene), movedItems(items), delta(moveDelta)
{
    setText(QString("Move %1 shape(s)").arg(items.size()));
}

DrawingScene::MoveCommand::~MoveCommand() {
    // Items are managed by the scene, nothing to delete here
}

void DrawingScene::MoveCommand::undo() {
    if (!myScene) return;
    for (StrokeItem* item : movedItems) {
        // Ensure item still exists in the scene before moving
        if (item->scene() == myScene) {
            item->moveBy(-delta.x(), -delta.y());
        }
    }
}

void DrawingScene::MoveCommand::redo() {
    if (!myScene) return;
    for (StrokeItem* item : movedItems) {
        // Ensure item still exists in the scene before moving
        if (item->scene() == myScene) {
            item->moveBy(delta.x(), delta.y());
        }
    }
}

// Merge consecutive moves of the same items - Revised
bool DrawingScene::MoveCommand::mergeWith(const QUndoCommand* other) {
    const MoveCommand* otherMove = dynamic_cast<const MoveCommand*>(other);
    if (!otherMove) return false;

    // Ensure we're merging with the last command on the stack (active command)
    if (otherMove->movedItems.size() != this->movedItems.size()) return false;

    QSet<StrokeItem*> mySet(movedItems.begin(), movedItems.end());
    QSet<StrokeItem*> otherSet(otherMove->movedItems.begin(), otherMove->movedItems.end());
    if (mySet != otherSet) return false;

    // Merge the deltas
    delta += otherMove->delta;
    return true;
}

// Handle mouse press event
void DrawingScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;

    switch (m_currentTool) {
    case Brush: startBrushStroke(event->scenePos()); break;
    case Eraser: startEraserStroke(event->scenePos()); break;
    case Fill: applyFill(event->scenePos()); break;
    case Select: startSelection(event->scenePos()); break;
    }
    QGraphicsScene::mousePressEvent(event);
}

// Handle mouse move event
void DrawingScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    switch (m_currentTool) {
    case Brush: updateBrushStroke(event->scenePos()); break;
    case Eraser: updateEraserStroke(event->scenePos()); break;
    case Select: updateSelection(event->scenePos()); break;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

// Handle mouse release event
void DrawingScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
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
    addItem(m_currentPath);

    // Create the temporary path item for visual feedback
    m_tempPathItem = new QGraphicsPathItem();
    QPen tempPen(m_brushColor, m_brushWidth);
    //tempPen.setStyle(Qt::DotLine);
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
    m_cooldownTimer.stop();
    if (!m_currentPath) return;

    if (m_points.size() > 1) {
        commitSegment(m_currentPath, m_tempPathItem, m_realPath);
    } else if (m_points.size() == 1 && m_realPath.elementCount() <= 1) {
        QPainterPath circlePath;
        circlePath.addEllipse(m_points.first(), m_brushWidth / 2, m_brushWidth / 2);
        m_currentPath->setPath(circlePath);
    }

    optimizePath(m_realPath, m_currentPath);
    m_currentPath->convertToFilledPath();

    // Create Add command INSTEAD of just adding
    AddCommand* cmd = new AddCommand(this, m_currentPath);
    // No longer add item directly here, command's redo will do it.
    // addItem(m_currentPath);

    // Clean up temporary item
    if (m_tempPathItem) {
        removeItem(m_tempPathItem);
        delete m_tempPathItem;
        m_tempPathItem = nullptr;
    }

    // Reset state AFTER creating command but BEFORE pushing
    StrokeItem* itemToAdd = m_currentPath;
    m_currentPath = nullptr;
    m_points.clear();

    // Push command (will call redo() which adds itemToAdd)
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
// Finalize the eraser stroke
void DrawingScene::finalizeEraserStroke() {
    m_cooldownTimer.stop();
    if (!m_currentEraserPath) return;

    if (m_points.size() > 1) {
        commitSegment(m_currentEraserPath, m_tempEraserPathItem, m_eraserRealPath);
    }
    else if (m_points.size() == 1 && m_eraserRealPath.elementCount() <= 1) {
        QPainterPath circlePath;
        circlePath.addEllipse(m_points.first(), m_brushWidth / 2, m_brushWidth / 2);
        m_currentEraserPath->setPath(circlePath);
    }
    optimizePath(m_eraserRealPath, m_currentEraserPath);
    m_currentEraserPath->convertToFilledPath();

    Clipper2Lib::Path64 eraserClipperPath = DrawingEngineUtils::convertPathToClipper(m_currentEraserPath->path());
    QPainterPath eraserQtPath = m_currentEraserPath->path();

    // Cleanup the temporary display items for the eraser itself
    if (m_tempEraserPathItem) {
        removeItem(m_tempEraserPathItem);
        delete m_tempEraserPathItem;
        m_tempEraserPathItem = nullptr;
    }
    // Remove the visual representation of the eraser path, but keep its data
    removeItem(m_currentEraserPath);
    delete m_currentEraserPath;
    m_currentEraserPath = nullptr;

    QList<StrokeItem*> originalItemsAffected;
    QList<StrokeItem*> resultingItems;
    QList<StrokeItem*> itemsToRemoveCompletely;

    QList<QGraphicsItem*> intersectingItems = items(eraserQtPath);

    for (QGraphicsItem* item : intersectingItems) {
        if (auto stroke = dynamic_cast<StrokeItem*>(item)) {
            if (!stroke->isOutlined()) {
                stroke->convertToFilledPath();
            }

            Clipper2Lib::Path64 strokePath = DrawingEngineUtils::convertPathToClipper(stroke->path());
            Clipper2Lib::Paths64 subj, clip, solution;
            subj.push_back(strokePath);
            clip.push_back(eraserClipperPath);

            Clipper2Lib::Clipper64 clipper;
            clipper.PreserveCollinear(true);
            clipper.AddSubject(subj);
            clipper.AddClip(clip);
            clipper.Execute(Clipper2Lib::ClipType::Difference, Clipper2Lib::FillRule::NonZero, solution);

            // Original item was affected
            originalItemsAffected.append(stroke);

            if (solution.empty()) {
                 // Item will be completely removed, handled by command later
                 itemsToRemoveCompletely.append(stroke);
            } else {
                // Convert solution back to new StrokeItems
                QColor originalColor = stroke->color();
                std::vector<std::pair<QPainterPath, std::vector<QPainterPath>>> shapes;
                for (const auto& path : solution) {
                    if (path.size() < 3) continue;
                    QPainterPath qtPath = DrawingEngineUtils::convertSingleClipperPath(path);
                    bool isOuter = Clipper2Lib::Area(path) > 0;
                    if (isOuter) {
                        shapes.push_back(std::make_pair(qtPath, std::vector<QPainterPath>()));
                    } else {
                        if (!shapes.empty()) {
                            shapes.back().second.push_back(qtPath);
                        }
                    }
                }
                for (const auto& shapePair : shapes) {
                    QPainterPath fullPath = shapePair.first;
                    for (const auto& hole : shapePair.second) {
                        fullPath = fullPath.subtracted(hole);
                    }
                    StrokeItem* newStroke = new StrokeItem(originalColor, 0);
                    newStroke->setPath(fullPath);
                    newStroke->setBrush(QBrush(originalColor));
                    newStroke->setPen(QPen(originalColor.darker(120), 0.5));
                    newStroke->setOutlined(true);
                    resultingItems.append(newStroke);
                }
            }
            // Don't remove original stroke here; command will handle it.
            // removeItem(stroke);
            // delete stroke;
        }
    }

    // Reset state
    m_points.clear();

    // Only create command if something changed
    if (!originalItemsAffected.isEmpty()) {
         // Create Erase command
        EraseCommand* cmd = new EraseCommand(this, originalItemsAffected, resultingItems);
        pushCommand(cmd); // This will call redo(), removing originals and adding results
    }
}

// Fill Implementation
// Apply fill at the clicked position
void DrawingScene::applyFill(const QPointF& pos) {
    QRectF sceneRect = this->sceneRect();
    QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    render(&painter);
    painter.end();
    int x = qRound(pos.x() - sceneRect.left());
    int y = qRound(pos.y() - sceneRect.top());
    if (x < 0 || x >= image.width() || y < 0 || y >= image.height()) return;
    QRgb targetColor = image.pixel(x, y);
    QVector<QPoint> fillPoints;
    QVector<bool> visited(image.width() * image.height(), false);
    QQueue<QPoint> queue;
    int tolerance = 30;
    queue.enqueue(QPoint(x, y));
    while (!queue.isEmpty()) {
        QPoint p = queue.dequeue();
        if (p.x() < 0 || p.x() >= image.width() || p.y() < 0 || p.y() >= image.height() ||
            visited[p.y() * image.width() + p.x()]) continue;
        QRgb currentColor = image.pixel(p.x(), p.y());
        bool similar =
            qAbs(qRed(currentColor) - qRed(targetColor)) <= tolerance &&
            qAbs(qGreen(currentColor) - qGreen(targetColor)) <= tolerance &&
            qAbs(qBlue(currentColor) - qBlue(targetColor)) <= tolerance;
        if (!similar) continue;
        visited[p.y() * image.width() + p.x()] = true;
        fillPoints.append(p);
        queue.enqueue(QPoint(p.x() + 1, p.y()));
        queue.enqueue(QPoint(p.x() - 1, p.y()));
        queue.enqueue(QPoint(p.x(), p.y() + 1));
        queue.enqueue(QPoint(p.x(), p.y() - 1));
    }
    if (fillPoints.isEmpty()) return;
    // OPTIMIZATION: Convert filled pixels to horizontal spans
    struct Span { int y, x1, x2; bool operator<(const Span& other) const { if (y != other.y) return y < other.y; return x1 < other.x1; } };
    std::sort(fillPoints.begin(), fillPoints.end(), [](const QPoint& a, const QPoint& b) { if (a.y() != b.y()) return a.y() < b.y(); return a.x() < b.x(); });
    QVector<Span> spans;
    int currentY = -1, spanStart = -1;
    for (int i = 0; i < fillPoints.size(); ++i) {
        QPoint p = fillPoints[i];
        if (p.y() != currentY) { currentY = p.y(); spanStart = p.x(); }
        else if (p.x() != fillPoints[i - 1].x() + 1) { spans.append({ currentY, spanStart, fillPoints[i - 1].x() }); spanStart = p.x(); }
        if (i == fillPoints.size() - 1 || fillPoints[i + 1].y() != currentY) { spans.append({ currentY, spanStart, p.x() }); }
    }

    Clipper2Lib::Paths64 paths;
    const double padding = 0.1;
    const int maxSpans = 5000;
    if (spans.size() > maxSpans) spans.resize(maxSpans);
    for (const Span& span : spans) {
        double sceneY1 = span.y + sceneRect.top() - padding, sceneY2 = span.y + sceneRect.top() + 1.0 + padding;
        double sceneX1 = span.x1 + sceneRect.left() - padding, sceneX2 = span.x2 + sceneRect.left() + 1.0 + padding;
        Clipper2Lib::Path64 rect;
        rect.push_back(Clipper2Lib::Point64(static_cast<int64_t>(sceneX1 * CLIPPER_SCALING), static_cast<int64_t>(sceneY1 * CLIPPER_SCALING)));
        rect.push_back(Clipper2Lib::Point64(static_cast<int64_t>(sceneX2 * CLIPPER_SCALING), static_cast<int64_t>(sceneY1 * CLIPPER_SCALING)));
        rect.push_back(Clipper2Lib::Point64(static_cast<int64_t>(sceneX2 * CLIPPER_SCALING), static_cast<int64_t>(sceneY2 * CLIPPER_SCALING)));
        rect.push_back(Clipper2Lib::Point64(static_cast<int64_t>(sceneX1 * CLIPPER_SCALING), static_cast<int64_t>(sceneY2 * CLIPPER_SCALING)));
        paths.push_back(rect);
    }

    Clipper2Lib::Clipper64 clipper;
    clipper.PreserveCollinear(true);
    clipper.AddSubject(paths);
    Clipper2Lib::Paths64 solution;
    clipper.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, solution);

    if (!solution.empty()) {
        QPainterPath fillPath;
        for (const auto& path : solution) {
            if (path.size() < 3) continue;
            QPainterPath subPath = DrawingEngineUtils::convertSingleClipperPath(path);
            if (fillPath.isEmpty()) fillPath = subPath;
            else { if (Clipper2Lib::Area(path) < 0) fillPath = fillPath.subtracted(subPath); else fillPath = fillPath.united(subPath); }
        }

        // Create stroke item
        StrokeItem* fill = new StrokeItem(m_brushColor, 0);
        fill->setPath(fillPath);
        fill->setBrush(QBrush(m_brushColor)); // Ensure brush is set for filled shape
        fill->setPen(QPen(m_brushColor.darker(120), 0.5)); // Optional outline
        fill->setOutlined(true);

        // Create Add command
        AddCommand* cmd = new AddCommand(this, fill);
        // Don't add item directly: addItem(fill);
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
            clickedOnAnyItem = true;
            
            if (m_selectedItems.contains(stroke)) {
                clickedOnSelected = true;
                m_isMovingSelection = true;
                m_lastMousePos = pos;
                break;
            }
            else if (!(QApplication::keyboardModifiers() & Qt::ShiftModifier)) {
                // If clicking on an unselected item without Shift, select only this item
                clearSelection();
                m_selectedItems.append(stroke);
                highlightSelectedItems(true);
                m_isMovingSelection = true;
                m_lastMousePos = pos;
                return;
            }
            else {
                // If clicking with Shift, add this item to the selection
                m_selectedItems.append(stroke);
                highlightSelectedItems(true);
                m_isMovingSelection = true;
                m_lastMousePos = pos;
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
        QRectF rect = QRectF(
            QPointF(qMin(m_selectionStartPos.x(), pos.x()), qMin(m_selectionStartPos.y(), pos.y())),
            QPointF(qMax(m_selectionStartPos.x(), pos.x()), qMax(m_selectionStartPos.y(), pos.y()))
        );
        m_selectionRect->setRect(rect);
    }
    else if (m_isMovingSelection) {
        QPointF delta = pos - m_lastMousePos;

        if (!m_selectedItems.isEmpty() && delta.manhattanLength() > 0.01) { 
             bool merged = false;
             if (m_undoStack && m_undoStack->count() > 0) {
                 const QUndoCommand* topConstCmd = m_undoStack->command(m_undoStack->count() - 1);

                 // Try to cast away const to get a modifiable MoveCommand pointer
                 // Use const_cast carefully, assuming commands pushed are modifiable
                 MoveCommand* lastCmd = dynamic_cast<MoveCommand*>(const_cast<QUndoCommand*>(topConstCmd));

                 if (lastCmd && lastCmd->id() == 1 /* Move Command ID */) {
                     // Create a temporary command representing the current delta ONLY for checking mergeability and passing data
                     MoveCommand tempCmd(this, m_selectedItems, delta); // Don't push this

                     // Check if the last command on the stack can merge with the current delta info
                     if (lastCmd->mergeWith(&tempCmd)) { // Call mergeWith on the command *already on the stack*
                         // Merge successful: lastCmd on stack has updated its *total* delta.
                         merged = true;

                         // Apply the VISUAL move for the CURRENT delta immediately.
                         // The merged command (lastCmd) handles the total move on future undo/redo.
                         for (StrokeItem* item : m_selectedItems) {
                             if (item->scene() == this) { // Check item still exists
                                 item->moveBy(delta.x(), delta.y());
                             }
                         }

                         // Update the command text on the stack to reflect potentially larger move
                         lastCmd->setText(QString("Move %1 shape(s)").arg(lastCmd->itemCount()));
                     }
                 }
             }

             if (!merged) {
                 MoveCommand* cmd = new MoveCommand(this, m_selectedItems, delta);
                 pushCommand(cmd);
             }
            m_lastMousePos = pos; 
        }
        else if (!m_selectedItems.isEmpty()){ 
            m_lastMousePos = pos;
        }
    }
}

void DrawingScene::finalizeSelection() {
    if (m_isSelecting && m_selectionRect) {
        // Get items within selection rectangle
        QList<QGraphicsItem*> itemsInRect = items(m_selectionRect->rect());

        for (QGraphicsItem* item : itemsInRect) {
            if (auto stroke = dynamic_cast<StrokeItem*>(item)) {
                if (!m_selectedItems.contains(stroke)) {
                    m_selectedItems.append(stroke);
                }
            }
        }

        // Hide selection rectangle
        m_selectionRect->hide();
        m_isSelecting = false;

        // Highlight selected items
        highlightSelectedItems(true);
    }
    else if (m_isMovingSelection) {
        // Now that move is done via commands during updateSelection,
        // we might just need to ensure the stack compression happens
        // by setting the clean index, or just reset the flag.
        // QUndoStack handles merging based on consecutive pushes.
        m_isMovingSelection = false;

        // Optional: Set stack clean index to allow further merges
        // if (m_undoStack) m_undoStack->setIndex(m_undoStack->index());
    }
}

void DrawingScene::clearSelection() {
    highlightSelectedItems(false);
    m_selectedItems.clear();
}

void DrawingScene::highlightSelectedItems(bool highlight) {
    for (StrokeItem* item : m_selectedItems) {
        if (highlight) {
            // Store original pen and use a highlighted pen
            item->setSelected(true);
            item->setZValue(item->zValue() + 0.1); // Bring slightly forward
        }
        else {
            item->setSelected(false);
            item->setZValue(item->zValue() - 0.1); // Restore z-order
        }
    }
}


void DrawingScene::keyPressEvent(QKeyEvent* event) {
    if (m_currentTool == Select && !m_selectedItems.isEmpty()) {
        int key = event->key();

        // Check if this is a new key press
        bool isNewKeyPress = !m_keysPressed.contains(key) || !m_keysPressed[key];

        // Store the key press state
        if (isNewKeyPress) {
            m_keysPressed[key] = true;
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
            for (StrokeItem* item : m_selectedItems) {
                removeItem(item);
                delete item;
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

    // Mark key as released
    if (m_keysPressed.contains(key)) {
        m_keysPressed[key] = false;

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

    QGraphicsScene::keyReleaseEvent(event);
}

DrawingScene::~DrawingScene() {
    // Clean up selection rectangle if it exists
    if (m_selectionRect) {
        delete m_selectionRect;
        m_selectionRect = nullptr;
    }
}