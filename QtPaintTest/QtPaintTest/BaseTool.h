#pragma once
#include <QObject>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QIcon>

class DrawingScene;

class BaseTool : public QObject {

public:
    BaseTool();
    virtual ~BaseTool() = default;

    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) = 0;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event) = 0;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) = 0;
    virtual void keyPressEvent(QKeyEvent* event);
    virtual void keyReleaseEvent(QKeyEvent* event);

    virtual QString toolName() const = 0;
    virtual QIcon toolIcon() const = 0;

signals:
    void statusMessage(const QString& message);

};