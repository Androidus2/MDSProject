#include <QtWidgets>
#include <clipper2/clipper.h>
#include <iostream>

using namespace Clipper2Lib;

constexpr double CLIPPER_SCALING = 1000.0; // Increased precision

// Custom Graphics Items
//-------------------------------------------------------------

class StrokeItem : public QGraphicsPathItem {
public:
    StrokeItem(const QColor& color, qreal width)
        : m_color(color), m_width(width), m_isOutlined(false), m_realPointCount(0)
    {
        QPen pen(m_color, m_width);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        setPen(pen);
        setBrush(Qt::NoBrush);
    }

    // Add tracking for actual point count
    void setRealPointCount(int count) {
        m_realPointCount = count;
    }

    int realPointCount() const {
        return m_realPointCount;
    }

    void setOutlined(bool outlined) {
        m_isOutlined = outlined;
    }

    void convertToFilledPath() {
        if (m_isOutlined) return;

        // Create a stroker to convert the path to an outline
        QPainterPathStroker stroker;
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        stroker.setWidth(m_width);

        // Get the stroked outline path
        QPainterPath outlinePath = stroker.createStroke(path());

        // Convert to Clipper2 format
        Clipper2Lib::Path64 clipperPath = convertPathToClipper(outlinePath);

        // Create a paths collection with our path
        Clipper2Lib::Paths64 subj;
        subj.push_back(clipperPath);

        // Use Clipper2 to simplify via union operation
        Clipper2Lib::Clipper64 clipper;
        clipper.PreserveCollinear(true); // Keep collinear points to preserve shape
        clipper.AddSubject(subj);

        Clipper2Lib::Paths64 solution;
        clipper.Execute(Clipper2Lib::ClipType::Union,
            Clipper2Lib::FillRule::NonZero,
            solution);

        // If we got results, convert the paths back
        if (!solution.empty()) {
            QPainterPath simplifiedPath;

            // Process each resulting path
            for (const auto& resultPath : solution) {
                QPainterPath subPath = convertSingleClipperPath(resultPath);

                // First path is the outline, subsequent paths are holes
                if (simplifiedPath.isEmpty()) {
                    simplifiedPath = subPath;
                }
                else {
                    // Check orientation to determine if it's a hole
                    // Using area calculation to determine if inside or outside
                    if (Clipper2Lib::Area(resultPath) < 0) {
                        simplifiedPath = simplifiedPath.subtracted(subPath);
                    }
                    else {
                        simplifiedPath = simplifiedPath.united(subPath);
                    }
                }
            }

            setPath(simplifiedPath);
        }

        // Update appearance - fill with color, thin outline
        setBrush(QBrush(m_color));
        setPen(QPen(m_color.darker(120), 0.5)); // Thin outline for definition

        m_isOutlined = true;
    }

    // Helper functions for Clipper2 path conversion
    Clipper2Lib::PathsD convertPathToClipperD(const QPainterPath& path) {
        Clipper2Lib::PathsD result;

        // QPainterPath is a complex structure, we need to extract each subpath
        int startIndex = 0;

        for (int i = 0; i < path.elementCount(); ++i) {
            const QPainterPath::Element& el = path.elementAt(i);

            if (el.isMoveTo() && i > 0) {
                // Extract the previous subpath
                Clipper2Lib::PathD subPath;
                for (int j = startIndex; j < i; ++j) {
                    const QPainterPath::Element& point = path.elementAt(j);
                    subPath.push_back(Clipper2Lib::PointD(point.x, point.y));
                }

                if (subPath.size() >= 3) {
                    result.push_back(subPath);
                }

                startIndex = i;
            }
        }

        // Add the last subpath
        if (startIndex < path.elementCount()) {
            Clipper2Lib::PathD subPath;
            for (int j = startIndex; j < path.elementCount(); ++j) {
                const QPainterPath::Element& point = path.elementAt(j);
                subPath.push_back(Clipper2Lib::PointD(point.x, point.y));
            }

            if (subPath.size() >= 3) {
                result.push_back(subPath);
            }
        }

        return result;
    }

    QPainterPath convertClipperPathsToQPainterPath(const Clipper2Lib::PathsD& paths) {
        QPainterPath result;

        // Process each path from Clipper2
        for (const auto& path : paths) {
            if (path.size() < 3) continue;

            QPainterPath subPath;

            // Start path
            subPath.moveTo(path[0].x, path[0].y);

            // Add remaining points
            for (size_t i = 1; i < path.size(); ++i) {
                subPath.lineTo(path[i].x, path[i].y);
            }

            subPath.closeSubpath();

            // Determine if this is an outer or inner path (hole)
            if (Clipper2Lib::Area(path) > 0) {
                // Positive area means outer contour in Clipper2
                result.addPath(subPath);
            }
            else {
                // Negative area means inner contour (hole)
                result = result.subtracted(subPath);
            }
        }

        return result;
    }

