#pragma once
#include <QtTest>
#include "Layer.h"
#include "BaseItem.h"

// Mock BaseItem for testing
class MockItem : public BaseItem {
public:
    MockItem() {}
    BaseItem* clone() const override { return new MockItem(); }
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override {}
};

class LayerTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testConstructor();
    void testNameGetterSetter();
    void testVisibilityGetterSetter();
    void testLockedGetterSetter();
    void testZValueGetterSetter();
    void testAddRemoveItems();
    void testNameChangedSignal();
    void testVisibilityChangedSignal();
    void testLockStateChangedSignal();
    void testZValueChangedSignal();
};