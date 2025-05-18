#pragma once
#include <QtWidgets>
#include "DrawingScene.h"
#include "StrokeItem.h"

class RemoveCommand : public QUndoCommand {
public:
    RemoveCommand(DrawingScene* scene, StrokeItem* item, QUndoCommand* parent = nullptr);
    ~RemoveCommand();
    void undo() override;
    void redo() override;
private:
    DrawingScene* myScene;
    StrokeItem* myItem;
};