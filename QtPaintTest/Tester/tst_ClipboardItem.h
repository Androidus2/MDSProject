#pragma once
#include <QtTest>
#include "ClipboardItem.h"

class ClipboardItemTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testStrokeItemConstruction();
    void testRasterItemConstruction();
    void testStrokeProperties();
    void testRasterProperties();
    void testPathCopy();
    void testColorProperties();
    void testWidthProperty();
    void testOutlinedProperty();
};