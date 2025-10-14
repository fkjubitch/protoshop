#include "transformablelineitem.h"
#include <QPainter>
#include <QLineF>
#include <QtMath>

TransformableLineItem::TransformableLineItem(const QLineF &line,
                                             QGraphicsItem *parent)
    : QGraphicsLineItem(line, parent)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemIsFocusable);
    setAcceptHoverEvents(true);
}

QRectF TransformableLineItem::boundingRect() const
{
    qreal extra = HANDLE_SIZE + ROTATE_HANDLE_OFFSET;
    return QRectF(line().p1(), line().p2())
        .normalized()
        .adjusted(-extra, -extra, extra, extra);
}

void TransformableLineItem::paint(QPainter *painter,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *widget)
{
    QGraphicsLineItem::paint(painter, option, widget);
    if (!isSelected()) return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(Qt::black, 1));
    painter->setBrush(Qt::white);

    /* 端点手柄 */
    QPointF p1 = line().p1();
    QPointF p2 = line().p2();
    painter->drawRect(QRectF(p1 - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
                             QSize(HANDLE_SIZE, HANDLE_SIZE)));
    painter->drawRect(QRectF(p2 - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
                             QSize(HANDLE_SIZE, HANDLE_SIZE)));

    /* 旋转手柄 */
    QPointF c = lineCenter();
    QPointF dir = p2 - p1;
    if (!qFuzzyIsNull(dir.manhattanLength()))
        dir = QPointF(-dir.y(), dir.x()) / dir.manhattanLength(); // 垂直单位向量
    QPointF rotPos = c + dir * ROTATE_HANDLE_OFFSET;
    painter->drawLine(c, rotPos);
    painter->drawEllipse(QRectF(rotPos - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
                                QSize(HANDLE_SIZE, HANDLE_SIZE)));
}

QPointF TransformableLineItem::lineCenter() const
{
    const QLineF l = line();
    return (l.p1() + l.p2()) * 0.5;
}

TransformableLineItem::Handle
TransformableLineItem::handleAt(const QPointF &pos)
{
    QPointF p1 = line().p1();
    QPointF p2 = line().p2();
    QPointF c  = lineCenter();

    QRectF p1Rect(p1 - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
                  QSize(HANDLE_SIZE, HANDLE_SIZE));
    QRectF p2Rect(p2 - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
                  QSize(HANDLE_SIZE, HANDLE_SIZE));

    QPointF dir = p2 - p1;
    if (!qFuzzyIsNull(dir.manhattanLength()))
        dir = QPointF(-dir.y(), dir.x()) / dir.manhattanLength();
    QPointF rotPos = c + dir * ROTATE_HANDLE_OFFSET;
    QRectF rotRect(rotPos - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
                   QSize(HANDLE_SIZE, HANDLE_SIZE));

    if (p1Rect.contains(pos)) return Pole1Handle;
    if (p2Rect.contains(pos)) return Pole2Handle;
    if (rotRect.contains(pos)) return RotateHandle;
    return NoHandle;
}

void TransformableLineItem::setHandleCursor(Handle handle)
{
    isRotateHandle = false;
    switch (handle) {
    case Pole1Handle:
    case Pole2Handle:
        setCursor(Qt::CrossCursor); break;
    case RotateHandle:
        isRotateHandle = true;      break;
    default:
        setCursor(Qt::ArrowCursor); break;
    }
}

void TransformableLineItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    Handle handle = handleAt(event->pos());
    setHandleCursor(handle);
    QGraphicsLineItem::hoverMoveEvent(event);
}

/* ====== 旋转核心 ====== */
void TransformableLineItem::receiveSceneMousePosition(
    const QPointF &scenePos, MouseLeftClickStatus status)
{
    if (isSelected() || isRotateHandling)
    {
        if (!this->isUnderMouse())
        {
            QPointF itemPos = mapFromScene(scenePos);
            Handle handle = handleAt(itemPos);
            setHandleCursor(handle);

            if (status == MouseLeftClickStatus::PRESS && handle == RotateHandle
                && !isRotateHandling) {
                m_currentHandle     = RotateHandle;
                m_mouseDownPos      = mapToScene(scenePos);
                m_mouseDownLine     = line();
                m_centerPoint       = lineCenter();
                m_initialRotation   = rotation();
                isRotateHandling    = true;
            }
        }
        if (status == MouseLeftClickStatus::RELEASE) {
            m_currentHandle  = NoHandle;
            isRotateHandling = false;
        }

        if (isRotateHandling) {
            QLineF start(m_centerPoint, m_mouseDownPos);
            QLineF curr(m_centerPoint, scenePos);
            qreal angleDelta = curr.angleTo(start);
            setTransformOriginPoint(lineCenter());
            setRotation(m_initialRotation - angleDelta);
        }
    }
}

void TransformableLineItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Handle handle = handleAt(event->pos());
    if (handle == NoHandle) {
        QGraphicsLineItem::mousePressEvent(event);
        return;
    }
    m_currentHandle = handle;
    m_mouseDownPos  = event->pos();          // item 坐标
    m_mouseDownLine = line();
    m_centerPoint   = lineCenter();          // item 坐标
    if (handle == RotateHandle)
        m_initialRotation = rotation();
}

void TransformableLineItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_currentHandle == NoHandle) {
        QGraphicsLineItem::mouseMoveEvent(event);
        return;
    }
    prepareGeometryChange();

    QPointF p1 = m_mouseDownLine.p1();
    QPointF p2 = m_mouseDownLine.p2();

    if (m_currentHandle == Pole1Handle)
        p1 = event->pos();
    else if (m_currentHandle == Pole2Handle)
        p2 = event->pos();
    else if (m_currentHandle == RotateHandle)
        return; // 已在 receiveSceneMousePosition 处理

    setLine(QLineF(p1, p2));
    setTransformOriginPoint(QLineF(p1, p2).center());
}

void TransformableLineItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    m_currentHandle = NoHandle;
    isRotateHandling = false;
}
