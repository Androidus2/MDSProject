#include <QtTest>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include "BrushTool.h"
#include "tst_BrushTool.h"
#include "DrawingScene.h"
#include "DrawingManager.h"
#include "StrokeItem.h"

void tst_BrushTool::initTestCase() {
    qDebug("Starting BrushTool tests...");
}

DrawingScene* tst_BrushTool::setupScene() {
    DrawingScene* scene = new DrawingScene();
    DrawingManager::getInstance().setScene(scene);
    DrawingManager::getInstance().setColor(Qt::black);
    DrawingManager::getInstance().setWidth(5.0);
    return scene;
}

void tst_BrushTool::simulateStroke(BrushTool& brush, const QList<QPointF>& points) {
    if (points.isEmpty()) return;

    QGraphicsSceneMouseEvent pressEvent(QEvent::GraphicsSceneMousePress);
    pressEvent.setScenePos(points.first());
    pressEvent.setButton(Qt::LeftButton);
    brush.mousePressEvent(&pressEvent);

    for (int i = 1; i < points.size(); i++) {
        QGraphicsSceneMouseEvent moveEvent(QEvent::GraphicsSceneMouseMove);
        moveEvent.setScenePos(points[i]);
        brush.mouseMoveEvent(&moveEvent);
    }

    QGraphicsSceneMouseEvent releaseEvent(QEvent::GraphicsSceneMouseRelease);
    releaseEvent.setScenePos(points.last());
    releaseEvent.setButton(Qt::LeftButton);
    brush.mouseReleaseEvent(&releaseEvent);
}

QList<QPointF> tst_BrushTool::createStraightLine(int count) {
    QList<QPointF> points;
    for (int i = 0; i < count; i++) {
        points << QPointF(i * 10, 100);
    }
    return points;
}

QList<QPointF> tst_BrushTool::createCurvedPath(int count) {
    QList<QPointF> points;
    for (int i = 0; i < count; i++) {
        double angle = i * M_PI / (count / 2);
        points << QPointF(100 + 50 * cos(angle), 100 + 50 * sin(angle));
    }
    return points;
}

void tst_BrushTool::testInitialization() {
    BrushTool brush;
    QCOMPARE(brush.toolName(), QString("Brush"));
    QVERIFY(!brush.toolIcon().isNull());
}

void tst_BrushTool::testPathCreation() {
    DrawingScene* scene = setupScene();

    BrushTool brush;
    QList<QPointF> points = createStraightLine(5);
    simulateStroke(brush, points);

    // Check that at least one stroke was created
    bool foundStroke = false;
    for (QGraphicsItem* item : scene->items()) {
        if (dynamic_cast<StrokeItem*>(item)) {
            foundStroke = true;
            break;
        }
    }
    QVERIFY(foundStroke);

    delete scene;
}

void tst_BrushTool::testSmoothPoint() {
    BrushTool brush;
    QPointF p1(0, 0);
    QPointF p2(10, 10);
    QPointF result = brush.smoothPoint(p2);

    // Smoothed point should be between original points
    QVERIFY(result.x() <= 10 && result.x() >= 0);
    QVERIFY(result.y() <= 10 && result.y() >= 0);
}

void tst_BrushTool::testPathSimplification() {
    DrawingScene* scene = setupScene();
    BrushTool brush;

    // Create a path with many points in a straight line
    QList<QPointF> straightLine = createStraightLine(20);
    simulateStroke(brush, straightLine);

    // Create a path with many points in a curve
    QList<QPointF> curvedPath = createCurvedPath(20);
    simulateStroke(brush, curvedPath);

    // Verify paths were created
    int strokeCount = 0;
    for (QGraphicsItem* item : scene->items()) {
        if (dynamic_cast<StrokeItem*>(item)) {
            strokeCount++;
        }
    }
    QVERIFY(strokeCount >= 2);

    delete scene;
}

void tst_BrushTool::testSingleClickDot() {
    DrawingScene* scene = setupScene();
    BrushTool brush;

    // Single click at one point should create a dot
    QPointF clickPoint(100, 100);

    QGraphicsSceneMouseEvent pressEvent(QEvent::GraphicsSceneMousePress);
    pressEvent.setScenePos(clickPoint);
    pressEvent.setButton(Qt::LeftButton);
    brush.mousePressEvent(&pressEvent);

    QGraphicsSceneMouseEvent releaseEvent(QEvent::GraphicsSceneMouseRelease);
    releaseEvent.setScenePos(clickPoint);
    releaseEvent.setButton(Qt::LeftButton);
    brush.mouseReleaseEvent(&releaseEvent);

    // Check if a stroke was created
    bool foundStroke = false;
    for (QGraphicsItem* item : scene->items()) {
        if (StrokeItem* stroke = dynamic_cast<StrokeItem*>(item)) {
            foundStroke = true;

            // Verify the path is circular (similar width and height)
            QRectF bounds = stroke->path().boundingRect();
            QVERIFY(qAbs(bounds.width() - bounds.height()) < 1.0);
        }
    }
    QVERIFY(foundStroke);

    delete scene;
}

void tst_BrushTool::testStateReset() {
    DrawingScene* scene = setupScene();
    BrushTool brush;

    // Create and finalize a stroke
    QList<QPointF> path = createStraightLine(5);
    simulateStroke(brush, path);

    // Create another stroke - should work without issues
    QList<QPointF> path2 = createCurvedPath(5);
    simulateStroke(brush, path2);

    // Verify two strokes were created
    int strokeCount = 0;
    for (QGraphicsItem* item : scene->items()) {
        if (dynamic_cast<StrokeItem*>(item)) {
            strokeCount++;
        }
    }
    QVERIFY(strokeCount >= 2);

    delete scene;
}

void tst_BrushTool::testCooldownTimer() {
    DrawingScene* scene = setupScene();
    BrushTool brush;

    // Start a stroke and wait for cooldown to trigger
    QPointF start(50, 50);
    brush.startBrushStroke(start);
    brush.updateBrushStroke(QPointF(100, 100));

    // Wait for cooldown timer (longer than the interval)
    QTest::qWait(100);

    // Finalize the stroke
    brush.finalizeBrushStroke();

    // Verify a stroke was created
    bool foundStroke = false;
    for (QGraphicsItem* item : scene->items()) {
        if (dynamic_cast<StrokeItem*>(item)) {
            foundStroke = true;
            break;
        }
    }
    QVERIFY(foundStroke);

    delete scene;
}

void tst_BrushTool::testMinDistanceFiltering() {
    DrawingScene* scene = setupScene();
    BrushTool brush;

    // Start a stroke
    QPointF start(50, 50);
    brush.startBrushStroke(start);

    // Add several very close points that should be filtered
    for (int i = 0; i < 10; i++) {
        brush.updateBrushStroke(QPointF(50 + 0.1 * i, 50 + 0.1 * i));
    }

    // Add a point that's far enough to be included
    brush.updateBrushStroke(QPointF(60, 60));
    brush.finalizeBrushStroke();

    // Verify a stroke was created
    bool foundStroke = false;
    for (QGraphicsItem* item : scene->items()) {
        if (dynamic_cast<StrokeItem*>(item)) {
            foundStroke = true;
            break;
        }
    }
    QVERIFY(foundStroke);

    delete scene;
}