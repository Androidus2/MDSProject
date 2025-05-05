#include "MainWindow.h"
#include "FileIOOperations.h"

MainWindow::MainWindow() : m_currentFrame(0) {
    // Create initial frames (3 instead of just 1)
    for (int i = 0; i < 3; i++) {
        DrawingScene* scene = new DrawingScene();
        scene->setSceneRect(-500, -500, 1000, 1000);
        scene->setBackgroundBrush(Qt::white);
        m_frames.append(scene);
    }

    setupUI();
    setupTools();
    setupMenus();

    // Set up animation timer
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this, &MainWindow::advanceFrame);

    // Set initial framerate (12 FPS)
    onFrameRateChanged(m_timeline->getFrameRate());
}

MainWindow::~MainWindow() {
    qDeleteAll(m_frames);
}

void MainWindow::setupUI() {
    QWidget* centralWidget = new QWidget;
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    setCentralWidget(centralWidget);

    // Drawing view
    m_view = new QGraphicsView(m_frames[m_currentFrame]);
    mainLayout->addWidget(m_view);

    // Timeline
    m_timeline = new TimelineWidget;
    mainLayout->addWidget(m_timeline);
    m_timeline->setFrames(m_frames.size(), m_currentFrame);

    // Connect timeline signals
    connect(m_timeline, &TimelineWidget::frameSelected, this, &MainWindow::onFrameSelected);
    connect(m_timeline, &TimelineWidget::addFrameRequested, this, &MainWindow::onAddFrame);
    connect(m_timeline, &TimelineWidget::removeFrameRequested, this, &MainWindow::onRemoveFrame);
    connect(m_timeline, &TimelineWidget::playbackToggled, this, &MainWindow::onPlaybackToggled);
    connect(m_timeline, &TimelineWidget::frameRateChanged, this, &MainWindow::onFrameRateChanged);

    // Rest of the UI setup remains the same...
    QToolBar* toolbar = new QToolBar;
    addToolBar(Qt::LeftToolBarArea, toolbar);

    toolbar->addAction("Brush", [this] { m_frames[m_currentFrame]->setTool(Brush); });
    toolbar->addAction("Eraser", [this] { m_frames[m_currentFrame]->setTool(Eraser); });
    toolbar->addAction("Fill", [this] { m_frames[m_currentFrame]->setTool(Fill); });
    toolbar->addAction("Select", [this] { m_frames[m_currentFrame]->setTool(Select); });

    // Add color selection button
    QToolButton* colorButton = new QToolButton;
    colorButton->setText("Color");
    colorButton->setIcon(createColorIcon(m_frames[m_currentFrame]->currentColor()));
    colorButton->setIconSize(QSize(24, 24));
    connect(colorButton, &QToolButton::clicked, this, &MainWindow::selectColor);
    toolbar->addWidget(colorButton);
    m_colorButton = colorButton;

    // Add brush size control
    toolbar->addSeparator();
    toolbar->addWidget(new QLabel("Size:"));
    QSpinBox* sizeSpinBox = new QSpinBox;
    sizeSpinBox->setRange(1, 100);
    sizeSpinBox->setValue(15); // Default brush width
    sizeSpinBox->setSingleStep(1);
    m_brushSizeSpinBox = sizeSpinBox;
    connect(sizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &MainWindow::setBrushSize);
    toolbar->addWidget(sizeSpinBox);

    statusBar();
}
void MainWindow::setupMenus() {
    // Create File menu
    QMenu* fileMenu = menuBar()->addMenu("&File");

    // New action
    QAction* newAction = fileMenu->addAction("&New");
    newAction->setShortcut(QKeySequence::New);
	//connect(newAction, &QAction::triggered, this, &MainWindow::newDrawing);
	connect(newAction, &QAction::triggered, this, [this]() {
		FileIOOperations::newDrawing(*m_frames[m_currentFrame], *this);
		});

    // Open action
    QAction* openAction = fileMenu->addAction("&Open...");
    openAction->setShortcut(QKeySequence::Open);
	connect(openAction, &QAction::triggered, this, [this]() {
		FileIOOperations::loadDrawing(*m_frames[m_currentFrame], *this);
		});

    // Save action
    QAction* saveAction = fileMenu->addAction("&Save");
    saveAction->setShortcut(QKeySequence::Save);
	connect(saveAction, &QAction::triggered, this, [this]() {
		FileIOOperations::saveDrawing(*m_frames[m_currentFrame], *this);
		});

    // Save As action
    QAction* saveAsAction = fileMenu->addAction("Save &As...");
    saveAsAction->setShortcut(QKeySequence::SaveAs);
	connect(saveAsAction, &QAction::triggered, this, [this]() {
		FileIOOperations::saveDrawingAs(*m_frames[m_currentFrame], *this);
		});

    fileMenu->addSeparator();

    // Export submenu
    QMenu* exportMenu = fileMenu->addMenu("&Export");

    QAction* exportSVG = exportMenu->addAction("Export as &SVG...");
	connect(exportSVG, &QAction::triggered, this, [this]() {
		FileIOOperations::exportSVG(*m_frames[m_currentFrame], *this);
		});

    QAction* exportPNG = exportMenu->addAction("Export as &PNG...");
	connect(exportPNG, &QAction::triggered, this, [this]() {
		FileIOOperations::exportPNG(*m_frames[m_currentFrame], *this);
		});

    QAction* exportJPEG = exportMenu->addAction("Export as &JPEG...");
	connect(exportJPEG, &QAction::triggered, this, [this]() {
		FileIOOperations::exportJPEG(*m_frames[m_currentFrame], *this);
		});

    fileMenu->addSeparator();

    // Exit action
    QAction* exitAction = fileMenu->addAction("&Exit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
}
void MainWindow::setupTools() {
    m_frames[m_currentFrame] -> setSceneRect(-500, -500, 1000, 1000);
    m_frames[m_currentFrame] -> setBackgroundBrush(Qt::white);
}

// Create a colored icon for the color button
QIcon MainWindow::createColorIcon(const QColor& color) {
    QPixmap pixmap(24, 24);
    pixmap.fill(color);
    return QIcon(pixmap);
}

void MainWindow::selectColor() {
    QColor color = QColorDialog::getColor(m_frames[m_currentFrame] -> currentColor(), this, "Select Color");
    if (color.isValid()) {
        m_frames[m_currentFrame] -> setColor(color);
        m_colorButton->setIcon(createColorIcon(color));
    }
}
void MainWindow::setBrushSize(int size) {
    m_frames[m_currentFrame] -> setBrushWidth(size);
}

void MainWindow::onFrameSelected(int frame) {
    if (frame >= 0 && frame < m_frames.size()) {
        m_currentFrame = frame;
        m_view->setScene(m_frames[frame]);
        m_colorButton->setIcon(createColorIcon(m_frames[frame]->currentColor()));
        m_brushSizeSpinBox->setValue(m_frames[frame]->brushWidth());
        m_timeline->setFrames(m_frames.size(), m_currentFrame);
    }
}

void MainWindow::onAddFrame() {
    // Create new scene
    DrawingScene* newScene = new DrawingScene();
    newScene->setSceneRect(-500, -500, 1000, 1000);
    newScene->setBackgroundBrush(Qt::white);
    newScene->setColor(m_frames[m_currentFrame]->currentColor());
    newScene->setBrushWidth(m_frames[m_currentFrame]->brushWidth());

    // Copy all items from current scene to new scene
    foreach(QGraphicsItem * item, m_frames[m_currentFrame]->items()) {
        // Specifically look for StrokeItem objects
        if (StrokeItem* strokeItem = dynamic_cast<StrokeItem*>(item)) {
            // Use the clone method for proper deep copying
            StrokeItem* newStrokeItem = strokeItem->clone();
            newScene->addItem(newStrokeItem);
        }
    }

    // Reset selection state to ensure proper initialization
    newScene->resetSelectionState();

    // Insert after current frame (not at the end)
    m_frames.insert(m_currentFrame + 1, newScene);

    // Select the new frame
    m_currentFrame = m_currentFrame + 1;

    // Update the timeline and view
    m_timeline->setFrames(m_frames.size(), m_currentFrame);
    m_view->setScene(m_frames[m_currentFrame]);
}

void MainWindow::onRemoveFrame() {
    if (m_frames.size() > 1) {
        delete m_frames.takeAt(m_currentFrame);
        m_currentFrame = qMin(m_currentFrame, m_frames.size() - 1);
        m_timeline->setFrames(m_frames.size(), m_currentFrame);
        m_view->setScene(m_frames[m_currentFrame]);
    }
}

void MainWindow::onPlaybackToggled(bool playing) {
    if (playing) {
        m_animationTimer->start();
    }
    else {
        m_animationTimer->stop();
    }
}

void MainWindow::onFrameRateChanged(int fps) {
    // Convert FPS to milliseconds per frame
    int msPerFrame = 1000 / fps;
    m_animationTimer->setInterval(msPerFrame);
}

void MainWindow::advanceFrame() {
    // Move to next frame during playback
    int nextFrame = (m_currentFrame + 1) % m_frames.size();
    onFrameSelected(nextFrame);
}   