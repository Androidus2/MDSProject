#include "DrawingScene.h"

DrawingScene::DrawingScene(QObject* parent)
    : QGraphicsScene(parent), m_currentTool(Brush),
    m_brushColor(Qt::black), m_brushWidth(15),
    m_cooldownInterval(100), m_tangentStrength(0.33) {

    m_cooldownTimer.setInterval(m_cooldownInterval);
    connect(&m_cooldownTimer, &QTimer::timeout, this, &DrawingScene::commitBrushSegment);
}

// Set the current tool
void DrawingScene::setTool(ToolType tool) { m_currentTool = tool; }

// Add color getter and setter
void DrawingScene::setColor(const QColor& color) { m_brushColor = color; }
QColor DrawingScene::currentColor() const { return m_brushColor; }

// Add brush width setter
void DrawingScene::setBrushWidth(qreal width) { m_brushWidth = width; }

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
    // Convert the stroke path to Clipper format
    Clipper2Lib::Path64 strokePath = DrawingEngineUtils::convertPathToClipper(stroke->path());

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

    bool clickedOnSelected = false;
    for (QGraphicsItem* item : itemsAtPos) {
        if (auto stroke = dynamic_cast<StrokeItem*>(item)) {
            if (m_selectedItems.contains(stroke)) {
                clickedOnSelected = true;
                m_isMovingSelection = true;
                m_lastMousePos = pos;
                break;
            }
        }
    }

    // If not clicking on selected item, start new selection
    if (!clickedOnSelected) {
        // Clear previous selection
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
    }
    else if (m_isMovingSelection) {
        m_isMovingSelection = false;
    }
}

void DrawingScene::moveSelectedItems(const QPointF& newPos) {
    if (m_selectedItems.isEmpty()) return;

    QPointF delta = newPos - m_lastMousePos;
    for (StrokeItem* item : m_selectedItems) {
        item->moveBy(delta.x(), delta.y());
    }
    m_lastMousePos = newPos;
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

// Add this to your DrawingScene class
void DrawingScene::keyPressEvent(QKeyEvent* event) {
    if (m_currentTool == Select && !m_selectedItems.isEmpty()) {
        switch (event->key()) {
        case Qt::Key_Delete:
            // Delete selected items
            for (StrokeItem* item : m_selectedItems) {
                removeItem(item);
                delete item;
            }
            m_selectedItems.clear();
            break;

        case Qt::Key_Left:
            // Move selection left
            for (StrokeItem* item : m_selectedItems) {
                item->moveBy(-1, 0);
            }
            break;

        case Qt::Key_Right:
            // Move selection right
            for (StrokeItem* item : m_selectedItems) {
                item->moveBy(1, 0);
            }
            break;

        case Qt::Key_Up:
            // Move selection up
            for (StrokeItem* item : m_selectedItems) {
                item->moveBy(0, -1);
            }
            break;

        case Qt::Key_Down:
            // Move selection down
            for (StrokeItem* item : m_selectedItems) {
                item->moveBy(0, 1);
            }
            break;
        }
    }

    QGraphicsScene::keyPressEvent(event);
}

DrawingScene::~DrawingScene() {
    // Clean up selection rectangle if it exists
    if (m_selectionRect) {
        delete m_selectionRect;
        m_selectionRect = nullptr;
    }
}