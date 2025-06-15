#include "tst_RasterItem.h"
#include <QTemporaryFile>

void RasterItemTest::initTestCase() {
    qDebug("Starting RasterItem tests...");
}

void RasterItemTest::testConstructionWithImage() {
    QImage testImage(100, 100, QImage::Format_ARGB32);
    testImage.fill(Qt::red);

    RasterItem item(testImage);

    QCOMPARE(item.getImage().width(), testImage.width());
    QCOMPARE(item.getImage().height(), testImage.height());
    QCOMPARE(item.getImage().format(), testImage.format());
    QCOMPARE(item.getImage().pixel(50, 50), testImage.pixel(50, 50));
}

void RasterItemTest::testConstructionWithPath() {
    // Create a temporary image file
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(true);

    if (tempFile.open()) {
        QImage testImage(50, 50, QImage::Format_ARGB32);
        testImage.fill(Qt::blue);
        testImage.save(&tempFile, "PNG");
        QString imagePath = tempFile.fileName();
        tempFile.close();

        RasterItem item(imagePath);

        QCOMPARE(item.getImage().width(), testImage.width());
        QCOMPARE(item.getImage().height(), testImage.height());
        QVERIFY(!item.getImage().isNull());
    }
    else {
        QFAIL("Failed to create temporary file for testing");
    }
}

void RasterItemTest::testCopyConstruction() {
    QImage testImage(75, 75, QImage::Format_ARGB32);
    testImage.fill(Qt::green);

    RasterItem original(testImage);
    RasterItem copy(original);

    QCOMPARE(copy.getImage().width(), original.getImage().width());
    QCOMPARE(copy.getImage().height(), original.getImage().height());
    QCOMPARE(copy.getImage().format(), original.getImage().format());
    QCOMPARE(copy.getImage().pixel(30, 30), original.getImage().pixel(30, 30));
}

void RasterItemTest::testClone() {
    QImage testImage(60, 60, QImage::Format_ARGB32);
    testImage.fill(Qt::yellow);

    RasterItem original(testImage);
    BaseItem* cloned = original.clone();

    QVERIFY(cloned != nullptr);

    RasterItem* clonedRaster = dynamic_cast<RasterItem*>(cloned);
    QVERIFY(clonedRaster != nullptr);

    QCOMPARE(clonedRaster->getImage().width(), original.getImage().width());
    QCOMPARE(clonedRaster->getImage().height(), original.getImage().height());
    QCOMPARE(clonedRaster->getImage().pixel(20, 20), original.getImage().pixel(20, 20));

    delete cloned;
}

void RasterItemTest::testGetImage() {
    QImage testImage(40, 40, QImage::Format_ARGB32);
    testImage.fill(Qt::magenta);

    RasterItem item(testImage);
    QImage retrievedImage = item.getImage();

    QCOMPARE(retrievedImage.width(), testImage.width());
    QCOMPARE(retrievedImage.height(), testImage.height());
    QCOMPARE(retrievedImage.format(), testImage.format());
    QCOMPARE(retrievedImage.pixel(15, 15), testImage.pixel(15, 15));
}

void RasterItemTest::testImageConsistency() {
    QImage testImage(30, 30, QImage::Format_ARGB32);
    testImage.fill(Qt::cyan);

    // Draw something on the image
    QPainter painter(&testImage);
    painter.setPen(Qt::black);
    painter.drawLine(0, 0, 30, 30);
    painter.end();

    RasterItem item(testImage);

    // Check if specific pixels match
    QCOMPARE(item.getImage().pixel(0, 0), testImage.pixel(0, 0));
    QCOMPARE(item.getImage().pixel(15, 15), testImage.pixel(15, 15));
    QCOMPARE(item.getImage().pixel(29, 29), testImage.pixel(29, 29));
}

void RasterItemTest::testCloneIndependence() {
    QImage testImage(25, 25, QImage::Format_ARGB32);
    testImage.fill(Qt::white);

    RasterItem original(testImage);
    RasterItem* cloned = dynamic_cast<RasterItem*>(original.clone());
    QVERIFY(cloned != nullptr);

    // Modify the original image
    QPainter painter(&testImage);
    painter.fillRect(0, 0, 25, 25, Qt::black);
    painter.end();

    // The cloned image should remain unchanged
    QVERIFY(cloned->getImage().pixel(10, 10) != testImage.pixel(10, 10));

    delete cloned;
}

void RasterItemTest::testInvalidImagePath() {
    // Test with a non-existent file path
    RasterItem item("nonexistent_file.png");

    // The image should be null or empty
    QVERIFY(item.getImage().isNull());
}

void RasterItemTest::testEmptyImage() {
    QImage emptyImage;
    RasterItem item(emptyImage);

    QVERIFY(item.getImage().isNull());
}

void RasterItemTest::testLargeImage() {
    QImage largeImage(1000, 1000, QImage::Format_ARGB32);
    largeImage.fill(Qt::darkGray);

    RasterItem item(largeImage);

    QCOMPARE(item.getImage().width(), largeImage.width());
    QCOMPARE(item.getImage().height(), largeImage.height());
}