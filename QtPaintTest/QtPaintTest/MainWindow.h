#pragma once
#include <QtWidgets>
#include "DrawingScene.h"

class MainWindow : public QMainWindow {
public:
    MainWindow();
    ~MainWindow();

private:
    void setupUI();
    void setupMenus();
    void setupTools();
    QIcon createColorIcon(const QColor& color);

    void selectColor();
    void setBrushSize(int size);

    DrawingScene m_scene;
    QGraphicsView* m_view;
    QToolButton* m_colorButton;

};