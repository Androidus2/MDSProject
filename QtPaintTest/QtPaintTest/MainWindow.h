#pragma once
#include <QtWidgets>
#include "DrawingScene.h"
#include "TimelineWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow();

private slots:
    void onFrameSelected(int frame);
    void onAddFrame();
    void onRemoveFrame();
    void onPlaybackToggled(bool playing);
    void onFrameRateChanged(int fps);
    void advanceFrame();

private:
    void setupUI();
    void setupMenus();
    void setupTools();
    QIcon createColorIcon(const QColor& color);

    void selectColor();
    void setBrushSize(int size);

    QList<DrawingScene*> m_frames;
    int m_currentFrame;
    QGraphicsView* m_view;
    QToolButton* m_colorButton;
    QSpinBox* m_brushSizeSpinBox;
    TimelineWidget* m_timeline;
    QTimer* m_animationTimer;
};