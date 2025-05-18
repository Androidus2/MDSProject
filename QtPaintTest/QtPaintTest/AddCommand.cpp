#include "AddCommand.h"

// AddCommand Implementation
AddCommand::AddCommand(DrawingScene* scene, StrokeItem* item, QUndoCommand* parent) : QUndoCommand(parent), myScene(scene), myItem(item), firstExecution(true)
{
    setText(QString("Add Shape %1").arg(QString::number(reinterpret_cast<uintptr_t>(item), 16)));
}

AddCommand::~AddCommand() {
    // If the command is destroyed and the item is still owned by it (i.e., was undone)
    // we need to delete the item to prevent memory leaks.
    if (!firstExecution && myItem) {
        // Check if item is still in the scene; if not, we own it.
        bool inScene = false;
        if (myScene) {
            for (QGraphicsItem* sceneItem : myScene->items()) {
                if (sceneItem == myItem) {
                    inScene = true;
                    break;
                }
            }
        }
        if (!inScene) {
            delete myItem;
            myItem = nullptr;
        }
    }
}

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