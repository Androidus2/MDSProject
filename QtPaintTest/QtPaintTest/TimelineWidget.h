#pragma once
#include <QtWidgets>
#include "FrameItem.h"

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    TimelineWidget(QWidget* parent = nullptr);
    void setFrames(int count, int currentFrame);
    int getFrameRate() const { return m_framerateSpinBox->value(); }
    bool isPlaying() const { return m_isPlaying; }

public slots:
    void togglePlayback();

signals:
    void frameSelected(int frame);
    void addFrameRequested();
    void removeFrameRequested();
    void playbackToggled(bool playing);
    void frameRateChanged(int fps);

private:
    void updateFramesDisplay();
    void updatePlayButton();

    QGraphicsScene* m_scene;
    QGraphicsView* m_view;
    int m_frameCount;
    int m_currentFrame;
    QSpinBox* m_framerateSpinBox;
    QPushButton* m_playButton;
    bool m_isPlaying;
};