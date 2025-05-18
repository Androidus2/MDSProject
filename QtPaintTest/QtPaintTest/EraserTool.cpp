#include "EraserTool.h"
#include "DrawingScene.h"
#include "DrawingManager.h"
#include "EraseCommand.h"

EraserTool::EraserTool()
	: m_cooldownInterval(100), m_tangentStrength(0.33) {
	m_cooldownTimer.setInterval(m_cooldownInterval);
	connect(&m_cooldownTimer, &QTimer::timeout, this, &EraserTool::commitEraserSegment);
}

EraserTool::~EraserTool() {
	// Cleanup
	if (m_tempEraserPathItem) {
		delete m_tempEraserPathItem;
		m_tempEraserPathItem = nullptr;
	}
	if (m_currentEraserPath) {
		delete m_currentEraserPath;
		m_currentEraserPath = nullptr;
	}
}

void EraserTool::mousePressEvent(QGraphicsSceneMouseEvent* event) {
	if (event->button() != Qt::LeftButton) return;
	// Start the eraser stroke
	startEraserStroke(event->scenePos());
	event->accept();
}
void EraserTool::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
	// Update the eraser stroke
	updateEraserStroke(event->scenePos());
	event->accept();
}
void EraserTool::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
	// Finalize the eraser stroke
	finalizeEraserStroke();
	event->accept();
}

void EraserTool::keyPressEvent(QKeyEvent* event) {
	// Handle key events if needed
}
void EraserTool::keyReleaseEvent(QKeyEvent* event) {
	// Handle key events if needed
}

void EraserTool::commitEraserSegment() {
	// Commit the current eraser segment
	commitSegment(m_currentEraserPath, m_tempEraserPathItem, m_eraserRealPath);
}

