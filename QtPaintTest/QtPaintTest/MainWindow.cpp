﻿#include "MainWindow.h"
#include "FileIOOperations.h"
#include "ManipulatableGraphicsView.h"
#include "DrawingManager.h"

MainWindow::MainWindow() : m_currentFrame(0) {
    // Create the undo stack first
    m_undoStack = new QUndoStack(this);
	DrawingManager::getInstance().setUndoStack(m_undoStack);

    // Create initial frames (3 instead of just 1)
    for (int i = 0; i < 3; i++) {
        DrawingScene* scene = new DrawingScene();
        scene->setSceneRect(-500, -500, 1000, 1000);
        scene->setBackgroundBrush(Qt::white);
        //scene->setUndoStack(m_undoStack); // Set the undo stack for each scene
        m_frames.append(scene);
    }
	DrawingManager::getInstance().setScene(m_frames[m_currentFrame]);

    setupUI();
    setupTools();
    setupMenus();
    setupUndoRedo();

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
    m_view = new ManipulatableGraphicsView(m_frames[m_currentFrame], this);
    mainLayout->addWidget(m_view);

    // Timeline section with onion skin controls
    QWidget* timelineSection = new QWidget;
    QVBoxLayout* timelineSectionLayout = new QVBoxLayout(timelineSection);
    timelineSectionLayout->setSpacing(4);
    timelineSectionLayout->setContentsMargins(0, 0, 0, 0);

    // Onion skin controls in a horizontal layout
    QWidget* onionSkinControls = new QWidget;
    QHBoxLayout* onionSkinLayout = new QHBoxLayout(onionSkinControls);
    onionSkinLayout->setContentsMargins(4, 0, 4, 0);

    m_onionSkinCheckBox = new QCheckBox("Onion Skin");
    connect(m_onionSkinCheckBox, &QCheckBox::toggled, this, &MainWindow::toggleOnionSkin);

    QLabel* opacityLabel = new QLabel("Opacity:");

    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(10, 50);
    m_opacitySlider->setValue(m_onionSkinOpacity);
    m_opacitySlider->setFixedWidth(100);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &MainWindow::setOnionSkinOpacity);

    onionSkinLayout->addWidget(m_onionSkinCheckBox);
    onionSkinLayout->addWidget(opacityLabel);
    onionSkinLayout->addWidget(m_opacitySlider);
    onionSkinLayout->addStretch();

    // Add onion skin controls above timeline
    timelineSectionLayout->addWidget(onionSkinControls);

    // Timeline widget
    m_timeline = new TimelineWidget;
    timelineSectionLayout->addWidget(m_timeline);
    m_timeline->setFrames(m_frames.size(), m_currentFrame);

    // Add the timeline section to main layout
    mainLayout->addWidget(timelineSection);

    // Connect timeline signals
    connect(m_timeline, &TimelineWidget::frameSelected, this, &MainWindow::onFrameSelected);
    connect(m_timeline, &TimelineWidget::addFrameRequested, this, &MainWindow::onAddFrame);
    connect(m_timeline, &TimelineWidget::removeFrameRequested, this, &MainWindow::onRemoveFrame);
    connect(m_timeline, &TimelineWidget::playbackToggled, this, &MainWindow::onPlaybackToggled);
    connect(m_timeline, &TimelineWidget::frameRateChanged, this, &MainWindow::onFrameRateChanged);

    // Create a narrow toolbar for tools
    QToolBar* toolbar = new QToolBar;
    toolbar->setFixedWidth(50); // Keep toolbar narrow
    addToolBar(Qt::LeftToolBarArea, toolbar);
    toolbar->setMovable(true);
    toolbar->setIconSize(QSize(24, 24)); // Smaller icons (was 32x32)

    // Create a button group to manage tool selection
    QButtonGroup* toolGroup = new QButtonGroup(this);
    toolGroup->setExclusive(true);

    // REORDERED: Create tool buttons with Select first
    // Select Tool (now first)
    QToolButton* selectButton = new QToolButton();
    selectButton->setIcon(DrawingManager::getInstance().getToolByName("Select")->toolIcon());
    selectButton->setToolTip("Select");
    selectButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    selectButton->setCheckable(true);
    selectButton->setFixedSize(38, 38); // Fixed size to fit toolbar

    // Brush Tool
    QToolButton* brushButton = new QToolButton();
    brushButton->setIcon(DrawingManager::getInstance().getToolByName("Brush")->toolIcon());
    brushButton->setToolTip("Brush");
    brushButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    brushButton->setCheckable(true);
    brushButton->setChecked(true); // Default selected tool
    brushButton->setFixedSize(38, 38); // Fixed size to fit toolbar

    // Eraser Tool
    QToolButton* eraserButton = new QToolButton();
    eraserButton->setIcon(DrawingManager::getInstance().getToolByName("Eraser")->toolIcon());
    eraserButton->setToolTip("Eraser");
    eraserButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    eraserButton->setCheckable(true);
    eraserButton->setFixedSize(38, 38); // Fixed size to fit toolbar

    // Fill Tool
    QToolButton* fillButton = new QToolButton();
    fillButton->setIcon(DrawingManager::getInstance().getToolByName("Fill")->toolIcon());
    fillButton->setToolTip("Fill");
    fillButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    fillButton->setCheckable(true);
    fillButton->setFixedSize(38, 38); // Fixed size to fit toolbar

    // Add buttons to button group for exclusive selection
    toolGroup->addButton(selectButton);
    toolGroup->addButton(brushButton);
    toolGroup->addButton(eraserButton);
    toolGroup->addButton(fillButton);

    // Add buttons to toolbar in new order
    toolbar->addWidget(selectButton);
    toolbar->addWidget(brushButton);
    toolbar->addWidget(eraserButton);
    toolbar->addWidget(fillButton);

    // Connect button signals to tool selection
    connect(selectButton, &QToolButton::clicked, this, [this]() {
        DrawingManager::getInstance().setCurrentTool("Select");
        });

    connect(brushButton, &QToolButton::clicked, this, [this]() {
        DrawingManager::getInstance().setCurrentTool("Brush");
        });

    connect(eraserButton, &QToolButton::clicked, this, [this]() {
        DrawingManager::getInstance().setCurrentTool("Eraser");
        });

    connect(fillButton, &QToolButton::clicked, this, [this]() {
        DrawingManager::getInstance().setCurrentTool("Fill");
        });

    toolbar->addSeparator();

    // Add color selection button (icon only)
    m_colorButton = new QToolButton();
    m_colorButton->setIcon(createColorIcon(DrawingManager::getInstance().getColor()));
    m_colorButton->setToolTip("Color");
    m_colorButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_colorButton->setFixedSize(38, 38); // Fixed size to fit toolbar
    connect(m_colorButton, &QToolButton::clicked, this, &MainWindow::selectColor);
    toolbar->addWidget(m_colorButton);

    // Add brush size control - compact version
    toolbar->addSeparator();

    m_brushSizeSpinBox = new QSpinBox();
    m_brushSizeSpinBox->setRange(1, 100);
    m_brushSizeSpinBox->setValue(15); // Default brush width
    m_brushSizeSpinBox->setSingleStep(1);
    m_brushSizeSpinBox->setFixedWidth(40); // Adjust width to fit
    m_brushSizeSpinBox->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    m_brushSizeSpinBox->setAlignment(Qt::AlignCenter);
    m_brushSizeSpinBox->setToolTip("Brush Size");

    connect(m_brushSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &MainWindow::setBrushSize);
    toolbar->addWidget(m_brushSizeSpinBox);

    // Apply existing CSS styles with modifications for a compact toolbar
    toolbar->setStyleSheet(R"( 
/* ───── Toolbar elegant ───── */
QToolBar {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 #2e2e2e, stop:1 #262626);
    border: 1px solid #444;
    border-radius: 4px;
    padding: 4px 2px; /* Less horizontal padding */
    spacing: 1px; /* Reduced spacing */
}

/* ───── Butoane cu iconiță ───── */
QToolButton {
    background: #2f2f2f;
    border: 1px solid #3c3c3c;
    border-radius: 4px;
    margin: 1px; /* Smaller margins */
    padding: 2px; /* Less padding */
    max-width: 38px;
    max-height: 38px;
}

/* ───── Hover ───── */
QToolButton:hover {
    background: #3d3d3d;
    border: 1px solid #666;
}

/* ───── Apăsat momentan ───── */
QToolButton:pressed {
    background: #1f1f1f;
    border: 1px solid #555;
}

/* ───── Apăsat permanent (checked) ───── */
QToolButton:checked {
    background: #5c8aff;     /* Albastru elegant */
    border: 1px solid #aaccff; /* Thinner border */
}

/* ───── SpinBox Styling ───── */
QSpinBox {
    background: #2f2f2f;
    color: #fff;
    border: 1px solid #3c3c3c;
    border-radius: 4px;
    min-height: 20px; /* Smaller height */
    max-width: 40px;
    font-size: 10px; /* Smaller font */
}

QSpinBox::up-button, QSpinBox::down-button {
    subcontrol-origin: border;
    width: 12px; /* Smaller width */
    border: 1px solid #3c3c3c;
}

QSpinBox::up-button {
    subcontrol-position: top right;
    border-top-right-radius: 3px;
}

QSpinBox::down-button {
    subcontrol-position: bottom right;
    border-bottom-right-radius: 3px;
}

QToolBar::separator {
    background: #555;
    width: 1px;
    height: 1px;
    margin: 4px 2px; /* Smaller margins */
}

/* Make tooltips nicer */
QToolTip {
    background: #2a2a2a;
    color: #ffffff;
    border: 1px solid #555;
    border-radius: 4px;
    padding: 4px;
    font-size: 11px;
}
)");

    // Configure tooltip appearance
    QToolTip::setFont(QFont("Sans Serif", 9));

    statusBar();
    connect(m_view, &ManipulatableGraphicsView::keyPressedInView,
        m_frames[m_currentFrame], &DrawingScene::keyPressEvent);
    connect(m_view, &ManipulatableGraphicsView::keyReleasedInView,
        m_frames[m_currentFrame], &DrawingScene::keyReleaseEvent);
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

    // Add Import Image action
    QAction* importImageAction = fileMenu->addAction("&Import Image...");
    importImageAction->setShortcut(QKeySequence("Ctrl+I"));
    connect(importImageAction, &QAction::triggered, this, &MainWindow::importImage);

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
    m_frames[m_currentFrame]->setSceneRect(-500, -500, 1000, 1000);
    m_frames[m_currentFrame]->setBackgroundBrush(Qt::white);
}
void MainWindow::setupUndoRedo() {
    // Create undo/redo actions
    m_undoAction = m_undoStack->createUndoAction(this, tr("&Undo"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setIcon(QIcon::fromTheme("edit-undo"));

    m_redoAction = m_undoStack->createRedoAction(this, tr("&Redo"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setIcon(QIcon::fromTheme("edit-redo"));

    // Get or create Edit menu
    QMenu* editMenu = nullptr;
    for (QAction* action : menuBar()->actions()) {
        if (action->text().contains("&Edit")) {
            editMenu = action->menu();
            break;
        }
    }

    if (!editMenu) {
        editMenu = menuBar()->addMenu(tr("&Edit"));
    }

    // undo/redo actions at the beginning of Edit menu
    if (editMenu->actions().isEmpty()) {
        editMenu->addAction(m_undoAction);
        editMenu->addAction(m_redoAction);
        editMenu->addSeparator(); // Add separator after actions
    }
    else {
        // Insert undo at beginning
        editMenu->insertAction(editMenu->actions().first(), m_undoAction);

        // Get the updated actions list
        QList<QAction*> actions = editMenu->actions();
        int undoIndex = actions.indexOf(m_undoAction);

        // Insert redo after undo
        if (undoIndex != -1 && undoIndex + 1 < actions.size()) {
            editMenu->insertAction(actions[undoIndex + 1], m_redoAction);
        }
        else {
            editMenu->addAction(m_redoAction);
        }

        // Get the updated actions list again
        actions = editMenu->actions();
        int redoIndex = actions.indexOf(m_redoAction);

        // Insert separator after redo
        if (redoIndex != -1 && redoIndex + 1 < actions.size()) {
            editMenu->insertSeparator(actions[redoIndex + 1]);
        }
        else {
            editMenu->addSeparator();
        }
    }

    // Toolbar for undo/redo actions
    QToolBar* editToolbar = addToolBar(tr("Edit"));
    editToolbar->addAction(m_undoAction);
    editToolbar->addAction(m_redoAction);

    // Create the undo view at the end, after everything else is set up
    QDockWidget* undoDock = new QDockWidget(tr("History"), this);
    QUndoView* undoView = new QUndoView(m_undoStack);
    undoDock->setWidget(undoView);
    addDockWidget(Qt::RightDockWidgetArea, undoDock);
}

// Update color icon to smaller size
QIcon MainWindow::createColorIcon(const QColor& color) {
    QPixmap pixmap(24, 24); // Smaller size (was 36x36)
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Draw color swatch with border
    QRect colorRect(2, 2, 20, 20);
    painter.setPen(QPen(Qt::gray, 1));
    painter.setBrush(color);
    painter.drawRoundedRect(colorRect, 4, 4);

    return QIcon(pixmap);
}

void MainWindow::selectColor() {
    QColor current = DrawingManager::getInstance().getColor();
    QColor color = QColorDialog::getColor(current, this, "Select Color",
        QColorDialog::ShowAlphaChannel);
    if (color.isValid()) {
        DrawingManager::getInstance().setColor(color);
        m_colorButton->setIcon(createColorIcon(color));
    }
}
void MainWindow::setBrushSize(int size) {
    DrawingManager::getInstance().setWidth(size);
}

void MainWindow::onFrameSelected(int frame) {
    if (frame >= 0 && frame < m_frames.size()) {
        if (m_view->scene()) {
            disconnect(m_view, &ManipulatableGraphicsView::keyPressedInView, static_cast<DrawingScene*>(m_view->scene()), &DrawingScene::keyPressEvent);
            disconnect(m_view, &ManipulatableGraphicsView::keyReleasedInView, static_cast<DrawingScene*>(m_view->scene()), &DrawingScene::keyReleaseEvent);
        }

        m_undoStack->clear();

        m_currentFrame = frame;
        m_view->setScene(m_frames[frame]);
        DrawingManager::getInstance().setScene(m_frames[frame]);

        connect(m_view, &ManipulatableGraphicsView::keyPressedInView, m_frames[frame], &DrawingScene::keyPressEvent);
        connect(m_view, &ManipulatableGraphicsView::keyReleasedInView, m_frames[frame], &DrawingScene::keyReleaseEvent);

        m_timeline->setFrames(m_frames.size(), m_currentFrame);
    }

    /*if (m_frames[m_currentFrame]->selectedItems().size() > 0) {
        m_frames[m_currentFrame]->updateSelectionUI();
    }*/

    if (m_onionSkinEnabled) {
        updateOnionSkin();
    }
}

void MainWindow::onAddFrame() {
    // Remember whether onion skin is enabled
    bool wasOnionSkinEnabled = m_onionSkinEnabled;
    // Disable onion skin temporarily
    if (m_onionSkinEnabled) {
        toggleOnionSkin(false);
    }

    // Create new scene
    DrawingScene* newScene = new DrawingScene();
    newScene->setSceneRect(-500, -500, 1000, 1000);
    newScene->setBackgroundBrush(Qt::white);

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
    //newScene->resetSelectionState();
    if (DrawingManager::getInstance().getCurrentTool()->toolName() == "Select") {
        // Cast to SelectTool and reset selection
        SelectTool* selectTool = dynamic_cast<SelectTool*>(DrawingManager::getInstance().getCurrentTool());
        if (selectTool) {
            selectTool->resetSelectionState();
        }
    }

    // Insert after current frame (not at the end)
    m_frames.insert(m_currentFrame + 1, newScene);

    // Select the new frame
    m_currentFrame = m_currentFrame + 1;

    // Connect key event handling to the new frame
    connect(m_view, &ManipulatableGraphicsView::keyPressedInView, m_frames[m_currentFrame], &DrawingScene::keyPressEvent);
    connect(m_view, &ManipulatableGraphicsView::keyReleasedInView, m_frames[m_currentFrame], &DrawingScene::keyReleaseEvent);

    // Update the timeline and view
    m_timeline->setFrames(m_frames.size(), m_currentFrame);
    m_view->setScene(m_frames[m_currentFrame]);
    DrawingManager::getInstance().setScene(m_frames[m_currentFrame]);

    // Re-enable onion skin if it was previously enabled
    if (wasOnionSkinEnabled) {
        toggleOnionSkin(true);
    }
}

void MainWindow::onRemoveFrame() {
    if (m_frames.size() > 1) {
        delete m_frames.takeAt(m_currentFrame);
        m_currentFrame = qMin(m_currentFrame, m_frames.size() - 1);
        m_timeline->setFrames(m_frames.size(), m_currentFrame);
        m_view->setScene(m_frames[m_currentFrame]);
        DrawingManager::getInstance().setScene(m_frames[m_currentFrame]);

        if (m_onionSkinEnabled) {
            updateOnionSkin();
        }
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

void MainWindow::toggleOnionSkin(bool enabled) {
    m_onionSkinEnabled = enabled;
    updateOnionSkin();
}

void MainWindow::setOnionSkinOpacity(int opacity) {
    m_onionSkinOpacity = opacity;
    if (m_onionSkinEnabled) {
        updateOnionSkin();
    }
}

void MainWindow::importImage() {
    // Create file dialog for selecting images
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Import Image"),
        QString(),
        tr("Image Files (*.png *.jpg *.jpeg)"));

    if (fileName.isEmpty())
        return;

    // Create a RasterItem with the selected image
    RasterItem* imageItem = new RasterItem(fileName);

    // Add the RasterItem to the current scene
    m_frames[m_currentFrame]->addItem(imageItem);

    // Center the image in the view
    QRectF itemRect = imageItem->boundingRect();
    imageItem->setPos(-itemRect.width() / 2, -itemRect.height() / 2);

    // Create an undo command if needed
    // For now, we'll just add the item directly

    // Update the scene
    m_frames[m_currentFrame]->update();
}

void MainWindow::updateOnionSkin() {
    // Clear any existing onion skin items
    for (QGraphicsItemGroup* group : m_onionSkinItems) {
        m_frames[m_currentFrame]->removeItem(group);
        delete group;
    }
    m_onionSkinItems.clear();

    if (!m_onionSkinEnabled) {
        return;
    }

    // Show up to 3 previous frames with gradually decreasing opacity
    for (int i = 1; i <= 3; i++) {
        int frameIndex = m_currentFrame - i;
        if (frameIndex >= 0) {
            // Calculate opacity multiplier: 1.0 for the most recent frame,
            // decreasing for older frames (0.7, 0.4)
            float opacityMultiplier = 1.0f - ((i - 1) * 0.3f);
            addOnionSkinFrame(frameIndex, opacityMultiplier);
        }
    }

    // Add next frame (if available) - keep full opacity for this one
    if (m_currentFrame < m_frames.size() - 1) {
        addOnionSkinFrame(m_currentFrame + 1, 1.0f);
    }
}

void MainWindow::addOnionSkinFrame(int frameIndex, float opacityMultiplier) {
    QGraphicsItemGroup* group = new QGraphicsItemGroup();
    m_frames[m_currentFrame]->addItem(group);
    m_onionSkinItems.append(group);

    // Create a semi-transparent copy of each item in the source frame
    foreach(QGraphicsItem * item, m_frames[frameIndex]->items()) {
        if (StrokeItem* strokeItem = dynamic_cast<StrokeItem*>(item)) {
            StrokeItem* newItem = strokeItem->clone();

            // Set opacity with multiplier for gradual fading
            newItem->setOpacity((m_onionSkinOpacity / 100.0) * opacityMultiplier);

            // Make it non-selectable and in background
            newItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
            newItem->setZValue(-100 - (3 - opacityMultiplier * 3)); // Adjust z-value for proper layering

            // Add to group for easy management
            group->addToGroup(newItem);
        }
    }
}