#include "ManipulatableGraphicsView.h"
#include <QScrollBar>
#include <QApplication> 
#include <DrawingScene.h>

ManipulatableGraphicsView::ManipulatableGraphicsView(QWidget* parent)
    : QGraphicsView(parent), m_isPanning(false) {
    setFocusPolicy(Qt::StrongFocus);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorUnderMouse);
    setDragMode(QGraphicsView::NoDrag);
}

ManipulatableGraphicsView::ManipulatableGraphicsView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent), m_isPanning(false) {
    setFocusPolicy(Qt::StrongFocus);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorUnderMouse);
    setDragMode(QGraphicsView::NoDrag);
}

void ManipulatableGraphicsView::wheelEvent(QWheelEvent* event) {
    qreal scaleFactor = 1.15;
    if (event->angleDelta().y() > 0) {
        scale(scaleFactor, scaleFactor);
    }
    else {
        scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    }
    // Optional: Limit zoom levels if needed
    // qreal currentScale = transform().m11(); // or m22()
    // if (currentScale < minScale || currentScale > maxScale) { /* revert scale */ }
}

void ManipulatableGraphicsView::mousePressEvent(QMouseEvent* event) {
    // Pan with Middle Mouse Button OR Ctrl + Left Mouse Button
    if ((event->button() == Qt::MiddleButton) ||
        (event->button() == Qt::LeftButton && (QApplication::keyboardModifiers() & Qt::ControlModifier)))
    {
        m_isPanning = true;
        m_panStartPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
    else {
        QGraphicsView::mousePressEvent(event); // Pass other events to base class
    }
}

void ManipulatableGraphicsView::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPanning) {
        QPoint delta = event->pos() - m_panStartPos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        m_panStartPos = event->pos();
        event->accept();
    }
    else {
        QGraphicsView::mouseMoveEvent(event);
    }
}

void ManipulatableGraphicsView::mouseReleaseEvent(QMouseEvent* event) {
    if (m_isPanning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
    }
    else {
        QGraphicsView::mouseReleaseEvent(event);
    }
}

void ManipulatableGraphicsView::keyPressEvent(QKeyEvent* event) {
    emit keyPressedInView(event);
    if (!event->isAccepted()) {
        QGraphicsView::keyPressEvent(event);
    }
}

void ManipulatableGraphicsView::keyReleaseEvent(QKeyEvent* event) {
    emit keyReleasedInView(event);
    if (!event->isAccepted()) {
        QGraphicsView::keyReleaseEvent(event);
    }
}