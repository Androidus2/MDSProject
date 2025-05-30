#pragma once
#include <QtWidgets>
#include <clipper2/clipper.h>
#include <QUndoCommand>
#include <QUndoStack>
#include "DrawingEngineUtils.h"
#include "StrokeItem.h"
#include "BrushTool.h"
#include "EraserTool.h"
#include "FillTool.h"
#include "SelectTool.h"
#include "Layer.h"
#include "LayerManager.h"

class DrawingScene : public QGraphicsScene {
    Q_OBJECT
public:
    DrawingScene(QObject* parent = nullptr);
    ~DrawingScene();

    // Hide the base class methods rather than override them
    void addItem(QGraphicsItem* item);
    void clear();

    // Initialize layers for this scene
    void initializeDefaultLayers();

    // Get all BaseItems in the scene
    QList<BaseItem*> getAllBaseItems() const;

    void keyReleaseEvent(QKeyEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};