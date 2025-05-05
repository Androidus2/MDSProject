#include "DrawingScene.h"
#include <fstream>

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

    // Clean up
    if (m_tempPathItem) {
        removeItem(m_tempPathItem);
        delete m_tempPathItem;
        m_tempPathItem = nullptr;
    }

    m_currentPath = nullptr;
    m_points.clear();
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

    // Process only the strokes that the eraser actually intersects
    for (QGraphicsItem* item : intersectingItems) {
        if (auto stroke = dynamic_cast<StrokeItem*>(item)) {
            // Make sure the stroke is converted to filled path if not already
            if (!stroke->isOutlined()) {
                stroke->convertToFilledPath();
            }

            // Apply boolean difference using Clipper2
            processEraserOnStroke(stroke, eraserClipperPath);
        }
    }

    m_points.clear();
}
// Process the eraser on a single stroke
void DrawingScene::processEraserOnStroke(StrokeItem* stroke, const Clipper2Lib::Path64& eraserPath) {
    // If the stroke is being transformed, apply transform to its path first
    QPainterPath strokeScenePath = stroke->path();

    // Apply any transform to get scene coordinates
    if (!stroke->transform().isIdentity() || stroke->pos() != QPointF(0, 0)) {
        QTransform sceneTransform = stroke->sceneTransform();
        strokeScenePath = sceneTransform.map(strokeScenePath);
    }

    // Convert the stroke path to Clipper format
    Clipper2Lib::Path64 strokePath = DrawingEngineUtils::convertPathToClipper(strokeScenePath);

    // Create paths collections
    Clipper2Lib::Paths64 subj, clip, solution;
    subj.push_back(strokePath);
    clip.push_back(eraserPath);

    // Perform the difference operation
    Clipper2Lib::Clipper64 clipper;
    clipper.PreserveCollinear(true);
    clipper.AddSubject(subj);
    clipper.AddClip(clip);

    clipper.Execute(Clipper2Lib::ClipType::Difference,
        Clipper2Lib::FillRule::NonZero,
        solution);

    // If nothing remains, remove the stroke
    if (solution.empty()) {
        removeItem(stroke);
        delete stroke;
        return;
    }

    // Get original color
    QColor originalColor = stroke->color();

    // Remove the original stroke
    removeItem(stroke);
    delete stroke;

    // Group paths by outer/inner contours based on their orientation
    std::vector<std::pair<QPainterPath, std::vector<QPainterPath>>> shapes;

    for (const auto& path : solution) {
        if (path.size() < 3) continue; // Skip too small paths

        // Convert to Qt path
        QPainterPath qtPath = DrawingEngineUtils::convertSingleClipperPath(path);

        // Check if this is an outer or inner contour based on area
        bool isOuter = Clipper2Lib::Area(path) > 0;

        if (isOuter) {
            // This is an outer contour, add it as a new shape
            shapes.push_back(std::make_pair(qtPath, std::vector<QPainterPath>()));
        }
        else {
            // This is a hole, add to the most recent outer contour
            if (!shapes.empty()) {
                shapes.back().second.push_back(qtPath);
            }
        }
    }

    // Create strokes for each shape with its holes
    for (const auto& shapePair : shapes) {
        QPainterPath fullPath = shapePair.first;

        // Add holes to the path using subtraction
        for (const auto& hole : shapePair.second) {
            fullPath = fullPath.subtracted(hole);
        }

        // Create a new stroke with the complex path
        StrokeItem* newStroke = new StrokeItem(originalColor, 0);
        newStroke->setPath(fullPath);
        newStroke->setBrush(QBrush(originalColor));
        newStroke->setPen(QPen(originalColor.darker(120), 0.5));
        newStroke->setOutlined(true);

        addItem(newStroke);
    }
}

// Fill Implementation
// Apply fill at the clicked position
void DrawingScene::applyFill(const QPointF& pos) {
    // Create a temporary image of the current scene
    QRectF sceneRect = this->sceneRect();
    QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    render(&painter);
    painter.end();

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

        // Create a yellow fill item with this path
        StrokeItem* fill = new StrokeItem(m_brushColor, 0); // Instead of Qt::yellow
        fill->setPath(fillPath);
        addItem(fill);
        fill->setPath(fillPath);
        addItem(fill);

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
        for (StrokeItem* item : m_selectedItems) {
            item->moveBy(delta.x(), delta.y());
        }
        m_lastMousePos = pos;
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

        // Create transform handles
        if (!m_selectedItems.isEmpty()) {
            createSelectionBox();
        }
    }
    else if (m_isMovingSelection) {
        m_isMovingSelection = false;

        // Update transform handles
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
            // Clean up selection UI elements
            removeSelectionBox();
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