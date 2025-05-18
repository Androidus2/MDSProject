#include "DrawingScene.h"
#include "DrawingManager.h"
#include <fstream>


DrawingScene::DrawingScene(QObject* parent)
    : QGraphicsScene(parent) {
}

// Handle mouse press event
void DrawingScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    DrawingManager::getInstance().mousePressEvent(event);
    QGraphicsScene::mousePressEvent(event);
}

void DrawingScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    DrawingManager::getInstance().mouseMoveEvent(event);
    QGraphicsScene::mouseMoveEvent(event);
}

void DrawingScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    DrawingManager::getInstance().mouseReleaseEvent(event);
    QGraphicsScene::mouseReleaseEvent(event);
}

void DrawingScene::keyPressEvent(QKeyEvent* event) {
    DrawingManager::getInstance().keyPressEvent(event);
    //QGraphicsScene::keyPressEvent(event);
}

void DrawingScene::keyReleaseEvent(QKeyEvent* event) {
    DrawingManager::getInstance().keyReleaseEvent(event);
    //QGraphicsScene::keyReleaseEvent(event);
}