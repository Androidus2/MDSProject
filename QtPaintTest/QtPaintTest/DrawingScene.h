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

class DrawingScene : public QGraphicsScene {
    Q_OBJECT
public:
    DrawingScene(QObject* parent = nullptr);
    void keyReleaseEvent(QKeyEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};