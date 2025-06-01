#pragma once

#include <QtWidgets>
#include "DrawingScene.h"
#include "BaseItem.h"

class AddBaseItemCommand : public QUndoCommand {
public:
    AddBaseItemCommand(DrawingScene* scene, BaseItem* item, QUndoCommand* parent = nullptr)
        : QUndoCommand(parent), myScene(scene), myItem(item), firstExecution(true)
    {
        setText(QString("Add Item %1").arg(QString::number(reinterpret_cast<uintptr_t>(item), 16)));
    }

    ~AddBaseItemCommand() {
        if (!firstExecution && myItem) {
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

    void undo() override {
        if (myScene && myItem) {
            myScene->removeItem(myItem);
            firstExecution = false;
        }
    }

    void redo() override {
        if (myScene && myItem) {
            myScene->addItem(myItem);
            myItem->update();
            firstExecution = false;
        }
    }

private:
    DrawingScene* myScene;
    BaseItem* myItem;
    bool firstExecution;
};