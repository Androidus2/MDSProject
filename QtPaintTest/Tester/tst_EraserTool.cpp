#include <QtTest>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include "EraserTool.h"
#include "tst_EraserTool.h"
#include "DrawingScene.h"
#include "DrawingManager.h"
#include "StrokeItem.h"

void tst_EraserTool::initTestCase() {
    qDebug("Starting EraserTool tests...");
}

DrawingScene* tst_EraserTool::setupScene() {
    DrawingScene* scene = new DrawingScene();
    DrawingManager::getInstance().setScene(scene);
    DrawingManager::getInstance().setColor(Qt::black);
    DrawingManager::getInstance().setWidth(5.0);
    return scene;
}

StrokeItem* tst_EraserTool::createTestStroke(DrawingScene* scene, const QList<QPointF>& points, Qt::GlobalColor color) {
    if (points.size() < 2) return nullptr;

    QPainterPath path;
    path.moveTo(points.first());
    for (int i = 1; i < points.size(); i++) {
        path.lineTo(points[i]);
    }

    StrokeItem* stroke = new StrokeItem(color, 5.0);
    stroke->setPath(path);
    stroke->convertToFilledPath();
    scene->addItem(stroke);

    return stroke;
}

void tst_EraserTool::simulateEraserStroke(EraserTool& eraser, const QList<QPointF>& points) {
    if (points.isEmpty()) return;

    QGraphicsSceneMouseEvent pressEvent(QEvent::GraphicsSceneMousePress);
    pressEvent.setScenePos(points.first());
    pressEvent.setButton(Qt::LeftButton);
    eraser.mousePressEvent(&pressEvent);

    for (int i = 1; i < points.size(); i++) {
        QGraphicsSceneMouseEvent moveEvent(QEvent::GraphicsSceneMouseMove);
        moveEvent.setScenePos(points[i]);
        eraser.mouseMoveEvent(&moveEvent);
    }

    QGraphicsSceneMouseEvent releaseEvent(QEvent::GraphicsSceneMouseRelease);
    releaseEvent.setScenePos(points.last());
    releaseEvent.setButton(Qt::LeftButton);
    eraser.mouseReleaseEvent(&releaseEvent);
}

int tst_EraserTool::countStrokeItems(DrawingScene* scene) {
    int count = 0;
    for (QGraphicsItem* item : scene->items()) {
        if (dynamic_cast<StrokeItem*>(item)) {
            count++;
        }
    }
    return count;
}

void tst_EraserTool::testInitialization() {
    EraserTool eraser;
    QCOMPARE(eraser.toolName(), QString("Eraser"));
    QVERIFY(!eraser.toolIcon().isNull());
}

void tst_EraserTool::testEraserPathCreation() {
    DrawingScene* scene = setupScene();

    EraserTool eraser;
    QGraphicsSceneMouseEvent pressEvent(QEvent::GraphicsSceneMousePress);
    pressEvent.setScenePos(QPointF(10, 10));
    pressEvent.setButton(Qt::LeftButton);
    eraser.mousePressEvent(&pressEvent);

    QGraphicsSceneMouseEvent moveEvent(QEvent::GraphicsSceneMouseMove);
    moveEvent.setScenePos(QPointF(50, 50));
    eraser.mouseMoveEvent(&moveEvent);

    QGraphicsSceneMouseEvent releaseEvent(QEvent::GraphicsSceneMouseRelease);
    releaseEvent.setScenePos(QPointF(100, 100));
    releaseEvent.setButton(Qt::LeftButton);
    eraser.mouseReleaseEvent(&releaseEvent);

    // Eraser should not leave strokes in the scene
    QCOMPARE(countStrokeItems(scene), 0);

    delete scene;
}

void tst_EraserTool::testCompleteErasure() {
    DrawingScene* scene = setupScene();

    // Create a test stroke
    QList<QPointF> strokePoints = { QPointF(50, 50), QPointF(150, 50) };
    createTestStroke(scene, strokePoints);

    // Create an eraser stroke that completely covers the test stroke
    QList<QPointF> eraserPoints = {
        QPointF(25, 25), QPointF(175, 25),
        QPointF(175, 75), QPointF(25, 75)
    };

    // Count items before erasure
    int beforeCount = countStrokeItems(scene);
    QCOMPARE(beforeCount, 1);

    // Perform the erasure
    EraserTool eraser;
    simulateEraserStroke(eraser, eraserPoints);

    // Count items after erasure - should be 0
    int afterCount = countStrokeItems(scene);
    QCOMPARE(afterCount, 0);

    delete scene;
}

