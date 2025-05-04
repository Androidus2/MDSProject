#include "MainWindow.h"
#include "FileIOOperations.h"
#include "ManipulatableGraphicsView.h" // Include the new header
#include <QUndoStack>

MainWindow::MainWindow() {
    m_undoStack = new QUndoStack(this);

    setupUI();
    setupTools();
    setupMenus();

    m_scene.setUndoStack(m_undoStack);
}
MainWindow::~MainWindow() {
    // Clean up resources if needed
}

void MainWindow::setupUI() {
    m_view = new ManipulatableGraphicsView(&m_scene); // Use the new view class
    setCentralWidget(m_view);

    QToolBar* toolbar = new QToolBar;
    addToolBar(Qt::LeftToolBarArea, toolbar);

    toolbar->addAction("Brush", [this] { m_scene.setTool(Brush); });
    toolbar->addAction("Eraser", [this] { m_scene.setTool(Eraser); });
    toolbar->addAction("Fill", [this] { m_scene.setTool(Fill); });
    toolbar->addAction("Select", [this] { m_scene.setTool(Select); });

    // Add color selection button
    QToolButton* colorButton = new QToolButton;
    colorButton->setText("Color");
    colorButton->setIcon(createColorIcon(Qt::black));
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
	connect(newAction, &QAction::triggered, this, [this]() {
        m_undoStack->clear();
		FileIOOperations::newDrawing(m_scene, *this);
		});

    // Open action
    QAction* openAction = fileMenu->addAction("&Open...");
    openAction->setShortcut(QKeySequence::Open);
	connect(openAction, &QAction::triggered, this, [this]() {
        m_undoStack->clear();
		FileIOOperations::loadDrawing(m_scene, *this);
		});

    // Save action
    QAction* saveAction = fileMenu->addAction("&Save");
    saveAction->setShortcut(QKeySequence::Save);
	connect(saveAction, &QAction::triggered, this, [this]() {
		FileIOOperations::saveDrawing(m_scene, *this);
		});

    // Save As action
    QAction* saveAsAction = fileMenu->addAction("Save &As...");
    saveAsAction->setShortcut(QKeySequence::SaveAs);
	connect(saveAsAction, &QAction::triggered, this, [this]() {
		FileIOOperations::saveDrawingAs(m_scene, *this);
		});

    fileMenu->addSeparator();

    // Export submenu
    QMenu* exportMenu = fileMenu->addMenu("&Export");

    QAction* exportSVG = exportMenu->addAction("Export as &SVG...");
	connect(exportSVG, &QAction::triggered, this, [this]() {
		FileIOOperations::exportSVG(m_scene, *this);
		});

    QAction* exportPNG = exportMenu->addAction("Export as &PNG...");
	connect(exportPNG, &QAction::triggered, this, [this]() {
		FileIOOperations::exportPNG(m_scene, *this);
		});

    QAction* exportJPEG = exportMenu->addAction("Export as &JPEG...");
	connect(exportJPEG, &QAction::triggered, this, [this]() {
		FileIOOperations::exportJPEG(m_scene, *this);
		});

    fileMenu->addSeparator();

    // Exit action
    QAction* exitAction = fileMenu->addAction("&Exit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // --- Add Edit Menu for Undo/Redo --- 
    QMenu* editMenu = menuBar()->addMenu("&Edit");

    m_undoAction = m_undoStack->createUndoAction(this, "&Undo");
    m_undoAction->setShortcuts(QKeySequence::Undo);
    editMenu->addAction(m_undoAction);

    m_redoAction = m_undoStack->createRedoAction(this, "&Redo");
    m_redoAction->setShortcuts(QKeySequence::Redo);
    editMenu->addAction(m_redoAction);

    // Optional: Connect signals to enable/disable actions based on stack state
    // Although createUndoAction/createRedoAction often handle this automatically,
    // explicit connection ensures it.
    connect(m_undoStack, &QUndoStack::canUndoChanged, m_undoAction, &QAction::setEnabled);
    connect(m_undoStack, &QUndoStack::canRedoChanged, m_redoAction, &QAction::setEnabled);
    // Initially disable actions
    m_undoAction->setEnabled(m_undoStack->canUndo());
    m_redoAction->setEnabled(m_undoStack->canRedo());
}
void MainWindow::setupTools() {
    m_scene.setSceneRect(-10000, -10000, 20000, 20000); 
    m_scene.setBackgroundBrush(Qt::white);
}

// Create a colored icon for the color button
QIcon MainWindow::createColorIcon(const QColor& color) {
    QPixmap pixmap(24, 24);
    pixmap.fill(color);
    return QIcon(pixmap);
}

void MainWindow::selectColor() {
    QColor color = QColorDialog::getColor(m_scene.currentColor(), this, "Select Color");
    if (color.isValid()) {
        m_scene.setColor(color);
        m_colorButton->setIcon(createColorIcon(color));
    }
}
void MainWindow::setBrushSize(int size) {
    m_scene.setBrushWidth(size);
}
