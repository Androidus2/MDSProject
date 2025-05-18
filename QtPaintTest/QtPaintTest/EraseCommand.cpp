#include "EraseCommand.h"

// EraseCommand Implementation
EraseCommand::EraseCommand(DrawingScene* scene,
    const QList<StrokeItem*>& originals,
    const QList<StrokeItem*>& results,
    QUndoCommand* parent) : QUndoCommand(parent), myScene(scene), originalItems(originals), resultItems(results), firstExecution(true)
{
    setText(QString("Erase %1 shape(s)").arg(originals.size()));
}

// Destructor needs to handle potential ownership of items if undone
EraseCommand::~EraseCommand() {
    if (!firstExecution) { // If undone
        // Result items were removed from scene by undo(), we own them now.
        qDeleteAll(resultItems);
    }
    else {
        // Original items were removed by redo() or initial execution.
        // If stack is cleared, we might own them.
        // Check if originals are still in the scene.
        bool originalsInScene = false;
        if (myScene && !originalItems.isEmpty()) {
            QList<QGraphicsItem*> sceneItems = myScene->items();
            for (StrokeItem* item : originalItems) {
                if (sceneItems.contains(item)) {
                    originalsInScene = true;
                    break;
                }
            }
        }
        if (!originalsInScene) {
            qDeleteAll(originalItems);
        }
    }
    originalItems.clear();
    resultItems.clear();
}

void EraseCommand::undo() {
    if (!myScene) return;
    for (StrokeItem* item : resultItems) {
        myScene->removeItem(item);
    }
    for (StrokeItem* item : originalItems) {
        myScene->addItem(item);
        item->update();
    }
    firstExecution = false;
}

void EraseCommand::redo() {
    if (!myScene) return;
    for (StrokeItem* item : originalItems) {
        myScene->removeItem(item);
    }
    for (StrokeItem* item : resultItems) {
        myScene->addItem(item);
        item->update();
    }
    firstExecution = true;
}