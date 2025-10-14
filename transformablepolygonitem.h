#ifndef TRANSFORMABLEPOLYGONITEM_H
#define TRANSFORMABLEPOLYGONITEM_H

#include "common.h"
#include <QGraphicsPolygonItem>

class TransformablePolygonItem : public QGraphicsPolygonItem,
                                 public IMousePositionReceiver,
                                 public ItemCommon
{
public:
    explicit TransformablePolygonItem(const QPolygonF &poly = QPolygonF(),
                                      QGraphicsItem *parent = nullptr);

    /* 关键重写 */
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;
    QPainterPath shape() const override;

    /* 接收来自 CustomView 的广播 */
    void receiveSceneMousePosition(const QPointF &scenePos,
                                   MouseLeftClickStatus status) override;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    enum Handle { NoHandle, Node0, Node1, Node2, Node3,
                  Node4, Node5, Node6, Node7, RotateHandle };
    Handle handleAt(const QPointF &pos) const;
    void setHandleCursor(Handle h);
    QPointF nodePos(int idx) const;
    QRectF handleRect(const QPointF &c) const;

    Handle m_currentHandle = NoHandle;
    QPolygonF m_startPolygon;          // mousePress 时的原始多边形
    QPointF m_mouseDownScene;          // mousePress 时的场景坐标
    QPointF m_center;                  // 几何中心
    qreal m_initialRotation = 0;
};

#endif // TRANSFORMABLEPOLYGONITEM_H
