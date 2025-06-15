#include "tst_Layer.h"

void LayerTest::initTestCase() {
    qDebug("Starting Layer tests...");
}

void LayerTest::testConstructor() {
    Layer layer;
    QCOMPARE(layer.getName(), QString("New Layer"));
    QVERIFY(layer.isVisible());
    QVERIFY(!layer.isLocked());
    QCOMPARE(layer.getZValue(), 0);
    QVERIFY(layer.getItems().isEmpty());

    Layer namedLayer("Test Layer");
    QCOMPARE(namedLayer.getName(), QString("Test Layer"));
}

void LayerTest::testNameGetterSetter() {
    Layer layer;
    QString newName = "Modified Layer";
    layer.setName(newName);
    QCOMPARE(layer.getName(), newName);
}

void LayerTest::testVisibilityGetterSetter() {
    Layer layer;
    QVERIFY(layer.isVisible()); // Default should be visible

    layer.setVisible(false);
    QVERIFY(!layer.isVisible());

    layer.setVisible(true);
    QVERIFY(layer.isVisible());
}

void LayerTest::testLockedGetterSetter() {
    Layer layer;
    QVERIFY(!layer.isLocked()); // Default should be unlocked

    layer.setLocked(true);
    QVERIFY(layer.isLocked());

    layer.setLocked(false);
    QVERIFY(!layer.isLocked());
}

void LayerTest::testZValueGetterSetter() {
    Layer layer;
    QCOMPARE(layer.getZValue(), 0); // Default should be 0

    layer.setZValue(5);
    QCOMPARE(layer.getZValue(), 5);

    layer.setZValue(-3);
    QCOMPARE(layer.getZValue(), -3);
}

void LayerTest::testAddRemoveItems() {
    Layer layer;
    QVERIFY(layer.getItems().isEmpty());

    MockItem* item1 = new MockItem();
    MockItem* item2 = new MockItem();

    layer.addItem(item1);
    QCOMPARE(layer.getItems().size(), 1);
    QVERIFY(layer.getItems().contains(item1));

    layer.addItem(item2);
    QCOMPARE(layer.getItems().size(), 2);
    QVERIFY(layer.getItems().contains(item2));

    layer.removeItem(item1);
    QCOMPARE(layer.getItems().size(), 1);
    QVERIFY(!layer.getItems().contains(item1));
    QVERIFY(layer.getItems().contains(item2));

    layer.removeItem(item2);
    QVERIFY(layer.getItems().isEmpty());

    // Clean up
    delete item1;
    delete item2;
}

void LayerTest::testNameChangedSignal() {
    Layer layer("Initial");
    QSignalSpy spy(&layer, SIGNAL(nameChanged(const QString&)));

    layer.setName("New Name");
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QString("New Name"));
}

void LayerTest::testVisibilityChangedSignal() {
    Layer layer;
    QSignalSpy spy(&layer, SIGNAL(visibilityChanged(bool)));

    layer.setVisible(false);
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toBool(), false);

    layer.setVisible(true);
    QCOMPARE(spy.count(), 1);
    arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toBool(), true);
}

void LayerTest::testLockStateChangedSignal() {
    Layer layer;
    QSignalSpy spy(&layer, SIGNAL(lockStateChanged(bool)));

    layer.setLocked(true);
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toBool(), true);

    layer.setLocked(false);
    QCOMPARE(spy.count(), 1);
    arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toBool(), false);
}

void LayerTest::testZValueChangedSignal() {
    Layer layer;
    QSignalSpy spy(&layer, SIGNAL(zValueChanged(int)));

    layer.setZValue(10);
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 10);
}