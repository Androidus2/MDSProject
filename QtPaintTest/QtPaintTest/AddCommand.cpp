#include "AddCommand.h"

// AddCommand Implementation
AddCommand::AddCommand(DrawingScene* scene, StrokeItem* item, QUndoCommand* parent) : QUndoCommand(parent), myScene(scene), myItem(item), firstExecution(true)
{
    setText(QString("Add Shape %1").arg(QString::number(reinterpret_cast<uintptr_t>(item), 16)));
}

AddCommand::~AddCommand() {}

void AddCommand::undo() {
    if (myScene && myItem) {
        myScene->removeItem(myItem);
        firstExecution = false;
    }
}

void AddCommand::redo() {
    if (myScene && myItem) {
        myScene->addItem(myItem);
        myItem->update();
        firstExecution = false;
    }
}