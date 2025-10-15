#include "transformableellipseitem.h"
#include <QPainter>
#include <QtMath>

TransformableEllipseItem::TransformableEllipseItem(const QRectF &rect,
                                                   QGraphicsItem *parent,
                                                   bool isCircle)
    : QGraphicsEllipseItem(rect, parent), isCircle(isCircle)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemIsFocusable);
    setAcceptHoverEvents(true);
}

QRectF TransformableEllipseItem::boundingRect() const
{
    const qreal extra = HANDLE_SIZE + ROTATE_HANDLE_OFFSET;
    return rect().adjusted(-extra, -extra, extra, extra);
}

void TransformableEllipseItem::paint(QPainter *painter,
                                     const QStyleOptionGraphicsItem *option,
                                     QWidget *widget)
{
    QGraphicsEllipseItem::paint(painter, option, widget);
    if (!isSelected()) return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(Qt::black, 1));
    painter->setBrush(Qt::white);

    const QRectF r = rect();
    const auto tl = r.topLeft(), tr = r.topRight() - QPointF(HANDLE_SIZE, 0),
        bl = r.bottomLeft() - QPointF(0, HANDLE_SIZE), br = r.bottomRight() - QPointF(HANDLE_SIZE, HANDLE_SIZE);

    // 4 个缩放手柄
    auto drawHandle = [&](const QPointF &p) {
        painter->drawRect(QRectF(p,
                                 QSize(HANDLE_SIZE, HANDLE_SIZE)));
    };
    drawHandle(tl); drawHandle(tr); drawHandle(bl); drawHandle(br);

    // 旋转手柄
    const QPointF topCenter = (r.topLeft() + r.topRight()) / 2.;
    const QPointF center = ellipseCenter();
    QPointF dir = topCenter - center;
    if (!qFuzzyIsNull(QPointF::dotProduct(dir, dir)))
        dir /= sqrt(QPointF::dotProduct(dir, dir));
    const QPointF rotPos = topCenter + dir * ROTATE_HANDLE_OFFSET;
    painter->drawLine(topCenter, rotPos);
    painter->drawEllipse(QRectF(rotPos - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
                                QSize(HANDLE_SIZE, HANDLE_SIZE)));
}

QPainterPath TransformableEllipseItem::shape() const
{
    // 用 rect 作为交互区域，手柄就能被 hover 到
    QPainterPath p;
    p.addRect(rect());
    return p;
}

TransformableEllipseItem::Handle
TransformableEllipseItem::handleAt(const QPointF &pos) const
{
    const QRectF r = rect();
    const QRectF tlH(r.topLeft(),
                     QSize(HANDLE_SIZE, HANDLE_SIZE));
    const QRectF trH(r.topRight() - QPointF(HANDLE_SIZE, 0),
                     QSize(HANDLE_SIZE, HANDLE_SIZE));
    const QRectF blH(r.bottomLeft() - QPointF(0, HANDLE_SIZE),
                     QSize(HANDLE_SIZE, HANDLE_SIZE));
    const QRectF brH(r.bottomRight() - QPointF(HANDLE_SIZE, HANDLE_SIZE),
                     QSize(HANDLE_SIZE, HANDLE_SIZE));

    const QPointF topCenter = (r.topLeft() + r.topRight()) / 2.;
    const QPointF center = ellipseCenter();
    QPointF dir = topCenter - center;
    if (!qFuzzyIsNull(QPointF::dotProduct(dir, dir)))
        dir /= sqrt(QPointF::dotProduct(dir, dir));
    const QPointF rotPos = topCenter + dir * ROTATE_HANDLE_OFFSET;
    const QRectF rotH(rotPos - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
                      QSize(HANDLE_SIZE, HANDLE_SIZE));

    if (tlH.contains(pos)) return TopLeft;
    if (trH.contains(pos)) return TopRight;
    if (blH.contains(pos)) return BottomLeft;
    if (brH.contains(pos)) return BottomRight;
    if (rotH.contains(pos)) return RotateHandle;
    return NoHandle;
}

void TransformableEllipseItem::setHandleCursor(Handle h)
{
    isRotateHandle = false;
    switch (h) {
    case TopLeft: case BottomRight: setCursor(Qt::SizeFDiagCursor); break;
    case TopRight: case BottomLeft: setCursor(Qt::SizeBDiagCursor); break;
    case RotateHandle: isRotateHandle = true; break;
    default: setCursor(Qt::ArrowCursor); break;
    }
}

void TransformableEllipseItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    setHandleCursor(handleAt(event->pos()));
    QGraphicsEllipseItem::hoverMoveEvent(event);
}

void TransformableEllipseItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_currentHandle = handleAt(event->pos());
    if (m_currentHandle != NoHandle) {
        m_mouseDownPos = event->pos();
        m_mouseDownRect = rect();
        m_centerPoint = ellipseCenter();
        if (m_currentHandle == RotateHandle)
            m_initialRotation = rotation();
    } else {
        QGraphicsEllipseItem::mousePressEvent(event);
    }
}

void TransformableEllipseItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_currentHandle == NoHandle) {
        QGraphicsEllipseItem::mouseMoveEvent(event);
        return;
    }
    prepareGeometryChange();
    QRectF newRect = m_mouseDownRect;
    const QPointF p = event->pos();
    switch (m_currentHandle) {
    case TopLeft:     newRect.setTopLeft(p);     break;
    case TopRight:    newRect.setTopRight(p);    break;
    case BottomLeft:  newRect.setBottomLeft(p);  break;
    case BottomRight: newRect.setBottomRight(p); break;
    case RotateHandle: return; // 在 receiveSceneMousePosition 中处理
    default: break;
    }
    if(isCircle){
        qreal side = qMax(newRect.width(), newRect.height());
        newRect.setSize(QSizeF(side, side));
    }
    newRect = newRect.normalized();
    setRect(newRect);
    setTransformOriginPoint(newRect.center());
}

void TransformableEllipseItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    m_currentHandle = NoHandle;
    isRotateHandling = false;
    QGraphicsEllipseItem::mouseReleaseEvent(event);
}

/* ===== 旋转核心 ===== */
void TransformableEllipseItem::receiveSceneMousePosition(const QPointF &scenePos,
                                                         MouseLeftClickStatus status)
{
    if (isSelected() || isRotateHandling)
    {
        if (!isUnderMouse())
        {
            const QPointF itemPos = mapFromScene(scenePos);
            const Handle h = handleAt(itemPos);
            setHandleCursor(h);

            if (status == MouseLeftClickStatus::PRESS
                && h == RotateHandle && !isRotateHandling) {
                m_currentHandle   = RotateHandle;
                m_mouseDownPos    = scenePos;
                m_mouseDownRect   = rect();
                m_centerPoint     = mapToScene(ellipseCenter());
                m_initialRotation = rotation();
                isRotateHandling  = true;
            }
        }
        if (status == MouseLeftClickStatus::RELEASE) {
            m_currentHandle  = NoHandle;
            isRotateHandling = false;
        }

        if (isRotateHandling) {
            const QLineF start(m_centerPoint, m_mouseDownPos);
            const QLineF curr(m_centerPoint, scenePos);
            const qreal angleDelta = start.angleTo(curr);
            setTransformOriginPoint(ellipseCenter());
            setRotation(m_initialRotation - angleDelta);
        }
    }
}
