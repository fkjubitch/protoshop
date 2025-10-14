#ifndef TRANSFORMABLECURVEITEM_H
#define TRANSFORMABLECURVEITEM_H

#include "common.h"
#include <QGraphicsPathItem>

class TransformableCurveItem : public QGraphicsPathItem,
                               public IMousePositionReceiver,
                               public ItemCommon
{
public:
    explicit TransformableCurveItem(const QPainterPath &path,
                                    QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;
    void receiveSceneMousePosition(const QPointF &scenePos,
                                   MouseLeftClickStatus status) override;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    enum Handle { NoHandle, StartHandle, EndHandle, Ctrl1Handle, Ctrl2Handle, RotateHandle };
    Handle handleAt(const QPointF &pos) const;
    void setHandleCursor(Handle h);
    void rebuildPath();

    Handle m_currentHandle = NoHandle;
    QPointF m_p1, m_p2;     // 起点 终点
    QPointF m_c1, m_c2;     // 两个控制点
    QPointF m_mouseDownPos;
    QPointF m_center;
    qreal m_initialRotation = 0.;
};

#endif // TRANSFORMABLECURVEITEM_H
