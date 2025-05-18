#pragma once
#include <QtWidgets>
#include "DrawingScene.h"
#include "StrokeItem.h"


class AddCommand : public QUndoCommand {
public:
    AddCommand(DrawingScene* scene, StrokeItem* item, QUndoCommand* parent = nullptr);
    ~AddCommand();
    void undo() override;
    void redo() override;
private:
    DrawingScene* myScene;
    StrokeItem* myItem;
    bool firstExecution;
};