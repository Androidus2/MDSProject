#pragma once
#include <QtWidgets>
#include <QUndoStack>
#include "DrawingScene.h"
#include "ManipulatableGraphicsView.h" // Include the new header

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
    ManipulatableGraphicsView* m_view; // Change type from QGraphicsView*
    QToolButton* m_colorButton;
    QUndoStack* m_undoStack;
    QAction *m_undoAction;
    QAction *m_redoAction;

};