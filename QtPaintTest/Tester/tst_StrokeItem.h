#pragma once
#include <QtTest>

class StrokeItemTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testConstruction();
    void testSetOutlined();
    void testSelection();
    void testClone();
};
