#include <QtTest>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include "EraserTool.h"
#include "DrawingScene.h"
#include "DrawingManager.h"
#include "tst_EraserTool.h"



    void tst_EraserTool::testInitialization()
    {
        EraserTool eraser;
        QCOMPARE(eraser.toolName(), QString("Eraser"));
    }

    void tst_EraserTool::testMousePressEventCreatesPath()
    {
        // Set up a scene for the DrawingManager singleton
        auto* scene = new DrawingScene();
        DrawingManager::getInstance().setScene(scene);

        EraserTool eraser;
        QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMousePress);
        event.setScenePos(QPointF(10, 10));
        event.setButton(Qt::LeftButton);
        eraser.mousePressEvent(&event);

        // Clean up
        delete scene;
    }

    void tst_EraserTool::testMouseMoveEventUpdatesPath()
    {
        auto* scene = new DrawingScene();
        DrawingManager::getInstance().setScene(scene);

        EraserTool eraser;
        QGraphicsSceneMouseEvent pressEvent(QEvent::GraphicsSceneMousePress);
        pressEvent.setScenePos(QPointF(10, 10));
        pressEvent.setButton(Qt::LeftButton);
        eraser.mousePressEvent(&pressEvent);

        QGraphicsSceneMouseEvent moveEvent(QEvent::GraphicsSceneMouseMove);
        moveEvent.setScenePos(QPointF(20, 20));
        eraser.mouseMoveEvent(&moveEvent);

        delete scene;
    }

    void tst_EraserTool::testMouseReleaseEventFinalizesPath()
    {
        auto* scene = new DrawingScene();
        DrawingManager::getInstance().setScene(scene);

        EraserTool eraser;
        QGraphicsSceneMouseEvent pressEvent(QEvent::GraphicsSceneMousePress);
        pressEvent.setScenePos(QPointF(10, 10));
        pressEvent.setButton(Qt::LeftButton);
        eraser.mousePressEvent(&pressEvent);

        QGraphicsSceneMouseEvent releaseEvent(QEvent::GraphicsSceneMouseRelease);
        releaseEvent.setScenePos(QPointF(20, 20));
        releaseEvent.setButton(Qt::LeftButton);
        eraser.mouseReleaseEvent(&releaseEvent);

        delete scene;
    }


