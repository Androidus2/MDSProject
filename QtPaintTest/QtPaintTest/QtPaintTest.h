#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QtPaintTest.h"

class QtPaintTest : public QMainWindow
{
    Q_OBJECT

public:
    QtPaintTest(QWidget *parent = nullptr);
    ~QtPaintTest();

private:
    Ui::QtPaintTestClass ui;
};
