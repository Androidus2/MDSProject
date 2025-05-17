#pragma once

#include <QGraphicsView>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>  

class ManipulatableGraphicsView : public QGraphicsView {
    Q_OBJECT

public:
    ManipulatableGraphicsView(QWidget* parent = nullptr);
    ManipulatableGraphicsView(QGraphicsScene* scene, QWidget* parent = nullptr);

signals:
    void keyPressedInView(QKeyEvent* event);
    void keyReleasedInView(QKeyEvent* event);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    bool m_isPanning;
    QPoint m_panStartPos;
};