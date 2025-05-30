#pragma once
#include <QtWidgets>
#include "Layer.h"
#include "LayerManager.h"

class LayerListItem : public QWidget {
    Q_OBJECT
public:
    LayerListItem(Layer* layer, QWidget* parent = nullptr) : QWidget(parent), m_layer(layer) {
        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 2, 4, 2);

        // Visibility toggle
        m_visibilityCheckBox = new QCheckBox(this);
        m_visibilityCheckBox->setChecked(layer->isVisible());
        m_visibilityCheckBox->setToolTip(tr("Toggle visibility"));
        connect(m_visibilityCheckBox, &QCheckBox::toggled, this, &LayerListItem::onVisibilityToggled);

        // Lock toggle
        m_lockButton = new QToolButton(this);
        m_lockButton->setCheckable(true);
        m_lockButton->setChecked(layer->isLocked());
        m_lockButton->setIcon(QIcon(layer->isLocked() ?
            QApplication::style()->standardIcon(QStyle::SP_DriveHDIcon) :
            QApplication::style()->standardIcon(QStyle::SP_DriveFDIcon)));
        m_lockButton->setToolTip(tr("Toggle lock"));
        connect(m_lockButton, &QToolButton::toggled, this, &LayerListItem::onLockToggled);

        // Layer name
        m_nameEdit = new QLineEdit(layer->getName(), this);
        connect(m_nameEdit, &QLineEdit::editingFinished, this, &LayerListItem::onNameEdited);

        layout->addWidget(m_visibilityCheckBox);
        layout->addWidget(m_lockButton);
        layout->addWidget(m_nameEdit, 1);

        setLayout(layout);

        // Connect layer signals to update UI
        connect(layer, &Layer::nameChanged, m_nameEdit, &QLineEdit::setText);
        connect(layer, &Layer::visibilityChanged, m_visibilityCheckBox, &QCheckBox::setChecked);
        connect(layer, &Layer::lockStateChanged, this, [this](bool locked) {
            m_lockButton->setChecked(locked);
            m_lockButton->setIcon(QIcon(locked ?
                QApplication::style()->standardIcon(QStyle::SP_DriveHDIcon) :
                QApplication::style()->standardIcon(QStyle::SP_DriveFDIcon)));
            });
    }

    Layer* getLayer() const { return m_layer; }

private slots:
    void onVisibilityToggled(bool visible) {
        m_layer->setVisible(visible);
    }

    void onLockToggled(bool locked) {
        m_layer->setLocked(locked);
    }

    void onNameEdited() {
        m_layer->setName(m_nameEdit->text());
    }

private:
    Layer* m_layer;
    QCheckBox* m_visibilityCheckBox;
    QToolButton* m_lockButton;
    QLineEdit* m_nameEdit;
};