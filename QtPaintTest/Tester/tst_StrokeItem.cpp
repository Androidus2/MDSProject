#include "tst_StrokeItem.h"
#include "StrokeItem.h"

void StrokeItemTest::initTestCase() {
    qDebug("Starting StrokeItem tests...");
}

void StrokeItemTest::testConstruction() {
    QColor testColor(Qt::red);
    qreal testWidth = 5.0;
    StrokeItem item(testColor, testWidth);

    QCOMPARE(item.color(), testColor);
    QCOMPARE(item.width(), testWidth);
    QVERIFY(!item.isOutlined());
}

void StrokeItemTest::testSetOutlined() {
    StrokeItem item(QColor(Qt::blue), 2.0);
    item.setOutlined(true);
    QVERIFY(item.isOutlined());

    item.setOutlined(false);
    QVERIFY(!item.isOutlined());
}

void StrokeItemTest::testSelection() {
    StrokeItem item(QColor(Qt::green), 3.0);
    QVERIFY(!item.isSelected());

    item.setSelected(true);
    QVERIFY(item.isSelected());

    item.setSelected(false);
    QVERIFY(!item.isSelected());
}

void StrokeItemTest::testClone() {
    StrokeItem original(QColor(Qt::black), 4.0);
    original.setOutlined(true);
    original.setSelected(true);

    StrokeItem* cloned = original.clone();
    QVERIFY(cloned != nullptr);
    QCOMPARE(cloned->color(), original.color());
    QCOMPARE(cloned->width(), original.width());
    QCOMPARE(cloned->isOutlined(), original.isOutlined());
    QCOMPARE(cloned->isSelected(), original.isSelected());

    delete cloned;
}
