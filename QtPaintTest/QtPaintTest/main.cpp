#include <QApplication>
#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QMouseEvent>
#include <QTimer>
#include <QVector2D>
#include <QPen>
#include <QElapsedTimer>
#include <QLabel>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QSlider>
#include <QColorDialog>
#include <QPushButton>
#include <QToolBar>

class DrawingView : public QGraphicsView {
    Q_OBJECT
public:
    DrawingView(QGraphicsScene* scene, QWidget* parent = nullptr)
        : QGraphicsView(scene, parent), cooldown(100), tangentStrength(0.33),
        currentColor(Qt::blue), currentWidth(2) {
        realPathItem = new QGraphicsPathItem();
        tempPathItem = new QGraphicsPathItem();

        updatePens();

        scene->addItem(realPathItem);
        scene->addItem(tempPathItem);

        cooldownTimer.setInterval(cooldown);
        connect(&cooldownTimer, &QTimer::timeout, this, &DrawingView::commitSegment);
    }

    int getTotalCommittedPoints() const { return totalCommittedPoints; }
    int getRealPoints() const { return realPoints; }
    int getCurrentTempPoints() const { return tempPoints.size(); }

    QColor getColor() const { return currentColor; }

    void setColor(const QColor& color) {
        currentColor = color;
        updatePens();
    }

    void setWidth(int width) {
        currentWidth = width;
        updatePens();
    }


protected:
    void paintEvent(QPaintEvent* event) override {
        frameCount++;
        QGraphicsView::paintEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            startNewStroke(mapToScene(event->pos()));
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton) {
            addTempPoint(mapToScene(event->pos()));
            event->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            finalCommit();
            event->accept();
        }
    }

private slots:
    void commitSegment() {
        if (tempPoints.size() < 2) return;

        // Track committed points
        totalCommittedPoints += tempPoints.size() - 1;
        realPoints++;

        QPointF start = realPath.currentPosition();
        QPointF end = tempPoints.last();

        QVector2D startDir = calculateTangent(0, 3);
        QVector2D endDir = calculateTangent(qMax(0, tempPoints.size() - 3), 3);

        QPointF c1 = start + (startDir * tangentStrength * QVector2D(end - start).length()).toPointF();
        QPointF c2 = end - (endDir * tangentStrength * QVector2D(end - start).length()).toPointF();

        realPath.cubicTo(c1, c2, end);
        realPathItem->setPath(realPath);

        tempPoints = { end };
        updateTempPath();
    }

    void finalCommit() {
        if (tempPoints.size() > 1) commitSegment();
        cooldownTimer.stop();
        optimizePath();
    }

