#include "RemoveCommand.h"

// RemoveCommand Implementation
RemoveCommand::RemoveCommand(DrawingScene* scene, StrokeItem* item, QUndoCommand* parent) : QUndoCommand(parent), myScene(scene), myItem(item)
{
    setText(QString("Remove Shape %1").arg(QString::number(reinterpret_cast<uintptr_t>(item), 16)));
}

RemoveCommand::~RemoveCommand() {
    // QUndoStack manages command deletion. We assume the item's lifetime
    // is managed elsewhere (e.g., by the scene or another command like EraseCommand)
    // unless explicitly handled (like in AddCommand's destructor).
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