void tst_EraserTool::testPartialErasure() {
    DrawingScene* scene = setupScene();

    // Create a test stroke - horizontal line
    QList<QPointF> strokePoints = { QPointF(50, 100), QPointF(250, 100) };
    createTestStroke(scene, strokePoints);

    // Create an eraser stroke that partially covers the test stroke
    QList<QPointF> eraserPoints = {
        QPointF(125, 75), QPointF(175, 75),
        QPointF(175, 125), QPointF(125, 125)
    };

    // Count items before erasure
    int beforeCount = countStrokeItems(scene);
    QCOMPARE(beforeCount, 1);

    // Perform the erasure
    EraserTool eraser;
    simulateEraserStroke(eraser, eraserPoints);

    // Count items after erasure - should have created two separate pieces
    int afterCount = countStrokeItems(scene);
    QCOMPARE(afterCount, 2);

    delete scene;
}

void tst_EraserTool::testMultiComponentCreation() {
    DrawingScene* scene = setupScene();

    // Create a test stroke - cross shape
    QPainterPath crossPath;
    crossPath.moveTo(100, 50);
    crossPath.lineTo(100, 150);
    crossPath.moveTo(50, 100);
    crossPath.lineTo(150, 100);

    StrokeItem* crossStroke = new StrokeItem(Qt::black, 5.0);
    crossStroke->setPath(crossPath);
    crossStroke->convertToFilledPath();
    scene->addItem(crossStroke);

    // Create an eraser stroke at the center of the cross
    QList<QPointF> eraserPoints = {
        QPointF(75, 75), QPointF(125, 75),
        QPointF(125, 125), QPointF(75, 125)
    };

    // Count items before erasure
    int beforeCount = countStrokeItems(scene);
    QCOMPARE(beforeCount, 1);

    // Perform the erasure
    EraserTool eraser;
    simulateEraserStroke(eraser, eraserPoints);

    // Count items after erasure - should have created multiple separate pieces
    int afterCount = countStrokeItems(scene);
    QVERIFY(afterCount >= 3); // Should create at least 3 parts

    delete scene;
}

void tst_EraserTool::testSingleClickErasure() {
    DrawingScene* scene = setupScene();

    // Create a test stroke
    QList<QPointF> strokePoints = { QPointF(50, 100), QPointF(150, 100) };
    createTestStroke(scene, strokePoints);

    // Single click eraser at center of the stroke
    QGraphicsSceneMouseEvent pressEvent(QEvent::GraphicsSceneMousePress);
    pressEvent.setScenePos(QPointF(100, 100));
    pressEvent.setButton(Qt::LeftButton);

    QGraphicsSceneMouseEvent releaseEvent(QEvent::GraphicsSceneMouseRelease);
    releaseEvent.setScenePos(QPointF(100, 100));
    releaseEvent.setButton(Qt::LeftButton);

    EraserTool eraser;
    eraser.mousePressEvent(&pressEvent);
    eraser.mouseReleaseEvent(&releaseEvent);

    // Should have erased a circle in the middle, possibly creating two parts
    int afterCount = countStrokeItems(scene);
    QVERIFY(afterCount >= 1); // Should create at least one part, possibly two

    delete scene;
}

void tst_EraserTool::testEmptySceneErasure() {
    DrawingScene* scene = setupScene();

    // Create an eraser stroke on empty scene
    QList<QPointF> eraserPoints = {
        QPointF(50, 50), QPointF(150, 50),
        QPointF(150, 150), QPointF(50, 150)
    };

    // Perform the erasure - should not throw exceptions
    EraserTool eraser;
    simulateEraserStroke(eraser, eraserPoints);

    // Scene should still be empty
    int itemCount = countStrokeItems(scene);
    QCOMPARE(itemCount, 0);

    delete scene;
}
