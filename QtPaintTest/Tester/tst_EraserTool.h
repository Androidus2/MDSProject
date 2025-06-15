#pragma once
#include <QtTest>
#include "EraserTool.h"
#include "DrawingScene.h"
#include "DrawingManager.h"
#include "StrokeItem.h"

class tst_EraserTool : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Basic properties
    void testInitialization();

    // Basic functionality
    void testEraserPathCreation();

    // Erasure functionality
    void testCompleteErasure();
    void testPartialErasure();
    void testMultiComponentCreation();
    void testSingleClickErasure();

    // Edge cases
    void testEmptySceneErasure();

private:
    DrawingScene* setupScene();
    StrokeItem* createTestStroke(DrawingScene* scene, const QList<QPointF>& points, Qt::GlobalColor color = Qt::black);
    void simulateEraserStroke(EraserTool& eraser, const QList<QPointF>& points);
    int countStrokeItems(DrawingScene* scene);
};