void EraserTool::commitSegment(StrokeItem* pathItem, QGraphicsPathItem* tempItem, QPainterPath& realPath) {
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
QVector2D EraserTool::calculateTangent(int startIndex, int count) {
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
void EraserTool::optimizePath(QPainterPath& path, StrokeItem* pathItem) {
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
	qreal avgSegmentLength = segments > 0 ? totalLength / segments : DrawingManager::getInstance().getWidth() * 3;
	qreal simplifyThreshold = qMin(DrawingManager::getInstance().getWidth() * 0.75, avgSegmentLength * 0.3);

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
		if (deviation < simplifyThreshold && segmentLength < DrawingManager::getInstance().getWidth() * 3) {
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
void EraserTool::updateTemporaryPath(QGraphicsPathItem* tempItem) {
	if (!tempItem) return;

	QPainterPath tempPath;
	if (m_points.isEmpty()) return;

	tempPath.moveTo(m_points.first());
	for (int i = 1; i < m_points.size(); ++i) {
		tempPath.lineTo(m_points[i]);
	}

	tempItem->setPath(tempPath);
}

void EraserTool::startEraserStroke(const QPointF& pos) {
    // Create a visible eraserPath item
    m_currentEraserPath = new StrokeItem(Qt::red, DrawingManager::getInstance().getWidth());
    m_currentEraserPath->setOpacity(0.5); // Semi-transparent
    DrawingManager::getInstance().getScene()->addItem(m_currentEraserPath);

    // Create temporary path for visual feedback
    m_tempEraserPathItem = new QGraphicsPathItem();
    QPen tempPen(Qt::red, DrawingManager::getInstance().getWidth());
    //tempPen.setStyle(Qt::DotLine);
    QColor tempColor = Qt::red;
    tempColor.lighter(150); // Make temp path slightly lighter
    tempPen.setColor(tempColor); // Make temp path slightly lighter
    m_tempEraserPathItem->setPen(tempPen);
    m_tempEraserPathItem->setOpacity(0.5);
    DrawingManager::getInstance().getScene()->addItem(m_tempEraserPathItem);

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
void EraserTool::updateEraserStroke(const QPointF& pos) {
    if (!m_currentEraserPath) return;

    m_points << pos;
    updateTemporaryPath(m_tempEraserPathItem);
}
// Finalize the eraser stroke
void EraserTool::finalizeEraserStroke() {
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
        circlePath.addEllipse(m_points.first(), DrawingManager::getInstance().getWidth() / 2, DrawingManager::getInstance().getWidth() / 2);
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
        DrawingManager::getInstance().getScene()->removeItem(m_tempEraserPathItem);
        delete m_tempEraserPathItem;
        m_tempEraserPathItem = nullptr;
    }

    // Remove the eraser path but keep its data for processing
    DrawingManager::getInstance().getScene()->removeItem(m_currentEraserPath);
    delete m_currentEraserPath;
    m_currentEraserPath = nullptr;

    // Get items that intersect with the eraser
    QList<QGraphicsItem*> intersectingItems = DrawingManager::getInstance().getScene()->items(eraserQtPath);

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
        EraseCommand* cmd = new EraseCommand(DrawingManager::getInstance().getScene(), originalItemsAffected, resultingItems);
        DrawingManager::getInstance().pushCommand(cmd);
    }
}
// Process the eraser on a single stroke
// This function should be removed or updated to use the command pattern
void EraserTool::processEraserOnStroke(StrokeItem* stroke, const Clipper2Lib::Path64& eraserPath) {
    // Get the stroke path with transform applied
    QPainterPath strokeScenePath = stroke->path();

    // Apply any transform to get scene coordinates
    if (!stroke->transform().isIdentity() || stroke->pos() != QPointF(0, 0)) {
        QTransform sceneTransform = stroke->sceneTransform();
        strokeScenePath = sceneTransform.map(strokeScenePath);
    }

    // Convert eraser path to Qt directly
    QPainterPath eraserQtPath = DrawingEngineUtils::convertSingleClipperPath(eraserPath);

    // Save original color and remove the original stroke
    QColor originalColor = stroke->color();
    DrawingManager::getInstance().getScene()->removeItem(stroke);
    delete stroke;

    // APPROACH: Use Qt's subtracted function and then find separate components
    QPainterPath resultPath = strokeScenePath.subtracted(eraserQtPath);

    // If nothing remains, we're done
    if (resultPath.isEmpty()) {
        return;
    }

    // Find disconnected components using floodfill-like algorithm
    QList<QPainterPath> separatePaths = findDisconnectedComponents(resultPath);

    // Create list of new items but DON'T add them directly to scene
    QList<StrokeItem*> resultItems;

    // Create a new StrokeItem for each separate component
    for (const QPainterPath& path : separatePaths) {
        if (!path.isEmpty()) {
            StrokeItem* newStroke = new StrokeItem(originalColor, 0);
            newStroke->setPath(path);
            newStroke->setBrush(QBrush(originalColor));
            newStroke->setPen(QPen(originalColor.darker(120), 0.5));
            newStroke->setOutlined(true);
            //DrawingManager::getInstance().getScene()->addItem(newStroke);
            resultItems.append(newStroke);
        }
    }

    // Create an EraseCommand to handle the change
    QList<StrokeItem*> originalItems;
    originalItems.append(stroke);
    EraseCommand* cmd = new EraseCommand(DrawingManager::getInstance().getScene(), originalItems, resultItems);
    DrawingManager::getInstance().pushCommand(cmd);
}
// Add this helper function to find disconnected components in a complex path
QList<QPainterPath> EraserTool::findDisconnectedComponents(const QPainterPath& complexPath) {
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
bool EraserTool::subpathsIntersect(const QPainterPath& path1, const QPainterPath& path2) {
    // First do a fast check with bounding rects
    if (!path1.boundingRect().intersects(path2.boundingRect())) {
        return false;
    }

    // A simple approach is to check if their intersection is non-empty
    QPainterPath intersection = path1.intersected(path2);
    return !intersection.isEmpty();
}