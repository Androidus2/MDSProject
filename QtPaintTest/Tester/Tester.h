#pragma once
#include <QtTest>

class Tester : public QObject {
    Q_OBJECT

private slots:
    void initTestCase_data();
    void initTestCase();
    void init();
    void myTest();
    void cleanup();
    void cleanupTestCase();
};
