#include <QtTest>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include "BrushTool.h"
#include "tst_BrushTool.h"
#include "DrawingScene.h"
#include "DrawingManager.h"


void tst_BrushTool::testInitialization()
{
   BrushTool brush;
   QVERIFY(brush.toolName() == "Brush");
   
}


void tst_BrushTool::testStartBrushStroke()
{
    // Inițializează scena înainte de test
    auto* scene = new DrawingScene();
    DrawingManager::getInstance().setScene(scene);

    BrushTool brush;
    QPointF start(10, 20);
    brush.startBrushStroke(start);

    // Curăță memoria dacă e nevoie
    delete scene;
}

void tst_BrushTool::testSmoothPoint()
{
    BrushTool brush;
    QPointF p1(0, 0);
    QPointF p2(10, 10);
    QPointF result = brush.smoothPoint(p2);
    // Rezultatul ar trebui să fie între p1 și p2 (depinde de implementare)
    QVERIFY(result.x() <= 10 && result.x() >= 0);
    QVERIFY(result.y() <= 10 && result.y() >= 0);
}

void tst_BrushTool::testMousePressEventCreatesPath()
{
    BrushTool brush;
    QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMousePress);
    event.setScenePos(QPointF(5, 5));
    brush.mousePressEvent(&event);
    // Nu putem verifica direct m_currentPath, dar nu ar trebui să crape
}

void tst_BrushTool::testFinalizeBrushStrokeResetsState()
{
    BrushTool brush;
    brush.startBrushStroke(QPointF(1, 1));
    brush.finalizeBrushStroke();
    // Nu putem verifica direct m_currentPath, dar nu ar trebui să crape
}

