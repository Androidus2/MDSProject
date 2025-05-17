#pragma once
#include <QtWidgets>
#include <QUndoStack>
#include <QUndoView>
#include "DrawingScene.h"
#include "TimelineWidget.h"
#include "ManipulatableGraphicsView.h"

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
    void toggleOnionSkin(bool enabled);
    void setOnionSkinOpacity(int opacity);

private:
    void setupUI();
    void setupMenus();
    void setupTools();
    void setupUndoRedo();
    QIcon createColorIcon(const QColor& color);

    void selectColor();
    void setBrushSize(int size);

    QList<DrawingScene*> m_frames;
    int m_currentFrame;
    ManipulatableGraphicsView* m_view;
    QToolButton* m_colorButton;
    QSpinBox* m_brushSizeSpinBox;
    TimelineWidget* m_timeline;
    QTimer* m_animationTimer;

    bool m_onionSkinEnabled = false;
    int m_onionSkinOpacity = 30; // Default 30% opacity
    QList<QGraphicsItemGroup*> m_onionSkinItems; // To track added onion skin items
    QAction* m_onionSkinAction;
    QSlider* m_opacitySlider;
    QCheckBox* m_onionSkinCheckBox;

    // Undo/Redo stack
    QUndoStack* m_undoStack;
    QAction* m_undoAction;
    QAction* m_redoAction;

    void updateOnionSkin();
    void addOnionSkinFrame(int frameIndex, float opacityMultiplier = 1.0f);
};