#pragma once
#include "Tester.h"
#include <QTest>

class tst_EraserTool : public QObject
{
    Q_OBJECT

private slots:
    void testInitialization();
    void testMousePressEventCreatesPath();
    void testMouseMoveEventUpdatesPath();
    void testMouseReleaseEventFinalizesPath();

};
