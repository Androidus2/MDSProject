#include "BrushTool.h"
#include "DrawingScene.h"
#include "DrawingManager.h"
#include "AddCommand.h"

BrushTool::BrushTool()
	: m_cooldownInterval(100), m_tangentStrength(0.33) {
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
}

void BrushTool::mousePressEvent(QGraphicsSceneMouseEvent* event) {
	if (event->button() != Qt::LeftButton) return;
	// Start the brush stroke
	startBrushStroke(event->scenePos());
	event->accept();
}
void BrushTool::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
	// Update the brush stroke
	updateBrushStroke(event->scenePos());
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

void BrushTool::commitBrushSegment() {
	// Commit the current brush segment
	commitSegment(m_currentPath, m_tempPathItem, m_realPath);
}

void BrushTool::commitSegment(StrokeItem* pathItem, QGraphicsPathItem* tempItem, QPainterPath& realPath) {
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
QVector2D BrushTool::calculateTangent(int startIndex, int count) {
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
void BrushTool::optimizePath(QPainterPath& path, StrokeItem* pathItem) {
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
void BrushTool::updateTemporaryPath(QGraphicsPathItem* tempItem) {
	if (!tempItem) return;

	QPainterPath tempPath;
	if (m_points.isEmpty()) return;

	tempPath.moveTo(m_points.first());
	for (int i = 1; i < m_points.size(); ++i) {
		tempPath.lineTo(m_points[i]);
	}

	tempItem->setPath(tempPath);
}

void BrushTool::startBrushStroke(const QPointF& pos) {
	// Create the real path item
	m_currentPath = new StrokeItem(DrawingManager::getInstance().getColor(), DrawingManager::getInstance().getWidth());
	DrawingManager::getInstance().getScene() -> addItem(m_currentPath);

	// Create the temporary path item for visual feedback
	m_tempPathItem = new QGraphicsPathItem();
	QPen tempPen(DrawingManager::getInstance().getColor(), DrawingManager::getInstance().getWidth());
	//tempPen.setStyle(Qt::DotLine);
	m_tempPathItem->setPen(tempPen);
	DrawingManager::getInstance().getScene() -> addItem(m_tempPathItem);

	// Reset point collection and paths
	m_points.clear();
	m_points << pos;
	m_realPath = QPainterPath();
	m_realPath.moveTo(pos);
	m_currentPath->setPath(m_realPath);

	// Start the cooldown timer
	m_cooldownTimer.start();
}
void BrushTool::updateBrushStroke(const QPointF& pos) {
	if (!m_currentPath) return;

	m_points << pos;
	updateTemporaryPath(m_tempPathItem);
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

	// Perform final path optimization
	optimizePath(m_realPath, m_currentPath);

	// Convert to filled path
	m_currentPath->convertToFilledPath();

	// Remove from scene before creating command
	DrawingManager::getInstance().getScene()->removeItem(m_currentPath);

	// Create a command to add the path to the scene
	AddCommand* cmd = new AddCommand(DrawingManager::getInstance().getScene(), m_currentPath);

	// Clean up
	if (m_tempPathItem) {
		DrawingManager::getInstance().getScene() -> removeItem(m_tempPathItem);
		delete m_tempPathItem;
		m_tempPathItem = nullptr;
	}

	// Store and reset state before pusing command
	StrokeItem* itemToAdd = m_currentPath;
	m_currentPath = nullptr;
	m_points.clear();

	// Push the command to the undo stack
	DrawingManager::getInstance().pushCommand(cmd);
}

