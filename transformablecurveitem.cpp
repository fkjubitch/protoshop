#include "transformablecurveitem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QtMath>
#include <QCursor>

TransformableCurveItem::TransformableCurveItem(const QPainterPath &path,
                                               QGraphicsItem *parent)
    : QGraphicsPathItem(path, parent)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemIsFocusable);
    setAcceptHoverEvents(true);
    /* 初始随便给两个端点 */
    m_p1 = QPointF(0,0); m_p2 = QPointF(100,0);
    m_c1 = QPointF(30,-30); m_c2 = QPointF(70,-30);
    rebuildPath();
}

void TransformableCurveItem::rebuildPath()
{
    QPainterPath p;
    p.moveTo(m_p1);
    p.cubicTo(m_c1, m_c2, m_p2);
    setPath(p);
}

QRectF TransformableCurveItem::boundingRect() const
{
    const qreal extra = HANDLE_SIZE + ROTATE_HANDLE_OFFSET;
    return path().controlPointRect().adjusted(-extra, -extra, extra, extra);
}

void TransformableCurveItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                   QWidget *widget)
{
    QGraphicsPathItem::paint(painter, option, widget);
    if (!isSelected()) return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(Qt::black, 1));
    painter->setBrush(Qt::white);

    auto drawCtrl = [&](const QPointF &p){
        painter->drawRect(QRectF(p - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
                                 QSize(HANDLE_SIZE, HANDLE_SIZE)));
    };
    drawCtrl(m_p1); drawCtrl(m_p2);
    drawCtrl(m_c1); drawCtrl(m_c2);
    /* 控制柄连线 */
    painter->setPen(QPen(Qt::gray, 1, Qt::DashLine));
    painter->drawLine(m_p1, m_c1);
    painter->drawLine(m_p2, m_c2);

    /* 旋转手柄 */
    QPointF top = path().controlPointRect().topLeft();
    QPointF c   = path().controlPointRect().center();
    QPointF rot = QPointF(c.x(), top.y() - ROTATE_HANDLE_OFFSET);
    painter->setPen(QPen(Qt::black, 1));
    painter->drawLine(QPointF(c.x(), top.y()), rot);
    painter->drawEllipse(QRectF(rot - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
                                QSize(HANDLE_SIZE, HANDLE_SIZE)));
}

TransformableCurveItem::Handle
TransformableCurveItem::handleAt(const QPointF &pos) const
{
    auto rect = [&](const QPointF &c){
        return QRectF(c - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2),
                      QSize(HANDLE_SIZE, HANDLE_SIZE));
    };
    if (rect(m_p1).contains(pos)) return StartHandle;
    if (rect(m_p2).contains(pos)) return EndHandle;
    if (rect(m_c1).contains(pos)) return Ctrl1Handle;
    if (rect(m_c2).contains(pos)) return Ctrl2Handle;

    QPointF c = path().controlPointRect().center();
    QPointF top = path().controlPointRect().topLeft();
    QPointF rot = QPointF(c.x(), top.y() - ROTATE_HANDLE_OFFSET);
    if (rect(rot).contains(pos)) return RotateHandle;
    return NoHandle;
}

void TransformableCurveItem::setHandleCursor(Handle h)
{
    isRotateHandle = false;
    switch (h) {
    case StartHandle: case EndHandle:
    case Ctrl1Handle: case Ctrl2Handle:
        setCursor(Qt::CrossCursor); break;
    case RotateHandle: isRotateHandle = true; break;
    default: setCursor(Qt::ArrowCursor); break;
    }
}

void TransformableCurveItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    setHandleCursor(handleAt(event->pos()));
    QGraphicsPathItem::hoverMoveEvent(event);
}

void TransformableCurveItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_currentHandle = handleAt(event->pos());
    if (m_currentHandle == NoHandle) {
        QGraphicsPathItem::mousePressEvent(event);
        return;
    }
    m_mouseDownPos = event->pos();
    m_center = path().controlPointRect().center();
    if (m_currentHandle == RotateHandle)
        m_initialRotation = rotation();
}

void TransformableCurveItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_currentHandle == NoHandle) {
        QGraphicsPathItem::mouseMoveEvent(event);
        return;
    }
    prepareGeometryChange();
    QPointF p = event->pos();
    switch (m_currentHandle) {
    case StartHandle: m_p1 = p; break;
    case EndHandle:   m_p2 = p; break;
    case Ctrl1Handle: m_c1 = p; break;
    case Ctrl2Handle: m_c2 = p; break;
    case RotateHandle: return; // 广播里处理
    default: break;
    }
    rebuildPath();
    setTransformOriginPoint(path().controlPointRect().center());
}

void TransformableCurveItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    m_currentHandle = NoHandle;
    isRotateHandling = false;
}

/* ===== 旋转 ===== */
void TransformableCurveItem::receiveSceneMousePosition(
    const QPointF &scenePos, MouseLeftClickStatus status)
{
    if (!isSelected() && !isRotateHandling) return;

    if (!isUnderMouse()) {
        Handle h = handleAt(mapFromScene(scenePos));
        setHandleCursor(h);
        if (status == MouseLeftClickStatus::PRESS
            && h == RotateHandle && !isRotateHandling) {
            m_currentHandle   = RotateHandle;
            m_mouseDownPos    = mapFromScene(scenePos);
            m_center          = path().controlPointRect().center();
            m_initialRotation = rotation();
            isRotateHandling  = true;
        }
    }
    if (status == MouseLeftClickStatus::RELEASE) {
        m_currentHandle  = NoHandle;
        isRotateHandling = false;
    }
    if (isRotateHandling) {
        QPointF cScene = mapToScene(m_center);
        QLineF start(cScene, m_mouseDownPos);
        QLineF curr(cScene, scenePos);
        qreal angleDelta = start.angleTo(curr);
        setTransformOriginPoint(m_center);
        setRotation(m_initialRotation + angleDelta);
    }
}
