#pragma once
#include <QtWidgets>
#include "Layer.h"

class DrawingScene;

class LayerManager : public QObject {
    Q_OBJECT
public:
    static LayerManager& getInstance() {
        static LayerManager instance;
        return instance;
    }

    Layer* createLayer(const QString& name = "New Layer");
    void removeLayer(Layer* layer);
    void removeLayer(int index);

    Layer* getCurrentLayer() const { return m_currentLayer; }
    void setCurrentLayer(Layer* layer);
    void setCurrentLayer(int index);

    QList<Layer*> getLayers() const { return m_layers; }
    int getLayerCount() const { return m_layers.size(); }

    void moveLayerUp(Layer* layer);
    void moveLayerDown(Layer* layer);

    void updateZValues();

    void setScene(DrawingScene* scene) { m_scene = scene; }
    DrawingScene* getScene() const { return m_scene; }

signals:
    void layerAdded(Layer* layer);
    void layerRemoved(Layer* layer);
    void layerMoved(int oldIndex, int newIndex);
    void currentLayerChanged(Layer* layer);

private:
    LayerManager();
    LayerManager(const LayerManager&) = delete;
    LayerManager& operator=(const LayerManager&) = delete;

    QList<Layer*> m_layers;
    Layer* m_currentLayer = nullptr;
    DrawingScene* m_scene = nullptr;
};