#include "MoveCommand.h"

// MoveCommand Implementation
MoveCommand::MoveCommand(DrawingScene* scene,
    const QList<BaseItem*>& items,
    const QPointF& moveDelta,
    QUndoCommand* parent)
    : QUndoCommand(parent), myScene(scene), movedItems(items), delta(moveDelta)
{
    setText(QString("Move %1 shape(s)").arg(items.size()));
    timestamp = QTime::currentTime();
}


MoveCommand::~MoveCommand() {
    // Items are managed by the scene, nothing to delete here
}

void MoveCommand::redo() {
    if (!myScene) return;
    for (BaseItem* item : movedItems) {
        if (item->scene() == myScene) {
            item->moveBy(delta.x(), delta.y());
        }
    }
}

void MoveCommand::undo() {
    if (!myScene) return;
    for (BaseItem* item : movedItems) {
        if (item->scene() == myScene) {
            item->moveBy(-delta.x(), -delta.y());
        }
    }
}

// Merge consecutive moves of the same items
bool MoveCommand::mergeWith(const QUndoCommand* other) {
    const MoveCommand* otherMove = dynamic_cast<const MoveCommand*>(other);
    if (!otherMove) return false;

    // Ensure we're merging with the last command on the stack
    if (otherMove->movedItems.size() != this->movedItems.size()) return false;

    QSet<BaseItem*> mySet(movedItems.begin(), movedItems.end());
    QSet<BaseItem*> otherSet(otherMove->movedItems.begin(), otherMove->movedItems.end());
    if (mySet != otherSet) return false;

    // Only merge if commands were created within 300ms of each other
    // (This groups moves that are part of the same operation)
    int msSinceLastMove = timestamp.msecsTo(otherMove->timestamp);
    if (msSinceLastMove > 300) return false;

    // Merge the deltas
    delta += otherMove->delta;
    timestamp = otherMove->timestamp;
    return true;
}