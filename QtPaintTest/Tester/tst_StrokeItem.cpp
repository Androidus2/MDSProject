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

 void StrokeItemTest::testColorAndWidth() { 
        QColor color(Qt::green);
        qreal width = 2.5;
        StrokeItem item(color, width);
        QCOMPARE(item.color(), color);
        QCOMPARE(item.width(), width);
 }

  void StrokeItemTest:: testOutlined() {  
        StrokeItem item(Qt::black, 3.0);
        item.setOutlined(true);
        QVERIFY(item.isOutlined());
        item.setOutlined(false);
        QVERIFY(!item.isOutlined());
    }

  void StrokeItemTest::testCloneIndependence() {
        StrokeItem original(Qt::blue, 5.0);
        original.setOutlined(true);
        StrokeItem* clone = original.clone();
        QVERIFY(clone != nullptr);
        QCOMPARE(clone->color(), original.color());
        QCOMPARE(clone->width(), original.width());
        QVERIFY(clone->isOutlined());
        clone->setOutlined(false);
        QVERIFY(!clone->isOutlined());
        QVERIFY(original.isOutlined());
        delete clone;
    }

        void StrokeItemTest::testOpacity() { 
        StrokeItem item(Qt::red, 4.0);
        item.setOpacity(0.5);
        QCOMPARE(item.opacity(), 0.5);
        item.setOpacity(1.0);
        QCOMPARE(item.opacity(), 1.0);
    }
        void StrokeItemTest::testDefaultValues() {
            StrokeItem item(QColor(Qt::black), 1.0);
            QCOMPARE(item.color(), QColor(Qt::black));
            QCOMPARE(item.width(), 1.0);
            QVERIFY(!item.isOutlined());
            QVERIFY(!item.isSelected());
            QCOMPARE(item.opacity(), 1.0); // presupunând că opacitatea implicită e 1.0
        }

        void StrokeItemTest::testOpacityLimits() {
            StrokeItem item(Qt::red, 2.0);
            item.setOpacity(0.0);
            QCOMPARE(item.opacity(), 0.0);
            item.setOpacity(1.0);
            QCOMPARE(item.opacity(), 1.0);
        }

   