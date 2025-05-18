#pragma once
#include <QtWidgets>
#include "DrawingScene.h"
#include "StrokeItem.h"


class EraseCommand : public QUndoCommand {
public:
    EraseCommand(DrawingScene* scene,
        const QList<StrokeItem*>& originals,
        const QList<StrokeItem*>& results,
        QUndoCommand* parent = nullptr);
    ~EraseCommand();
    void undo() override;
    void redo() override;
private:
    DrawingScene* myScene;
    QList<StrokeItem*> originalItems; // Items before erase
    QList<StrokeItem*> resultItems;   // Items after erase
    bool firstExecution;
};