    Clipper2Lib::Path64 convertPathToClipper(const QPainterPath& path) {
        Clipper2Lib::Path64 result;
        for (int i = 0; i < path.elementCount(); ++i) {
            const QPainterPath::Element& el = path.elementAt(i);
            result.emplace_back(
                static_cast<int64_t>(el.x * CLIPPER_SCALING),
                static_cast<int64_t>(el.y * CLIPPER_SCALING)
            );
        }
        return result;
    }

    QPainterPath convertSingleClipperPath(const Clipper2Lib::Path64& path) {
        QPainterPath result;
        if (path.empty()) return result;

        result.moveTo(
            static_cast<qreal>(path[0].x) / CLIPPER_SCALING,
            static_cast<qreal>(path[0].y) / CLIPPER_SCALING
        );

        for (size_t i = 1; i < path.size(); ++i) {
            result.lineTo(
                static_cast<qreal>(path[i].x) / CLIPPER_SCALING,
                static_cast<qreal>(path[i].y) / CLIPPER_SCALING
            );
        }

        if (path.size() > 2) {
            result.closeSubpath();
        }

        return result;
    }

    QColor color() const { return m_color; }
    qreal width() const { return m_width; }
    bool isOutlined() const { return m_isOutlined; }

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override {
        // Draw the regular path first
        QGraphicsPathItem::paint(painter, option, widget);

        // Then draw points - only show real control points, not all path elements
        painter->save();
        painter->setPen(QPen(Qt::magenta, 1));
        painter->setBrush(Qt::magenta);

        const QPainterPath& p = path();
        for (int i = 0; i < p.elementCount(); i += 3) {
            // Only draw actual points (start/end of cubic segments)
            const QPainterPath::Element& el = p.elementAt(i);
            if (el.type == QPainterPath::MoveToElement ||
                (i > 0 && p.elementAt(i - 1).type == QPainterPath::CurveToDataElement)) {
                QRectF pointRect(el.x - 0.5, el.y - 0.5, 1, 1);
                painter->drawRect(pointRect);
            }
        }

        painter->restore();
    }

private:
    QColor m_color;
    qreal m_width;
    bool m_isOutlined;
    int m_realPointCount;
};

// Replace the existing FillItem class with this version:
class FillItem : public StrokeItem {
public:
    FillItem(const QColor& color) : StrokeItem(color, 0) {
        // Set as already outlined so it behaves like a filled shape
        setOutlined(true);
        setBrush(QBrush(color));
        setPen(QPen(color.darker(120), 0.5)); // Thin outline for better visibility
    }

    // Override paint to avoid drawing points
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override {
        // Only call QGraphicsPathItem's paint method, not StrokeItem's
        QGraphicsPathItem::paint(painter, option, widget);
    }
};

// Core Drawing Tools
//-------------------------------------------------------------

enum ToolType { Brush, Eraser, Fill };

class DrawingScene : public QGraphicsScene {
    Q_OBJECT
public:
    DrawingScene(QObject* parent = nullptr)
        : QGraphicsScene(parent), m_currentTool(Brush),
        m_brushColor(Qt::black), m_brushWidth(15),
        m_cooldownInterval(100), m_tangentStrength(0.33) {

        m_cooldownTimer.setInterval(m_cooldownInterval);
        connect(&m_cooldownTimer, &QTimer::timeout, this, &DrawingScene::commitBrushSegment);
    }

    void setTool(ToolType tool) { m_currentTool = tool; }

    int countTotalPoints() const {
        int totalPoints = 0;

        for (QGraphicsItem* item : items()) {
            if (auto* stroke = dynamic_cast<StrokeItem*>(item)) {
                // Count real points rather than all path elements
                totalPoints += stroke->realPointCount();
            }
        }

        return totalPoints;
    }

signals:
    void pointCountChanged();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) return;

        switch (m_currentTool) {
        case Brush: startBrushStroke(event->scenePos()); break;
        case Eraser: startEraserStroke(event->scenePos()); break;
        case Fill: applyFill(event->scenePos()); break;
        }
        QGraphicsScene::mousePressEvent(event);
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        switch (m_currentTool) {
        case Brush: updateBrushStroke(event->scenePos()); break;
        case Eraser: updateEraserStroke(event->scenePos()); break;
        }
        QGraphicsScene::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override {
        switch (m_currentTool) {
        case Brush: finalizeBrushStroke(); break;
        case Eraser: finalizeEraserStroke(); break;
        }
        QGraphicsScene::mouseReleaseEvent(event);
    }

private slots:
    void commitBrushSegment() {
        // Handle both brush and eraser with separate paths
        if (m_currentTool == Brush) {
            commitSegment(m_currentPath, m_tempPathItem, m_realPath);
        }
        else if (m_currentTool == Eraser) {
            commitSegment(m_currentEraserPath, m_tempEraserPathItem, m_eraserRealPath);
        }
    }

