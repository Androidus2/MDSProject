#pragma once
#include <QtWidgets>
#include "BaseItem.h"

class Layer : public QObject {
    Q_OBJECT
public:
    Layer(const QString& name = "New Layer");
    ~Layer();

    QString getName() const { return m_name; }
    void setName(const QString& name);

    bool isVisible() const { return m_visible; }
    void setVisible(bool visible);

    bool isLocked() const { return m_locked; }
    void setLocked(bool locked);

    int getZValue() const { return m_zValue; }
    void setZValue(int zValue);

    void addItem(BaseItem* item);
    void removeItem(BaseItem* item);
    QList<BaseItem*> getItems() const { return m_items; }

signals:
    void nameChanged(const QString& name);
    void visibilityChanged(bool visible);
    void lockStateChanged(bool locked);
    void zValueChanged(int zValue);

private:
    QString m_name;
    bool m_visible = true;
    bool m_locked = false;
    int m_zValue = 0;
    QList<BaseItem*> m_items;
};