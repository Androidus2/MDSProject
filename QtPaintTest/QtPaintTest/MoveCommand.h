#pragma once
#include <QtWidgets>
#include "DrawingScene.h"
#include "StrokeItem.h"


class MoveCommand : public QUndoCommand {
public:
    MoveCommand(DrawingScene* scene,
        const QList<StrokeItem*>& items,
        const QPointF& moveDelta,
        QUndoCommand* parent = nullptr);
    ~MoveCommand();
    void undo() override;
    void redo() override;
    bool mergeWith(const QUndoCommand* other) override;
    int id() const override { return 1; } // ID for merging moves

    // Add a public accessor for getting number of items
    int itemCount() const { return movedItems.size(); }

private:
    QTime timestamp;
    DrawingScene* myScene;
    QList<StrokeItem*> movedItems;
    QPointF delta;
};