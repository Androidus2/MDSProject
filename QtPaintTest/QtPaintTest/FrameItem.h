#pragma once
#include <QtWidgets>

class FrameItem : public QObject, public QGraphicsRectItem {
    Q_OBJECT

public:
    FrameItem(int frameIndex, qreal x, qreal y, qreal width, qreal height, QGraphicsItem* parent = nullptr)
        : QGraphicsRectItem(x, y, width, height, parent), m_frameIndex(frameIndex) {
        setFlag(QGraphicsItem::ItemIsSelectable);
    }

signals:
    void frameClicked(int frameIndex);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        QGraphicsRectItem::mousePressEvent(event);
        emit frameClicked(m_frameIndex);
    }

private:
    int m_frameIndex;
};