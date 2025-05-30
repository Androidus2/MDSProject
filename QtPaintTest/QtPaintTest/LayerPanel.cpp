#include "LayerPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QToolButton>
#include <QApplication>
#include <QStyle>
#include "LayerListItem.h"

LayerPanel::LayerPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    connectSignals();
    updateLayerList();
}

void LayerPanel::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Layer list
    m_layerList = new QListWidget(this);
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerList->setDragDropMode(QAbstractItemView::InternalMove);

    // Buttons panel
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_addLayerBtn = new QPushButton(tr("Add"), this);
    m_removeLayerBtn = new QPushButton(tr("Remove"), this);
    m_moveUpBtn = new QPushButton(tr("Up"), this);
    m_moveDownBtn = new QPushButton(tr("Down"), this);

    buttonLayout->addWidget(m_addLayerBtn);
    buttonLayout->addWidget(m_removeLayerBtn);
    buttonLayout->addWidget(m_moveUpBtn);
    buttonLayout->addWidget(m_moveDownBtn);

    mainLayout->addWidget(m_layerList);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void LayerPanel::connectSignals()
{
    connect(m_addLayerBtn, &QPushButton::clicked, this, &LayerPanel::onAddLayer);
    connect(m_removeLayerBtn, &QPushButton::clicked, this, &LayerPanel::onRemoveLayer);
    connect(m_moveUpBtn, &QPushButton::clicked, this, &LayerPanel::onMoveLayerUp);
    connect(m_moveDownBtn, &QPushButton::clicked, this, &LayerPanel::onMoveLayerDown);

    connect(m_layerList, &QListWidget::currentRowChanged, this, &LayerPanel::onLayerSelected);

    // Connect to LayerManager signals
    LayerManager& manager = LayerManager::getInstance();
    connect(&manager, &LayerManager::layerAdded, this, &LayerPanel::updateLayerList);
    connect(&manager, &LayerManager::layerRemoved, this, &LayerPanel::updateLayerList);
    connect(&manager, &LayerManager::layerMoved, this, &LayerPanel::updateLayerList);
    connect(&manager, &LayerManager::currentLayerChanged, this, &LayerPanel::updateLayerList);
}

void LayerPanel::updateLayerList()
{
    // Remember the current selection
    Layer* selectedLayer = nullptr;
    if (m_layerList->currentRow() >= 0) {
        QListWidgetItem* item = m_layerList->item(m_layerList->currentRow());
        LayerListItem* layerItem = qobject_cast<LayerListItem*>(m_layerList->itemWidget(item));
        if (layerItem) {
            selectedLayer = layerItem->getLayer();
        }
    }

    // Clear and rebuild the list
    m_layerList->clear();

    LayerManager& manager = LayerManager::getInstance();
    QList<Layer*> layers = manager.getLayers();

    for (Layer* layer : layers) {
        QListWidgetItem* item = new QListWidgetItem(m_layerList);
        LayerListItem* layerItem = new LayerListItem(layer, m_layerList);
        m_layerList->setItemWidget(item, layerItem);

        // Set a minimum height for the item
        item->setSizeHint(layerItem->sizeHint());

        // Highlight the current layer
        if (layer == manager.getCurrentLayer()) {
            item->setBackground(QColor(200, 220, 255));
        }
    }

    // Restore selection
    if (selectedLayer) {
        for (int i = 0; i < m_layerList->count(); ++i) {
            QListWidgetItem* item = m_layerList->item(i);
            LayerListItem* layerItem = qobject_cast<LayerListItem*>(m_layerList->itemWidget(item));
            if (layerItem && layerItem->getLayer() == selectedLayer) {
                m_layerList->setCurrentRow(i);
                break;
            }
        }
    }
}

void LayerPanel::onAddLayer()
{
    Layer* layer = LayerManager::getInstance().createLayer(tr("New Layer"));
    LayerManager::getInstance().setCurrentLayer(layer);
    updateLayerList();

	// Automatically select the newly added layer
	for (int i = 0; i < m_layerList->count(); ++i) {
		QListWidgetItem* item = m_layerList->item(i);
		LayerListItem* layerItem = qobject_cast<LayerListItem*>(m_layerList->itemWidget(item));
		if (layerItem && layerItem->getLayer() == layer) {
			m_layerList->setCurrentRow(i);
			break;
		}
	}
}

void LayerPanel::onRemoveLayer()
{
    int row = m_layerList->currentRow();
    if (row >= 0) {
        QListWidgetItem* item = m_layerList->item(row);
        LayerListItem* layerItem = qobject_cast<LayerListItem*>(m_layerList->itemWidget(item));
        if (layerItem) {
            LayerManager::getInstance().removeLayer(layerItem->getLayer());
        }
    }
}

void LayerPanel::onMoveLayerUp()
{
    int row = m_layerList->currentRow();
    if (row > 0) {
        QListWidgetItem* item = m_layerList->item(row);
        LayerListItem* layerItem = qobject_cast<LayerListItem*>(m_layerList->itemWidget(item));
        if (layerItem) {
            LayerManager::getInstance().moveLayerUp(layerItem->getLayer());
        }
    }
}

void LayerPanel::onMoveLayerDown()
{
    int row = m_layerList->currentRow();
    if (row >= 0 && row < m_layerList->count() - 1) {
        QListWidgetItem* item = m_layerList->item(row);
        LayerListItem* layerItem = qobject_cast<LayerListItem*>(m_layerList->itemWidget(item));
        if (layerItem) {
            LayerManager::getInstance().moveLayerDown(layerItem->getLayer());
        }
    }
}

void LayerPanel::onLayerSelected(int row)
{
    if (row >= 0) {
        QListWidgetItem* item = m_layerList->item(row);
        LayerListItem* layerItem = qobject_cast<LayerListItem*>(m_layerList->itemWidget(item));
        if (layerItem) {
            LayerManager::getInstance().setCurrentLayer(layerItem->getLayer());
        }
    }
}

void LayerPanel::onLayerVisibilityToggled(int row)
{
    if (row >= 0 && row < m_layerList->count()) {
        QListWidgetItem* item = m_layerList->item(row);
        LayerListItem* layerItem = static_cast<LayerListItem*>(m_layerList->itemWidget(item));
        if (layerItem) {
            Layer* layer = layerItem->getLayer();
            layer->setVisible(!layer->isVisible());
        }
    }
}

void LayerPanel::onLayerLockToggled(int row)
{
    if (row >= 0 && row < m_layerList->count()) {
        QListWidgetItem* item = m_layerList->item(row);
        LayerListItem* layerItem = static_cast<LayerListItem*>(m_layerList->itemWidget(item));
        if (layerItem) {
            Layer* layer = layerItem->getLayer();
            layer->setLocked(!layer->isLocked());
        }
    }
}

void LayerPanel::onLayerNameEdited(int row, const QString& name)
{
    if (row >= 0 && row < m_layerList->count()) {
        QListWidgetItem* item = m_layerList->item(row);
        LayerListItem* layerItem = static_cast<LayerListItem*>(m_layerList->itemWidget(item));
        if (layerItem) {
            Layer* layer = layerItem->getLayer();
            layer->setName(name);
        }
    }
}