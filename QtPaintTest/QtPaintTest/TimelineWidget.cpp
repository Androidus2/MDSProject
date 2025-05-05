#include "TimelineWidget.h"

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent), m_frameCount(0), m_currentFrame(0), m_isPlaying(false) {
    QVBoxLayout* layout = new QVBoxLayout(this);

    // Toolbar
    QToolBar* toolbar = new QToolBar;

    // Add/Remove frame buttons
    QAction* addAction = toolbar->addAction("+");
    QAction* removeAction = toolbar->addAction("-");

    // Playback controls
    toolbar->addSeparator();
    m_playButton = new QPushButton;
    updatePlayButton();
    connect(m_playButton, &QPushButton::clicked, this, &TimelineWidget::togglePlayback);
    toolbar->addWidget(m_playButton);

    // Framerate control
    toolbar->addSeparator();
    toolbar->addWidget(new QLabel("FPS:"));
    m_framerateSpinBox = new QSpinBox;
    m_framerateSpinBox->setRange(1, 60);
    m_framerateSpinBox->setValue(12); // Default 12 FPS
    toolbar->addWidget(m_framerateSpinBox);

    connect(m_framerateSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &TimelineWidget::frameRateChanged);
    connect(addAction, &QAction::triggered, this, &TimelineWidget::addFrameRequested);
    connect(removeAction, &QAction::triggered, this, &TimelineWidget::removeFrameRequested);

    layout->addWidget(toolbar);

    // Graphics view
    m_view = new QGraphicsView;
    m_scene = new QGraphicsScene;
    m_view->setScene(m_scene);
    m_view->setFixedHeight(80);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    layout->addWidget(m_view);
}

void TimelineWidget::togglePlayback() {
    m_isPlaying = !m_isPlaying;
    updatePlayButton();
    emit playbackToggled(m_isPlaying);
}

void TimelineWidget::updatePlayButton() {
    if (m_isPlaying) {
        m_playButton->setText("⏸"); // Pause symbol
        m_playButton->setToolTip("Pause");
    }
    else {
        m_playButton->setText("▶"); // Play symbol
        m_playButton->setToolTip("Play");
    }
}

void TimelineWidget::setFrames(int count, int currentFrame) {
    m_frameCount = count;
    m_currentFrame = currentFrame;
    updateFramesDisplay();
}

void TimelineWidget::updateFramesDisplay() {
    m_scene->clear();

    const int frameWidth = 50;
    const int frameHeight = 30;
    const int spacing = 2;
    int xPos = 0;

    for (int i = 0; i < m_frameCount; ++i) {
        FrameItem* frame = new FrameItem(i, xPos, 10, frameWidth, frameHeight);
        frame->setBrush(i == m_currentFrame ? QColor(100, 150, 255) : Qt::lightGray);
        frame->setPen(QPen(Qt::black));

        connect(frame, &FrameItem::frameClicked, this, &TimelineWidget::frameSelected);

        m_scene->addItem(frame);
        xPos += frameWidth + spacing;
    }

    m_scene->setSceneRect(0, 0, xPos, 50);
}