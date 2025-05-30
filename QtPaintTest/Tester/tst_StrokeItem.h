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
    void testColorAndWidth();
    void testOutlined();
    void testCloneIndependence();
    void testOpacity();
    void testDefaultValues();
    void testOpacityLimits();
     
};
