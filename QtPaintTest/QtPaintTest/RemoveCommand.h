#pragma once
#include <QtWidgets>
#include "DrawingScene.h"
#include "StrokeItem.h"

class RemoveCommand : public QUndoCommand {
public:
    RemoveCommand(DrawingScene* scene, BaseItem* item, QUndoCommand* parent = nullptr);
    ~RemoveCommand();
    void undo() override;
    void redo() override;
private:
    DrawingScene* myScene;
    BaseItem* myItem;
};