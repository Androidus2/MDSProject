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