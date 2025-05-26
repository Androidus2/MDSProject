#include "Tester.h"

void Tester::initTestCase_data() {
    qDebug("Creates a global test data table.");
}

void Tester::initTestCase() {
    qDebug("Called before the first test function is executed.");
}

void Tester::init() {
    qDebug("Called before each test function is executed.");
}

void Tester::myTest() {
    QVERIFY(true);
    QCOMPARE(1, 1);
}

void Tester::cleanup() {
    qDebug("Called after every test function.");
}

void Tester::cleanupTestCase() {
    qDebug("Called after the last test function was executed.");
}
