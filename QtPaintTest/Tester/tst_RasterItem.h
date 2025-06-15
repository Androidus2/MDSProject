#pragma once
#include <QtTest>
#include "RasterItem.h"

class RasterItemTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testConstructionWithImage();
    void testConstructionWithPath();
    void testCopyConstruction();
    void testClone();
    void testGetImage();
    void testImageConsistency();
    void testCloneIndependence();
    void testInvalidImagePath();
    void testEmptyImage();
    void testLargeImage();
};