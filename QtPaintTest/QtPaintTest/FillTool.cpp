#include "FillTool.h"
#include "DrawingScene.h"
#include "DrawingManager.h"
#include "AddCommand.h"

FillTool::FillTool(){
}

FillTool::~FillTool() {
	// Cleanup if needed
}

void FillTool::mousePressEvent(QGraphicsSceneMouseEvent* event) {
	if (event->button() != Qt::LeftButton) return;
	// Apply fill at the clicked position
	applyFill(event->scenePos());
	event->accept();
}
void FillTool::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
	// Handle mouse move if needed
}
void FillTool::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
	// Handle mouse release if needed
}

void FillTool::keyPressEvent(QKeyEvent* event) {
	// Handle key events if needed
}
void FillTool::keyReleaseEvent(QKeyEvent* event) {
	// Handle key events if needed
}

void FillTool::applyFill(const QPointF& pos) {
    // Create a temporary image of the current scene, excluding onion skin items
    QRectF sceneRect = DrawingManager::getInstance().getScene()->sceneRect();
    QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    // Save the current visibility state of onion skin items
    QList<QGraphicsItem*> hiddenItems;
    for (QGraphicsItem* item : DrawingManager::getInstance().getScene() -> items()) {
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
    DrawingManager::getInstance().getScene()-> render(&painter);
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

        // Create a filled shape using the new constructor
        StrokeItem* fill = new StrokeItem(DrawingManager::getInstance().getColor());
        fill->setPath(fillPath);
        //DrawingManager::getInstance().getScene()-> addItem(fill);

        AddCommand* cmd = new AddCommand(DrawingManager::getInstance().getScene(), fill);
        DrawingManager::getInstance().pushCommand(cmd);
    }
}