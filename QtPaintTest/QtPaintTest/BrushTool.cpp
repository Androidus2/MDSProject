#include "BrushTool.h"
#include "DrawingScene.h"
#include "DrawingManager.h"
#include "AddCommand.h"

BrushTool::BrushTool()
    : m_cooldownInterval(50),
    m_tangentStrength(0.4),
    m_smoothingFactor(0.65),
    m_smoothingWindowSize(4),
    m_minDistance(1.0),
    m_preserveCurves(true),
    m_showPrediction(true),
    m_cursorIndicator(nullptr) {
    m_cooldownTimer.setInterval(m_cooldownInterval);
    connect(&m_cooldownTimer, &QTimer::timeout, this, &BrushTool::commitBrushSegment);
}

BrushTool::~BrushTool() {
    // Cleanup
    if (m_tempPathItem) {
        delete m_tempPathItem;
        m_tempPathItem = nullptr;
    }
    if (m_currentPath) {
        delete m_currentPath;
        m_currentPath = nullptr;
    }
    if (m_cursorIndicator) {
        // Make sure to remove from scene before deleting
        if (m_cursorIndicator->scene()) {
            m_cursorIndicator->scene()->removeItem(m_cursorIndicator);
        }
        delete m_cursorIndicator;
        m_cursorIndicator = nullptr;
    }
}

void BrushTool::ensureCursorIndicator() {
    // Always create a fresh cursor indicator to avoid any issues
    if (m_cursorIndicator) {
        if (m_cursorIndicator->scene()) {
            m_cursorIndicator->scene()->removeItem(m_cursorIndicator);
        }
        delete m_cursorIndicator;
        m_cursorIndicator = nullptr;
    }

    // Create a new indicator
    m_cursorIndicator = new QGraphicsEllipseItem();
    QPen cursorPen(Qt::white);
    cursorPen.setWidth(1);
    m_cursorIndicator->setPen(cursorPen);
    m_cursorIndicator->setBrush(Qt::transparent);

    // Set z-value to ensure it stays on top
    m_cursorIndicator->setZValue(1000);

    // Add to scene
    DrawingManager::getInstance().getScene()->addItem(m_cursorIndicator);
}

void BrushTool::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    // Start the brush stroke
    startBrushStroke(event->scenePos());
    event->accept();
}

void BrushTool::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    // Always update cursor position even if not drawing
    QPointF pos = event->scenePos();
    if (m_cursorIndicator && m_cursorIndicator->isVisible()) {
        qreal brushSize = DrawingManager::getInstance().getWidth();
        m_cursorIndicator->setRect(pos.x() - brushSize / 2,
            pos.y() - brushSize / 2,
            brushSize, brushSize);
    }

    // Update the brush stroke if active
    if (m_currentPath) {
        updateBrushStroke(pos);
    }

    event->accept();
}

void BrushTool::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    // Finalize the brush stroke
    finalizeBrushStroke();
    event->accept();
}

void BrushTool::keyPressEvent(QKeyEvent* event) {
    // Handle key events if needed
}

void BrushTool::keyReleaseEvent(QKeyEvent* event) {
    // Handle key events if needed
}

QPointF BrushTool::smoothPoint(const QPointF& newPoint) {
    // Don't apply smoothing if we don't have enough history
    if (m_pointHistory.empty()) {
        return newPoint;
    }

    // Fast smoothing algorithm - performance optimized
    // Uses only the most recent point with a weight blend
    //QPointF lastPoint = m_pointHistory.back();
    //QPointF smoothedPoint = lastPoint * m_smoothingFactor + newPoint * (1.0 - m_smoothingFactor);

    // For higher quality (but slower) smoothing
    QPointF smoothedPoint = newPoint * (1.0 - m_smoothingFactor);
    double totalWeight = 1.0 - m_smoothingFactor;
    double stepWeight = m_smoothingFactor / m_pointHistory.size();

    for (const QPointF& point : m_pointHistory) {
        smoothedPoint += point * stepWeight;
        totalWeight += stepWeight;
    }

    // Normalize by total weight
    if (totalWeight > 0) {
        smoothedPoint /= totalWeight;
    }

    return smoothedPoint;
}

void BrushTool::commitBrushSegment() {
    // Commit the current brush segment
    commitSegment(m_currentPath, m_tempPathItem, m_realPath);
}

