#ifndef TRANSFORMABLEELLIPSEITEM_H
#define TRANSFORMABLEELLIPSEITEM_H

#include "common.h"
#include <QGraphicsEllipseItem>
#include <QGraphicsSceneMouseEvent>
#include <QStyleOptionGraphicsItem>

class TransformableEllipseItem : public QGraphicsEllipseItem,
                                 public IMousePositionReceiver,
                                 public ItemCommon
{
public:
    explicit TransformableEllipseItem(const QRectF &rect = QRectF(),
                                      QGraphicsItem *parent = nullptr,
                                      bool isCircle = false);

    /* 关键重写 */
    QRectF boundingRect() const override;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;
    void receiveSceneMousePosition(const QPointF &scenePos,
                                   MouseLeftClickStatus status) override;
    QPainterPath shape() const override;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    enum Handle { NoHandle, TopLeft, TopRight, BottomLeft, BottomRight, RotateHandle };
    Handle handleAt(const QPointF &pos) const;
    void setHandleCursor(Handle h);
    QPointF ellipseCenter() const { return rect().center(); }

    Handle m_currentHandle = NoHandle;
    QRectF m_mouseDownRect;
    QPointF m_mouseDownPos;
    QPointF m_centerPoint;
    QLineF m_startLine;
    qreal m_initialRotation = 0.;

    bool isCircle;
};
#endif
