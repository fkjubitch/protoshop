#include "transformablepathitem.h"
#include <QPainter>
#include <QtMath>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>

TransformablePathItem::TransformablePathItem(const QPainterPath &path,
                                             QGraphicsItem *parent)
    : QGraphicsPathItem(path, parent)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemIsFocusable);
    setAcceptHoverEvents(true);
}

QRectF TransformablePathItem::boundingRect() const
{
    const qreal extra = HANDLE_SIZE + ROTATE_HANDLE_OFFSET;
    return path().controlPointRect().adjusted(-extra, -extra, extra, extra);
}

void TransformablePathItem::paint(QPainter *painter,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *widget)
{
    QGraphicsPathItem::paint(painter, option, widget);
    if (!isSelected()) return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(Qt::black, 1));
    painter->setBrush(Qt::white);

    /* 旋转手柄 */
    const QPointF c = pathCenter();
    const qreal top = path().controlPointRect().top();
    const QPointF rotateHandle(c.x(), top - ROTATE_HANDLE_OFFSET);
    painter->drawEllipse(QRectF(rotateHandle - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
                                QSize(HANDLE_SIZE, HANDLE_SIZE)));
}

QPainterPath TransformablePathItem::shape() const
{
    QPainterPath pathShape;
    pathShape.addRect(path().controlPointRect());
    return pathShape;
}

QPointF TransformablePathItem::pathCenter() const
{
    return path().controlPointRect().center();
}

TransformablePathItem::Handle
TransformablePathItem::handleAt(const QPointF &pos) const
{
    const QPointF c = pathCenter();
    const qreal top = path().controlPointRect().top();
    const QPointF rotateHandle(c.x(), top - ROTATE_HANDLE_OFFSET);
    if (QRectF(rotateHandle - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
               QSize(HANDLE_SIZE, HANDLE_SIZE)).contains(pos))
        return RotateHandle;
    return NoHandle;
}

void TransformablePathItem::setHandleCursor(Handle h)
{
    isRotateHandle = false;
    setCursor(h == RotateHandle ? Qt::SizeAllCursor : Qt::ArrowCursor);
    if (h == RotateHandle) isRotateHandle = true;
}

void TransformablePathItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    setHandleCursor(handleAt(event->pos()));
    QGraphicsPathItem::hoverMoveEvent(event);
}

void TransformablePathItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_currentHandle = handleAt(event->pos());
    if (m_currentHandle == NoHandle) {
        QGraphicsPathItem::mousePressEvent(event);
        return;
    }
    m_mouseDownScene = mapToScene(event->pos());
    m_center         = pathCenter();
    m_initialRotation= rotation();
}

void TransformablePathItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_currentHandle == NoHandle)
        QGraphicsPathItem::mouseMoveEvent(event);
}

void TransformablePathItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    m_currentHandle = NoHandle;
    isRotateHandling= false;
    QGraphicsPathItem::mouseReleaseEvent(event);
}

/* ===== 旋转 ===== */
void TransformablePathItem::receiveSceneMousePosition(
    const QPointF &scenePos, MouseLeftClickStatus status)
{
    if (!isSelected() && !isRotateHandling) return;

    if (!isUnderMouse()) {
        Handle h = handleAt(mapFromScene(scenePos));
        setHandleCursor(h);
        if (status == MouseLeftClickStatus::PRESS
            && h == RotateHandle && !isRotateHandling) {
            m_currentHandle   = RotateHandle;
            m_mouseDownScene  = scenePos;
            m_center          = mapToScene(pathCenter());
            m_initialRotation = rotation();
            isRotateHandling  = true;
        }
    }
    if (status == MouseLeftClickStatus::RELEASE) {
        m_currentHandle  = NoHandle;
        isRotateHandling = false;
    }
    if (isRotateHandling) {
        QLineF start(m_center, m_mouseDownScene);
        QLineF curr(m_center, scenePos);
        qreal angleDelta = start.angleTo(curr);
        setTransformOriginPoint(pathCenter());
        setRotation(m_initialRotation - angleDelta);
    }
}
