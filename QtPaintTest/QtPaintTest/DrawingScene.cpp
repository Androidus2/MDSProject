#include "DrawingScene.h"
#include "DrawingManager.h"
#include "BaseItem.h"
#include <fstream>

DrawingScene::DrawingScene(QObject* parent)
    : QGraphicsScene(parent) {
    // Initialize default layers when scene is created
    initializeDefaultLayers();
}

DrawingScene::~DrawingScene() {
    // Layers are managed by LayerManager, no cleanup needed here
}

void DrawingScene::addItem(QGraphicsItem* item) {
    // First add the item to the scene using the parent implementation
    QGraphicsScene::addItem(item);

    // If it's a BaseItem, add it to the current layer
    BaseItem* baseItem = dynamic_cast<BaseItem*>(item);
    if (baseItem) {
        Layer* currentLayer = LayerManager::getInstance().getCurrentLayer();
        if (currentLayer) {
            currentLayer->addItem(baseItem);
        }
    }
}

void DrawingScene::initializeDefaultLayers() {
    // Set this scene as the current scene in LayerManager
    LayerManager::getInstance().setScene(this);

    // Create a default layer if none exists
    if (LayerManager::getInstance().getLayerCount() == 0) {
        Layer* defaultLayer = LayerManager::getInstance().createLayer("Background");
        LayerManager::getInstance().setCurrentLayer(defaultLayer);
    }
}

QList<BaseItem*> DrawingScene::getAllBaseItems() const {
    QList<BaseItem*> result;

    // Iterate through all items in the scene
    for (QGraphicsItem* item : items()) {
        BaseItem* baseItem = dynamic_cast<BaseItem*>(item);
        if (baseItem) {
            result.append(baseItem);
        }
    }

    return result;
}

void DrawingScene::clear() {
    // Clear the scene
    QGraphicsScene::clear();

    // Also clear the layers associated with this scene
    LayerManager& manager = LayerManager::getInstance();
    if (manager.getScene() == this) {
        // Remove all layers except one
        while (manager.getLayerCount() > 1) {
            manager.removeLayer(manager.getLayerCount() - 1);
        }

        // Reset the remaining layer
        if (manager.getLayerCount() == 1) {
            Layer* layer = manager.getLayers().first();
            layer->setName("Background");
        }
        else {
            // If no layers remain, create a default one
            initializeDefaultLayers();
        }
    }
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