private:
    void commitSegment(StrokeItem* pathItem, QGraphicsPathItem* tempItem, QPainterPath& realPath) {
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

        // Increase real point count
        m_realPointCount++;
        pathItem->setRealPointCount(m_realPointCount);

        // Keep only the last point for the next segment
        m_points = { end };

        // Update the temporary path
        updateTemporaryPath(tempItem);
    }

    QVector2D calculateTangent(int startIndex, int count) {
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

    void optimizePath(QPainterPath& path, StrokeItem* pathItem) {
        // Skip optimization if there aren't enough points
        if (path.elementCount() < 4) return;

        QPainterPath newPath;
        QPointF lastPoint = path.elementAt(0);
        newPath.moveTo(lastPoint);

        for (int i = 1; i < path.elementCount(); i += 3) {
            if (i + 2 >= path.elementCount()) break;

            QPointF c1 = path.elementAt(i);
            QPointF c2 = path.elementAt(i + 1);
            QPointF end = path.elementAt(i + 2);

            // Merge nearby points to simplify the path
            if (QVector2D(c1 - lastPoint).length() < m_brushWidth * 2 &&
                QVector2D(c2 - end).length() < m_brushWidth * 2) {
                newPath.quadTo((lastPoint + end) / 2, end);
            }
            else {
                newPath.cubicTo(c1, c2, end);
            }
            lastPoint = end;
        }

        path = newPath;
        pathItem->setPath(path);
    }

    void updateTemporaryPath(QGraphicsPathItem* tempItem) {
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
    void startBrushStroke(const QPointF& pos) {
        // Create the real path item
        m_currentPath = new StrokeItem(m_brushColor, m_brushWidth);
        addItem(m_currentPath);

        // Create the temporary path item for visual feedback
        m_tempPathItem = new QGraphicsPathItem();
        QPen tempPen(m_brushColor, m_brushWidth);
        tempPen.setStyle(Qt::DotLine);
        m_tempPathItem->setPen(tempPen);
        addItem(m_tempPathItem);

        // Reset point collection and paths
        m_points.clear();
        m_points << pos;
        m_realPath = QPainterPath();
        m_realPath.moveTo(pos);
        m_currentPath->setPath(m_realPath);

        // Reset real point count
        m_realPointCount = 1;
        m_currentPath->setRealPointCount(m_realPointCount);

        // Start the cooldown timer
        m_cooldownTimer.start();
    }

    void updateBrushStroke(const QPointF& pos) {
        if (!m_currentPath) return;

        m_points << pos;
        updateTemporaryPath(m_tempPathItem);
    }

    void finalizeBrushStroke() {
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
            m_currentPath->setRealPointCount(1);
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

        emit pointCountChanged();
    }

    // Eraser Implementation
    void startEraserStroke(const QPointF& pos) {
        // Create a visible eraserPath item
        m_currentEraserPath = new StrokeItem(Qt::red, m_brushWidth);
        m_currentEraserPath->setOpacity(0.5); // Semi-transparent
        addItem(m_currentEraserPath);

        // Create temporary path for visual feedback
        m_tempEraserPathItem = new QGraphicsPathItem();
        QPen tempPen(Qt::red, m_brushWidth);
        tempPen.setStyle(Qt::DotLine);
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

        // Reset real point count
        m_realPointCount = 1;
        m_currentEraserPath->setRealPointCount(m_realPointCount);

        // Start the cooldown timer
        m_cooldownTimer.start();
    }

    void updateEraserStroke(const QPointF& pos) {
        if (!m_currentEraserPath) return;

        m_points << pos;
        updateTemporaryPath(m_tempEraserPathItem);
    }

    void finalizeEraserStroke() {
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
            m_currentEraserPath->setRealPointCount(1);
        }

        // Perform final path optimization
        optimizePath(m_eraserRealPath, m_currentEraserPath);

        // Convert to filled path
        m_currentEraserPath->convertToFilledPath();

        // Get the path and convert to Clipper format
        Clipper2Lib::Path64 eraserClipperPath = convertPathToClipper(m_currentEraserPath->path());
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

        emit pointCountChanged();
    }

    void performCleanErase(StrokeItem* stroke, const QPainterPath& eraserPath) {
        // Create a subtracted path using Qt's built-in boolean operations
        QPainterPath remainingPath = stroke->path().subtracted(eraserPath);

        // If nothing remains, remove the stroke
        if (remainingPath.isEmpty()) {
            removeItem(stroke);
            delete stroke;
            return;
        }

        // Get original properties
        QColor originalColor = stroke->color();
        qreal originalWidth = stroke->width();

        // Create new stroke with the remaining path
        StrokeItem* newStroke = new StrokeItem(originalColor, originalWidth);
        newStroke->setPath(remainingPath);
        newStroke->convertToFilledPath();
        addItem(newStroke);

        // Remove the original
        removeItem(stroke);
        delete stroke;

        emit pointCountChanged();
    }

    void processEraserOnStroke(StrokeItem* stroke, const Clipper2Lib::Path64& eraserPath) {
        // Convert the stroke path to Clipper format
        Clipper2Lib::Path64 strokePath = convertPathToClipper(stroke->path());

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
            QPainterPath qtPath = convertSingleClipperPath(path);

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

        emit pointCountChanged();
    }

    // Fill Implementation
    // Replace the current applyFill method with this simpler implementation:
    void applyFill(const QPointF& pos) {
        qDebug() << "Starting fill at" << pos;

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
            qDebug() << "Click position out of bounds";
            return;
        }

        // Get target color
        QRgb targetColor = image.pixel(x, y);
        qDebug() << "Target color:" << QColor(targetColor);

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

        qDebug() << "Found" << fillPoints.size() << "points to fill";

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

        qDebug() << "Created" << spans.size() << "spans from" << fillPoints.size() << "points";

        // Convert spans to Clipper2 paths
        Clipper2Lib::Paths64 paths;
        const double padding = 0.1; // Small padding to ensure connectivity

        // For very large fills, limit the number of spans we process
        const int maxSpans = 5000;
        if (spans.size() > maxSpans) {
            qDebug() << "Too many spans, limiting to" << maxSpans;
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

                QPainterPath subPath = convertSingleClipperPath(path);

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
            FillItem* fill = new FillItem(Qt::yellow);
            fill->setPath(fillPath);
            addItem(fill);

            qDebug() << "Fill complete";
        }

        emit pointCountChanged();
    }


    // Coordinate Conversion Helpers
    Clipper2Lib::Path64 convertPathToClipper(const QPainterPath& path) {
        Clipper2Lib::Path64 result;
        for (int i = 0; i < path.elementCount(); ++i) {
            const QPainterPath::Element& el = path.elementAt(i);
            result.emplace_back(
                static_cast<int64_t>(el.x * CLIPPER_SCALING),
                static_cast<int64_t>(el.y * CLIPPER_SCALING)
            );
        }
        return result;
    }

    QPainterPath convertSingleClipperPath(const Clipper2Lib::Path64& path) {
        QPainterPath result;
        if (path.empty()) return result;

        result.moveTo(
            static_cast<qreal>(path[0].x) / CLIPPER_SCALING,
            static_cast<qreal>(path[0].y) / CLIPPER_SCALING
        );

        for (size_t i = 1; i < path.size(); ++i) {
            result.lineTo(
                static_cast<qreal>(path[i].x) / CLIPPER_SCALING,
                static_cast<qreal>(path[i].y) / CLIPPER_SCALING
            );
        }

        if (path.size() > 2) {
            result.closeSubpath();
        }

        return result;
    }

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
    int m_realPointCount = 0;

    // Curve optimization parameters
    QTimer m_cooldownTimer;
    int m_cooldownInterval;
    float m_tangentStrength;
};

