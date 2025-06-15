#include "tst_ClipboardItem.h"

void ClipboardItemTest::initTestCase() {
    qDebug("Starting ClipboardItem tests...");
}

void ClipboardItemTest::testStrokeItemConstruction() {
    QPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(100, 100);

    QColor color(Qt::red);
    qreal width = 2.5;
    bool outlined = true;

    ClipboardItem item(path, color, width, outlined);

    QCOMPARE(item.type, ClipboardItemType::Stroke);
}

void ClipboardItemTest::testRasterItemConstruction() {
    QImage image(100, 100, QImage::Format_ARGB32);
    image.fill(Qt::blue);

    ClipboardItem item(image);

    QCOMPARE(item.type, ClipboardItemType::Raster);
}

void ClipboardItemTest::testStrokeProperties() {
    QPainterPath path;
    path.moveTo(10, 10);
    path.lineTo(50, 50);

    QColor color(Qt::green);
    qreal width = 3.0;
    bool outlined = false;

    ClipboardItem item(path, color, width, outlined);

    QCOMPARE(item.color, color);
    QCOMPARE(item.width, width);
    QCOMPARE(item.outlined, outlined);
}

void ClipboardItemTest::testRasterProperties() {
    QImage image(200, 200, QImage::Format_ARGB32);
    image.fill(Qt::yellow);

    ClipboardItem item(image);

    QCOMPARE(item.image.width(), image.width());
    QCOMPARE(item.image.height(), image.height());
    QCOMPARE(item.image.pixel(100, 100), image.pixel(100, 100));
}

void ClipboardItemTest::testPathCopy() {
    QPainterPath path;
    path.addEllipse(QRectF(10, 10, 80, 80));

    ClipboardItem item(path, Qt::black, 1.0, true);

    QCOMPARE(item.path.elementCount(), path.elementCount());

    QPainterPath::Element e1 = item.path.elementAt(0);
    QPainterPath::Element e2 = path.elementAt(0);

    QCOMPARE(e1.x, e2.x);
    QCOMPARE(e1.y, e2.y);
}

void ClipboardItemTest::testColorProperties() {
    QColor color(45, 90, 135, 180);
    ClipboardItem item(QPainterPath(), color, 1.0, false);

    QCOMPARE(item.color.red(), color.red());
    QCOMPARE(item.color.green(), color.green());
    QCOMPARE(item.color.blue(), color.blue());
    QCOMPARE(item.color.alpha(), color.alpha());
}

void ClipboardItemTest::testWidthProperty() {
    qreal width = 4.75;
    ClipboardItem item(QPainterPath(), Qt::black, width, false);

    QCOMPARE(item.width, width);
}

void ClipboardItemTest::testOutlinedProperty() {
    bool outlined = true;
    ClipboardItem item(QPainterPath(), Qt::black, 1.0, outlined);

    QCOMPARE(item.outlined, outlined);
}