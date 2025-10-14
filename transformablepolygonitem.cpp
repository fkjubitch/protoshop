#include "transformablepolygonitem.h"
#include <QPainter>
#include <QtMath>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>

TransformablePolygonItem::TransformablePolygonItem(const QPolygonF &poly,
                                                   QGraphicsItem *parent)
    : QGraphicsPolygonItem(poly, parent)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemIsFocusable);
    setAcceptHoverEvents(true);
}

QRectF TransformablePolygonItem::boundingRect() const
{
    const qreal extra = HANDLE_SIZE + ROTATE_HANDLE_OFFSET;
    return polygon().boundingRect().adjusted(-extra, -extra, extra, extra);
}

QPainterPath TransformablePolygonItem::shape() const
{
    QPainterPath p;
    p.addPolygon(polygon().boundingRect());
    return p;
}

QPointF TransformablePolygonItem::nodePos(int idx) const
{
    const QPolygonF poly = polygon();
    return idx < poly.size() ? poly.at(idx) : QPointF();
}

QRectF TransformablePolygonItem::handleRect(const QPointF &c) const
{
    return QRectF(c - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
                  QSize(HANDLE_SIZE, HANDLE_SIZE));
}

TransformablePolygonItem::Handle
TransformablePolygonItem::handleAt(const QPointF &pos) const
{
    const QPolygonF poly = polygon();
    for (int i = 0; i < poly.size(); ++i)
        if (handleRect(nodePos(i)).contains(pos))
            return static_cast<Handle>(Node0 + i);

    /* 旋转手柄：顶点质心上方 */
    const QPointF centroid = polygon().boundingRect().center();
    const qreal topY = polygon().boundingRect().top();
    const QPointF rotateHandle(centroid.x(), topY - ROTATE_HANDLE_OFFSET);
    if (handleRect(rotateHandle).contains(pos))
        return RotateHandle;
    return NoHandle;
}

void TransformablePolygonItem::setHandleCursor(Handle h)
{
    isRotateHandle = false;
    switch (h) {
    case Node0: case Node1: case Node2: case Node3:
    case Node4: case Node5: case Node6: case Node7:
        setCursor(Qt::CrossCursor); break;
    case RotateHandle: isRotateHandle = true; break;
    default: setCursor(Qt::ArrowCursor); break;
    }
}

void TransformablePolygonItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    setHandleCursor(handleAt(event->pos()));
    QGraphicsPolygonItem::hoverMoveEvent(event);
}

void TransformablePolygonItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_currentHandle = handleAt(event->pos());
    if (m_currentHandle == NoHandle) {
        QGraphicsPolygonItem::mousePressEvent(event);
        return;
    }
    m_startPolygon  = polygon();
    m_mouseDownScene= mapToScene(event->pos());
    m_center        = polygon().boundingRect().center();
    if (m_currentHandle == RotateHandle)
        m_initialRotation = rotation();
}

void TransformablePolygonItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_currentHandle == NoHandle) {
        QGraphicsPolygonItem::mouseMoveEvent(event);
        return;
    }
    prepareGeometryChange();
    if (m_currentHandle == RotateHandle) return; // 广播里处理

    /* 简易缩放：拖动节点即移动该顶点 */
    const QPointF local = event->pos();
    QPolygonF newPoly = m_startPolygon;
    int idx = m_currentHandle - Node0;
    if (idx >= 0 && idx < newPoly.size()) {
        newPoly[idx] = local;
        setPolygon(newPoly);
        setTransformOriginPoint(newPoly.boundingRect().center());
    }
}

void TransformablePolygonItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    m_currentHandle = NoHandle;
    isRotateHandling = false;
}

/* ================= 旋转 ================= */
void TransformablePolygonItem::receiveSceneMousePosition(
    const QPointF &scenePos, MouseLeftClickStatus status)
{
    if (!isSelected() && !isRotateHandling) return;

    if (!isUnderMouse()) {
        const QPointF ip = mapFromScene(scenePos);
        Handle h = handleAt(ip);
        setHandleCursor(h);

        if (status == MouseLeftClickStatus::PRESS
            && h == RotateHandle && !isRotateHandling) {
            m_currentHandle    = RotateHandle;
            m_mouseDownScene   = scenePos;
            m_startPolygon     = polygon();
            m_center           = polygon().boundingRect().center();
            m_initialRotation  = rotation();
            isRotateHandling   = true;
        }
    }
    if (status == MouseLeftClickStatus::RELEASE) {
        m_currentHandle  = NoHandle;
        isRotateHandling = false;
    }

    if (isRotateHandling) {
        const QPointF cScene = mapToScene(m_center);
        QLineF start(cScene, m_mouseDownScene);
        QLineF curr(cScene, scenePos);
        qreal angleDelta = start.angleTo(curr);
        setTransformOriginPoint(m_center);
        setRotation(m_initialRotation - angleDelta);
    }
}

/* ================= 绘制 ================= */
void TransformablePolygonItem::paint(QPainter *painter,
                                     const QStyleOptionGraphicsItem *option,
                                     QWidget *widget)
{
    QGraphicsPolygonItem::paint(painter, option, widget);
    if (!isSelected()) return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(Qt::black, 1));
    painter->setBrush(Qt::white);

    /* 节点手柄 */
    const QPolygonF poly = polygon();
    for (int i = 0; i < poly.size(); ++i)
        painter->drawRect(handleRect(nodePos(i)));

    /* 旋转手柄 */
    const QPointF centroid = poly.boundingRect().center();
    const qreal topY = poly.boundingRect().top();
    const QPointF rotateHandle(centroid.x(), topY - ROTATE_HANDLE_OFFSET);
    painter->drawEllipse(handleRect(rotateHandle));
}
