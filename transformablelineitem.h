#ifndef TRANSFORMABLELINEITEM_H
#define TRANSFORMABLELINEITEM_H

#include "common.h"
#include <QGraphicsLineItem>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QPointF>

class TransformableLineItem : public QGraphicsLineItem, public IMousePositionReceiver, public ItemCommon
{
public:
    TransformableLineItem(const QLineF &line, QGraphicsItem *parent = nullptr);

    // 重写关键的虚函数
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    QRectF boundingRect() const override;
    void receiveSceneMousePosition(const QPointF &scenePos, const MouseLeftClickStatus mouseLeftClickStatus) override;

protected:
    // 重写鼠标事件以处理控制点
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    // 枚举表示不同的控制点
    enum Handle {
        NoHandle,
        Pole1Handle,
        Pole2Handle,
        RotateHandle
    };

    // 用于跟踪当前正在操作的控制点
    Handle m_currentHandle = NoHandle;
    // 用于在变换时保存鼠标按下的初始状态
    QPointF m_mouseDownPos;
    QLineF m_mouseDownLine;
    QPointF m_centerPoint;
    qreal m_initialRotation = 0;

    // 辅助函数
    void setHandleCursor(Handle handle);
    Handle handleAt(const QPointF &pos);
    QPointF lineCenter() const;
};

#endif // TRANSFORMABLELINEITEM_H
