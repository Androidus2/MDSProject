#pragma once
#include <QtWidgets>
#include "LayerManager.h"

class LayerPanel : public QWidget {
    Q_OBJECT
public:
    LayerPanel(QWidget* parent = nullptr);
    void updateLayerList();

private slots:
    void onAddLayer();
    void onRemoveLayer();
    void onMoveLayerUp();
    void onMoveLayerDown();
    void onLayerSelected(int row);
    void onLayerVisibilityToggled(int row);
    void onLayerLockToggled(int row);
    void onLayerNameEdited(int row, const QString& name);

private:
    void setupUI();
    void connectSignals();

    QListWidget* m_layerList;
    QPushButton* m_addLayerBtn;
    QPushButton* m_removeLayerBtn;
    QPushButton* m_moveUpBtn;
    QPushButton* m_moveDownBtn;
};