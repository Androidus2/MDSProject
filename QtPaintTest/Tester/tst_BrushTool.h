#pragma once
#include <QtTest>
#include "BrushTool.h"
#include "DrawingScene.h"
#include "DrawingManager.h"

class tst_BrushTool : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Basic properties
    void testInitialization();

    // Core functionality
    void testPathCreation();
    void testSmoothPoint();
    void testPathSimplification();
    void testSingleClickDot();

    // State management
    void testStateReset();
    void testCooldownTimer();
    void testMinDistanceFiltering();

private:
    DrawingScene* setupScene();
    void simulateStroke(BrushTool& brush, const QList<QPointF>& points);
    QList<QPointF> createStraightLine(int count);
    QList<QPointF> createCurvedPath(int count);
};