private:
    void updatePens() {
        realPathItem->setPen(QPen(currentColor, currentWidth));
        tempPathItem->setPen(QPen(currentColor, currentWidth, Qt::DotLine));
    }

    void optimizePath() {
        // Simple post-smoothing: merge recent segments
        if (realPath.elementCount() < 4) return;

        QPainterPath newPath;
        QPointF lastPoint = realPath.elementAt(0);
        newPath.moveTo(lastPoint);

        for (int i = 1; i < realPath.elementCount(); i += 3) {
            if (i + 2 >= realPath.elementCount()) break;

            QPointF c1 = realPath.elementAt(i);
            QPointF c2 = realPath.elementAt(i + 1);
            QPointF end = realPath.elementAt(i + 2);

            // Simple optimization: merge if control points are close
            if (QVector2D(c1 - lastPoint).length() < currentWidth * 2 &&
                QVector2D(c2 - end).length() < currentWidth * 2) {
                newPath.quadTo((lastPoint + end) / 2, end);
            }
            else {
                newPath.cubicTo(c1, c2, end);
            }
            lastPoint = end;
        }
        realPath = newPath;
        realPathItem->setPath(realPath);
    }

    void startNewStroke(const QPointF& startPoint) {
        tempPath = QPainterPath();
        tempPath.moveTo(startPoint);
        tempPoints = { startPoint };
        cooldownTimer.start();
    }

    void addTempPoint(const QPointF& newPoint) {
        tempPoints.append(newPoint);
        updateTempPath();
    }

    void updateTempPath() {
        tempPath = QPainterPath();
        if (tempPoints.isEmpty()) return;

        tempPath.moveTo(tempPoints.first());
        for (int i = 1; i < tempPoints.size(); ++i) {
            tempPath.lineTo(tempPoints[i]);
        }
        tempPathItem->setPath(tempPath);
    }

    QVector2D calculateTangent(int startIndex, int count) {
        startIndex = qBound(0, startIndex, tempPoints.size() - 2);
        const int availablePoints = tempPoints.size() - startIndex - 1;
        count = qMin(count, availablePoints);

        if (count < 1) return QVector2D(1, 0);

        QVector2D avgDirection(0, 0);
        for (int i = 0; i < count; i++) {
            const int idx = startIndex + i;
            if (idx >= 0 && idx < tempPoints.size() - 1) {
                avgDirection += QVector2D(tempPoints[idx + 1] - tempPoints[idx]);
            }
        }

        return avgDirection.normalized();
    }

    int totalCommittedPoints = 0;
	int realPoints = 0;
    QGraphicsPathItem* realPathItem;
    QGraphicsPathItem* tempPathItem;
    QPainterPath realPath;
    QPainterPath tempPath;
    QVector<QPointF> tempPoints;
    QTimer cooldownTimer;
    int cooldown;
    float tangentStrength;
    QColor currentColor;
    int currentWidth;
    int frameCount = 0;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        scene = new QGraphicsScene(this);
        view = new DrawingView(scene);
        setCentralWidget(view);
        scene->setSceneRect(0, 0, 800, 600);
        createDockWidgets();

        // Update display every 100ms
        QTimer* updateTimer = new QTimer(this);
        connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateStats);
        updateTimer->start(1000);

        createToolbar();
    }

private slots:
    void updateStats() {  // Renamed from updatePointCounters
        committedPointsLabel->setText(QString("Approx Points: %1").arg(view->getTotalCommittedPoints()));
        realPointsLabel->setText(QString("Real Points: %1").arg(view->getRealPoints()));
        fpsLabel->setText(QString("FPS: %1").arg(currentFPS));
        currentFPS = 0;
    }

    void changeColor() {
        QColor color = QColorDialog::getColor(view->getColor(), this);
        if (color.isValid()) view->setColor(color);
    }

    void changeWidth(int width) {
        view->setWidth(width);
    }

private:
    void createToolbar() {
        QToolBar* toolbar = addToolBar("Tools");

        // Color picker
        QPushButton* colorBtn = new QPushButton("Color");
        connect(colorBtn, &QPushButton::clicked, this, &MainWindow::changeColor);
        toolbar->addWidget(colorBtn);

        // Width slider
        QSlider* widthSlider = new QSlider(Qt::Horizontal);
        widthSlider->setRange(1, 50);
        widthSlider->setValue(2);
        connect(widthSlider, &QSlider::valueChanged, this, &MainWindow::changeWidth);
        toolbar->addWidget(widthSlider);

        // FPS counter
        fpsLabel = new QLabel("FPS: 0");
        toolbar->addWidget(fpsLabel);
    }

    void createDockWidgets() {
        QDockWidget* statsDock = new QDockWidget("Statistics", this);
        QWidget* content = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout();

        committedPointsLabel = new QLabel("Approximated Points: 0");
		realPointsLabel = new QLabel("Real Points: 0");
        tempPointsLabel = new QLabel("Temp Points: 0");

        layout->addWidget(committedPointsLabel);
		layout->addWidget(realPointsLabel);
        layout->addWidget(tempPointsLabel);

        // Add existing controls
        QSlider* smoothingSlider = new QSlider(Qt::Horizontal);

        content->setLayout(layout);
        statsDock->setWidget(content);
        addDockWidget(Qt::RightDockWidgetArea, statsDock);
    }

    QGraphicsScene* scene;
    DrawingView* view;
    QLabel* committedPointsLabel;
    QLabel* realPointsLabel;
    QLabel* tempPointsLabel;
    QLabel* fpsLabel;
    int currentFPS = 0;

};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow mainWindow;
    mainWindow.resize(800, 600);
    mainWindow.show();
    return app.exec();
}

#include "main.moc"