void BrushTool::commitSegment(StrokeItem* pathItem, QGraphicsPathItem* tempItem, QPainterPath& realPath) {
    if (!pathItem || m_points.size() < 2) return;

    QPointF start = realPath.currentPosition();
    QPointF end = m_points.last();

    // Performance optimization: only calculate tangents when needed
    // Calculate tangent directions for smooth curves
    QVector2D startDir = calculateTangent(0, qMin(3, m_points.size() - 1));
    QVector2D endDir = calculateTangent(qMax(0, m_points.size() - 3), qMin(3, m_points.size() - 1));

    // Scale control points based on distance and brush width for better curve shape
    float segmentLength = QVector2D(end - start).length();
    float brushWidth = DrawingManager::getInstance().getWidth();
    float scaleFactor = qMin(segmentLength * 0.4f, brushWidth * 2.0f);

    // Calculate control points for cubic Bezier curve
    QPointF c1 = start + (startDir * m_tangentStrength * scaleFactor).toPointF();
    QPointF c2 = end - (endDir * m_tangentStrength * scaleFactor).toPointF();

    // Add cubic curve to the path
    realPath.cubicTo(c1, c2, end);
    pathItem->setPath(realPath);

    // Keep only the last point for the next segment
    m_points = { end };

    // Update the temporary path with prediction
    updateTemporaryPath(tempItem, m_lastCursorPos);
}

QVector2D BrushTool::calculateTangent(int startIndex, int count) {
    // Performance optimization: avoid unnecessary calculations
    startIndex = qBound(0, startIndex, m_points.size() - 2);
    const int availablePoints = m_points.size() - startIndex - 1;
    count = qMin(count, availablePoints);

    if (count < 1) return QVector2D(1, 0);

    QVector2D avgDirection(0, 0);
    for (int i = 0; i < count; i++) {
        const int idx = startIndex + i;
        if (idx >= 0 && idx < m_points.size() - 1) {
            QVector2D segDir = QVector2D(m_points[idx + 1] - m_points[idx]);
            // Weight closer points more heavily
            float weight = 1.0f - (i / static_cast<float>(count));
            avgDirection += segDir.normalized() * weight;
        }
    }

    return avgDirection.normalized();
}

void BrushTool::optimizePath(QPainterPath& path, StrokeItem* pathItem) {
    // Skip optimization for very short paths - performance optimization
    if (path.elementCount() < 4) return;

    QPainterPath newPath;
    const QPainterPath::Element& firstEl = path.elementAt(0);
    QPointF lastPoint(firstEl.x, firstEl.y);
    newPath.moveTo(lastPoint);

    // Performance optimization: pre-calculate thresholds once
    qreal brushWidth = DrawingManager::getInstance().getWidth();
    qreal simplifyThreshold = brushWidth * 0.5;
    qreal curveThreshold = simplifyThreshold * 2.5;

    // Process each cubic segment
    for (int i = 1; i < path.elementCount(); i += 3) {
        if (i + 2 >= path.elementCount()) break;

        QPointF c1(path.elementAt(i).x, path.elementAt(i).y);
        QPointF c2(path.elementAt(i + 1).x, path.elementAt(i + 1).y);
        QPointF end(path.elementAt(i + 2).x, path.elementAt(i + 2).y);

        // Performance optimization: simplified curve analysis
        // Check just one point at t=0.5 instead of multiple t values
        QPointF bezierMidPoint = lastPoint * 0.125 + c1 * 0.375 + c2 * 0.375 + end * 0.125;
        QPointF lineMidPoint = lastPoint + (end - lastPoint) * 0.5;
        qreal deviation = QLineF(bezierMidPoint, lineMidPoint).length();

        // Decision logic with better curve preservation
        if (m_preserveCurves && deviation > curveThreshold) {
            // Significant curve, preserve the cubic Bezier
            newPath.cubicTo(c1, c2, end);
        }
        else if (deviation < simplifyThreshold * 0.5) {
            // Nearly straight segment, use line
            newPath.lineTo(end);
        }
        else {
            // Moderately curved - use quadratic curve
            QPointF qc = bezierMidPoint + (bezierMidPoint - lineMidPoint) * 0.5;
            newPath.quadTo(qc, end);
        }

        lastPoint = end;
    }

    path = newPath;
    pathItem->setPath(path);
}

void BrushTool::updateTemporaryPath(QGraphicsPathItem* tempItem, const QPointF& cursorPos) {
    if (!tempItem || m_points.isEmpty()) return;

    QPainterPath tempPath;
    tempPath.moveTo(m_points.first());

    // Create smooth temporary path for existing points
    if (m_points.size() > 1) {
        for (int i = 1; i < m_points.size(); ++i) {
            tempPath.lineTo(m_points[i]);
        }
    }

    // Add predictive segment to cursor position
    if (m_showPrediction && !m_points.isEmpty()) {
        QPointF lastPoint = m_points.last();

        // Calculate distance to cursor
        qreal distance = QLineF(lastPoint, cursorPos).length();

        // Only show prediction if cursor is far enough from last point
        if (distance > m_minDistance * 2) {
            // Calculate direction vector from last points
            QVector2D direction;
            if (m_points.size() > 1) {
                direction = calculateTangent(qMax(0, m_points.size() - 3), qMin(3, m_points.size() - 1));
            }
            else {
                direction = QVector2D(cursorPos - lastPoint).normalized();
            }

            // Calculate control point for smooth prediction
            QPointF controlPoint = lastPoint + (direction * distance * 0.5).toPointF();

            // Add prediction curve
            tempPath.quadTo(controlPoint, cursorPos);
        }
    }

    tempItem->setPath(tempPath);

    // Update cursor indicator position
    if (m_cursorIndicator) {
        qreal brushSize = DrawingManager::getInstance().getWidth();
        m_cursorIndicator->setRect(cursorPos.x() - brushSize / 2,
            cursorPos.y() - brushSize / 2,
            brushSize, brushSize);
    }
}