// Main Window
//-------------------------------------------------------------

class MainWindow : public QMainWindow {
public:
    MainWindow() {
        setupUI();
        setupTools();
        setupPointCounter();
    }

private:
    void setupUI() {
        m_view = new QGraphicsView(&m_scene);
        setCentralWidget(m_view);

        QToolBar* toolbar = new QToolBar;
        addToolBar(Qt::LeftToolBarArea, toolbar);

        toolbar->addAction("Brush", [this] { m_scene.setTool(Brush); });
        toolbar->addAction("Eraser", [this] { m_scene.setTool(Eraser); });
        toolbar->addAction("Fill", [this] { m_scene.setTool(Fill); });

        statusBar();
    }

    void setupTools() {
        m_scene.setSceneRect(-500, -500, 1000, 1000);
        m_scene.setBackgroundBrush(Qt::white);
    }

    void setupPointCounter() {
        // Create a label for point count in the status bar
        m_pointCountLabel = new QLabel("Points: 0");
        statusBar()->addPermanentWidget(m_pointCountLabel);

        // Connect to scene's pointCountChanged signal
        connect(&m_scene, &DrawingScene::pointCountChanged,
            this, &MainWindow::updatePointCounter);

        // Initial update
        updatePointCounter();
    }

    void updatePointCounter() {
        int count = m_scene.countTotalPoints();
        m_pointCountLabel->setText(QString("Points: %1").arg(count));
    }

    DrawingScene m_scene;
    QGraphicsView* m_view;
    QLabel* m_pointCountLabel;
};

// Application Entry
//-------------------------------------------------------------

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow win;
    win.show();
    return app.exec();
}

#include "main.moc"