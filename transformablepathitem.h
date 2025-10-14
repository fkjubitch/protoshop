#ifndef TRANSFORMABLEPATHITEM_H
#define TRANSFORMABLEPATHITEM_H

#include "common.h"
#include <QGraphicsPathItem>

class TransformablePathItem : public QGraphicsPathItem,
                              public IMousePositionReceiver,
                              public ItemCommon
{
public:
    explicit TransformablePathItem(const QPainterPath &path,
                                   QGraphicsItem *parent = nullptr);

    /* 关键重写 */
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
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
    enum Handle { NoHandle, RotateHandle };
    Handle handleAt(const QPointF &pos) const;
    void setHandleCursor(Handle h);
    QPointF pathCenter() const;

    Handle m_currentHandle = NoHandle;
    QPointF m_mouseDownScene;
    QPointF m_center;
    qreal m_initialRotation = 0.;
};

#endif // TRANSFORMABLEPATHITEM_H