void BrushTool::startBrushStroke(const QPointF& pos) {
    // Create the real path item
    m_currentPath = new StrokeItem(DrawingManager::getInstance().getColor(), DrawingManager::getInstance().getWidth());
    DrawingManager::getInstance().getScene()->addItem(m_currentPath);

    // Create the temporary path item for visual feedback
    m_tempPathItem = new QGraphicsPathItem();
    QPen tempPen(DrawingManager::getInstance().getColor(), DrawingManager::getInstance().getWidth());
    tempPen.setCapStyle(Qt::RoundCap);
    tempPen.setJoinStyle(Qt::RoundJoin);
    m_tempPathItem->setPen(tempPen);
    DrawingManager::getInstance().getScene()->addItem(m_tempPathItem);

    // Ensure we have a valid cursor indicator
    ensureCursorIndicator();

    // Update and show cursor indicator
    qreal brushSize = DrawingManager::getInstance().getWidth();
    m_cursorIndicator->setRect(pos.x() - brushSize / 2, pos.y() - brushSize / 2,
        brushSize, brushSize);
    m_cursorIndicator->show();

    // Reset point collection and paths
    m_points.clear();
    m_pointHistory.clear();
    m_points << pos;
    m_pointHistory.push_back(pos);
    m_lastCursorPos = pos;
    m_realPath = QPainterPath();
    m_realPath.moveTo(pos);
    m_currentPath->setPath(m_realPath);

    // Start the cooldown timer
    m_cooldownTimer.start();
}

void BrushTool::updateBrushStroke(const QPointF& pos) {
    if (!m_currentPath) return;

    // Store cursor position for prediction
    m_lastCursorPos = pos;

    // Apply input smoothing
    QPointF smoothed = smoothPoint(pos);

    // Only add point if it's far enough from the last point
    if (m_points.isEmpty() ||
        QVector2D(smoothed - m_points.last()).length() >= m_minDistance) {

        m_points << smoothed;

        // Add to history buffer, maintaining max size
        m_pointHistory.push_back(smoothed);
        while (m_pointHistory.size() > m_smoothingWindowSize) {
            m_pointHistory.pop_front();
        }

        // Update with prediction to cursor position
        updateTemporaryPath(m_tempPathItem, pos);
    }
    else {
        // Still update cursor indicator even if we don't add a new point
        if (m_cursorIndicator) {
            qreal brushSize = DrawingManager::getInstance().getWidth();
            m_cursorIndicator->setRect(pos.x() - brushSize / 2,
                pos.y() - brushSize / 2,
                brushSize, brushSize);
        }
    }
}

void BrushTool::finalizeBrushStroke() {
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
        circlePath.addEllipse(m_points.first(), DrawingManager::getInstance().getWidth() / 2, DrawingManager::getInstance().getWidth() / 2);
        m_currentPath->setPath(circlePath);
    }

    // Don't hide cursor indicator - keep it visible between strokes
    // if (m_cursorIndicator) {
    //     m_cursorIndicator->hide();
    // }

    // Perform final path optimization with curve preservation
    optimizePath(m_realPath, m_currentPath);

    // Convert to filled path
    m_currentPath->convertToFilledPath();

    // Remove from scene before creating command
    DrawingManager::getInstance().getScene()->removeItem(m_currentPath);

    // Create a command to add the path to the scene
    AddCommand* cmd = new AddCommand(DrawingManager::getInstance().getScene(), m_currentPath);

    // Clean up
    if (m_tempPathItem) {
        DrawingManager::getInstance().getScene()->removeItem(m_tempPathItem);
        delete m_tempPathItem;
        m_tempPathItem = nullptr;
    }

    // Store and reset state before pushing command
    StrokeItem* itemToAdd = m_currentPath;
    m_currentPath = nullptr;
    m_points.clear();
    m_pointHistory.clear();

    // Push the command to the undo stack
    DrawingManager::getInstance().pushCommand(cmd);
}