#include "RemoveCommand.h"

// RemoveCommand Implementation
RemoveCommand::RemoveCommand(DrawingScene* scene, BaseItem* item, QUndoCommand* parent) : QUndoCommand(parent), myScene(scene), myItem(item)
{
    setText(QString("Remove Shape %1").arg(QString::number(reinterpret_cast<uintptr_t>(item), 16)));
}

RemoveCommand::~RemoveCommand() {
}

void RemoveCommand::undo() {
    if (myScene && myItem) {
        myScene->addItem(myItem);
        myItem->update();
    }
}

void RemoveCommand::redo() {
    if (myScene && myItem) {
        myScene->removeItem(myItem);
    }
}