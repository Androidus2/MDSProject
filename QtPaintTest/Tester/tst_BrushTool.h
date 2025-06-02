#pragma once
#include <QtTest>
#include "BrushTool.h"

class tst_BrushTool : public QObject
{
    Q_OBJECT

private slots:
    void testInitialization();
    void testStartBrushStroke();
    void testSmoothPoint();
    void testMousePressEventCreatesPath();
    void testFinalizeBrushStrokeResetsState();
};

