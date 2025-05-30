#include "LayerManager.h"
#include "DrawingScene.h"

LayerManager::LayerManager()
{
}

Layer* LayerManager::createLayer(const QString& name)
{
    Layer* layer = new Layer(name);

    // Calculate Z-value for the new layer (place on top)
    int zValue = 0;
    if (!m_layers.isEmpty()) {
        zValue = m_layers.first()->getZValue() + 1;
    }
    layer->setZValue(zValue);

    // Add to the beginning of the list (top of the stack)
    m_layers.prepend(layer);

    // If this is the first layer, make it the current layer
    if (m_layers.size() == 1) {
        m_currentLayer = layer;
    }

    emit layerAdded(layer);
    return layer;
}

void LayerManager::removeLayer(Layer* layer)
{
    int index = m_layers.indexOf(layer);
    if (index != -1) {
        removeLayer(index);
    }
}

void LayerManager::removeLayer(int index)
{
    if (index >= 0 && index < m_layers.size()) {
        Layer* layer = m_layers.at(index);

        // Make sure we don't delete the last layer
        if (m_layers.size() <= 1) {
            return;
        }

        // Update current layer if needed
        if (layer == m_currentLayer) {
            // Choose another layer to be current
            int newIndex = (index > 0) ? index - 1 : 0;
            m_currentLayer = m_layers.at(newIndex);
            emit currentLayerChanged(m_currentLayer);
        }

        // Remove from list
        m_layers.removeAt(index);

        // Notify about removal
        emit layerRemoved(layer);

        // Delete the layer
        delete layer;
    }
}

void LayerManager::setCurrentLayer(Layer* layer)
{
    if (m_layers.contains(layer) && m_currentLayer != layer) {
        m_currentLayer = layer;
        emit currentLayerChanged(m_currentLayer);
    }
}

void LayerManager::setCurrentLayer(int index)
{
    if (index >= 0 && index < m_layers.size()) {
        setCurrentLayer(m_layers.at(index));
    }
}

void LayerManager::moveLayerUp(Layer* layer)
{
    int index = m_layers.indexOf(layer);
    if (index > 0) {
        // Manually swap elements instead of using swap function
        m_layers.move(index, index - 1);
        updateZValues();
        emit layerMoved(index, index - 1);
    }
}

void LayerManager::moveLayerDown(Layer* layer)
{
    int index = m_layers.indexOf(layer);
    if (index >= 0 && index < m_layers.size() - 1) {
        // Manually swap elements instead of using swap function
        m_layers.move(index, index + 1);
        updateZValues();
        emit layerMoved(index, index + 1);
    }
}

void LayerManager::updateZValues()
{
    // Top layer (index 0) gets highest Z value
    for (int i = 0; i < m_layers.size(); ++i) {
        m_layers[i]->setZValue(m_layers.size() - i - 1);
    }
}