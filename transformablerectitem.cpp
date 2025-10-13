#include "transformablerectitem.h"
#include <qmath.h> // for qAtan2, M_PI
#include <QGraphicsScene>

TransformableRectItem::TransformableRectItem(const QRectF &rect, QGraphicsItem *parent)
    : QGraphicsRectItem(rect, parent), m_currentHandle(NoHandle)
{
    // 设置标志位
    setFlags(ItemIsMovable | ItemIsSelectable | ItemIsFocusable);
    // 开启Hover事件，以便在鼠标悬停时改变光标
    setAcceptHoverEvents(true);
}

QRectF TransformableRectItem::boundingRect() const
{
    // 在原有包围盒的基础上扩大，以容纳控制点
    qreal extra = HANDLE_SIZE + ROTATE_HANDLE_OFFSET;
    return rect().adjusted(-extra, -extra, extra, extra);
}

void TransformableRectItem::receiveSceneMousePosition(const QPointF &scenePos, const MouseLeftClickStatus mouseLeftClickStatus)
{
    if (isSelected() || isRotateHandling){
        QPointF itemPos = this->mapFromScene(scenePos);
        if (!this->isUnderMouse()) {
            Handle handle = handleAt(itemPos);
            setHandleCursor(handle);

            if (handle == Handle::RotateHandle && mouseLeftClickStatus == MouseLeftClickStatus::PRESS && !isRotateHandling) {
                m_currentHandle = handle;
                m_mouseDownPos = mapToScene(itemPos); // 一定要在按下的时候就转换, 不然会出现抖动
                m_mouseDownRect = rect();
                m_centerPoint = rect().center();
                m_initialRotation = this->rotation();
                isRotateHandling = true;
            }
        }
        if (mouseLeftClickStatus == MouseLeftClickStatus::RELEASE) {
            m_currentHandle = NoHandle;
            isRotateHandling = false;
        }
        if (isRotateHandling) {
            // 将中心点和当前点都转换到场景坐标系下
            QPointF centerScenePos = mapToScene(m_centerPoint);
            QPointF mouseDownScenePos = m_mouseDownPos;

            // 创建从中心点到鼠标按下点和当前点的两条线
            QLineF startLine(centerScenePos, mouseDownScenePos);
            QLineF currentLine(centerScenePos, scenePos);

            // 计算两条线之间的夹角（角度增量）
            qreal angleDelta = currentLine.angleTo(startLine);

            // 设置变换原点为矩形中心
            setTransformOriginPoint(m_centerPoint);
            // 在初始角度的基础上，应用角度增量
            // Qt中角度逆时针为正，angleTo也是逆时针为正，所以用减法
            setRotation(m_initialRotation + angleDelta);
        }
    }
}

void TransformableRectItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // 首先调用基类的paint方法绘制矩形本身
    QGraphicsRectItem::paint(painter, option, widget);

    // 如果被选中，就绘制控制点
    if (isSelected()) {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(Qt::black, 1));
        painter->setBrush(Qt::white);

        // 缩放控制点 (四个角)
        painter->drawRect(rect().topLeft().x(), rect().topLeft().y(), HANDLE_SIZE, HANDLE_SIZE);
        painter->drawRect(rect().topRight().x() - HANDLE_SIZE, rect().topRight().y(), HANDLE_SIZE, HANDLE_SIZE);
        painter->drawRect(rect().bottomLeft().x(), rect().bottomLeft().y() - HANDLE_SIZE, HANDLE_SIZE, HANDLE_SIZE);
        painter->drawRect(rect().bottomRight().x() - HANDLE_SIZE, rect().bottomRight().y() - HANDLE_SIZE, HANDLE_SIZE, HANDLE_SIZE);

        // 旋转控制点 (顶部中心)
        QPointF topCenter = QPointF(rect().center().x(), rect().top());
        QPointF centerToTopVector = topCenter - rect().center();
        qreal centerToTopVectorNorm2 = QPointF::dotProduct(centerToTopVector, centerToTopVector);
        if(centerToTopVectorNorm2 > 0.0001){
            centerToTopVector /= sqrt(QPointF::dotProduct(centerToTopVector, centerToTopVector));
        }
        QPointF rotateHandlePos = topCenter + ROTATE_HANDLE_OFFSET * centerToTopVector;
        painter->drawLine(topCenter, rotateHandlePos);
        painter->drawEllipse(rotateHandlePos, HANDLE_SIZE / 2, HANDLE_SIZE / 2);
    }
}

void TransformableRectItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    // 判断鼠标当前位置是否在某个控制点上，并设置相应的鼠标样式
    Handle handle = handleAt(event->pos());
    setHandleCursor(handle);
    QGraphicsItem::hoverMoveEvent(event);
}

void TransformableRectItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // 判断是否按下了某个控制点
    m_currentHandle = handleAt(event->pos());
    if (m_currentHandle != NoHandle) {
        m_mouseDownPos = event->pos();
        m_mouseDownRect = rect();
        m_centerPoint = rect().center();

        // 如果是旋转操作，记录下当前的旋转角度
        if (m_currentHandle == RotateHandle) {
            m_initialRotation = this->rotation();
        }
    } else {
        // 如果没有按下控制点，则执行基类的默认行为（例如移动图形）
        QGraphicsRectItem::mousePressEvent(event);
    }
}

void TransformableRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_currentHandle != NoHandle) {
        QPointF pos = event->pos();
        prepareGeometryChange(); // 准备更新几何形状

        switch (m_currentHandle) {
        case TopLeft: {
            QRectF newRect = m_mouseDownRect;
            newRect.setTopLeft(pos);
            setRect(newRect.normalized());
            break;
        }
        case TopRight: {
            QRectF newRect = m_mouseDownRect;
            newRect.setTopRight(pos);
            setRect(newRect.normalized());
            break;
        }
        case BottomLeft: {
            QRectF newRect = m_mouseDownRect;
            newRect.setBottomLeft(pos);
            setRect(newRect.normalized());
            break;
        }
        case BottomRight: {
            QRectF newRect = m_mouseDownRect;
            newRect.setBottomRight(pos);
            setRect(newRect.normalized());
            break;
        }
        case RotateHandle: {
            break;
        }
        default: break;
        }
    } else {
        QGraphicsRectItem::mouseMoveEvent(event);
    }
}

void TransformableRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    m_currentHandle = NoHandle;
    QGraphicsRectItem::mouseReleaseEvent(event);
}

// 辅助函数：判断点在哪个控制点上
TransformableRectItem::Handle TransformableRectItem::handleAt(const QPointF &pos)
{
    QRectF topLeftHandle(rect().topLeft(), QSizeF(HANDLE_SIZE, HANDLE_SIZE));
    QRectF topRightHandle(rect().topRight() - QPointF(HANDLE_SIZE, 0), QSizeF(HANDLE_SIZE, HANDLE_SIZE));
    QRectF bottomLeftHandle(rect().bottomLeft() - QPointF(0, HANDLE_SIZE), QSizeF(HANDLE_SIZE, HANDLE_SIZE));
    QRectF bottomRightHandle(rect().bottomRight() - QPointF(HANDLE_SIZE, HANDLE_SIZE), QSizeF(HANDLE_SIZE, HANDLE_SIZE));

    QPointF topCenter = QPointF(rect().center().x(), rect().top());
    QPointF centerToTopVector = topCenter - rect().center();
    qreal centerToTopVectorNorm2 = QPointF::dotProduct(centerToTopVector, centerToTopVector);
    if(centerToTopVectorNorm2 > 0.0001){
        centerToTopVector /= sqrt(QPointF::dotProduct(centerToTopVector, centerToTopVector));
    }

    QPointF rotateHandlePos = topCenter + ROTATE_HANDLE_OFFSET * centerToTopVector;
    QRectF rotateHandle(rotateHandlePos - QPointF(HANDLE_SIZE/2, HANDLE_SIZE/2), QSizeF(HANDLE_SIZE, HANDLE_SIZE));

    // QPointF topCenter = QPointF(rect().center().x(), rect().top());
    // QPointF centerToTopVector = topCenter - rect().center();
    // qreal centerToTopVectorNorm2 = QPointF::dotProduct(centerToTopVector, centerToTopVector);
    // if(centerToTopVectorNorm2 > 0.0001){
    //     centerToTopVector /= sqrt(QPointF::dotProduct(centerToTopVector, centerToTopVector));
    // }
    // QPointF rotateHandlePos = topCenter + ROTATE_HANDLE_OFFSET * centerToTopVector;
    // painter->drawLine(topCenter, rotateHandlePos);
    // painter->drawEllipse(rotateHandlePos, HANDLE_SIZE / 2, HANDLE_SIZE / 2);

    // qDebug() << rotateHandle.topLeft() << ", " << rotateHandle.bottomRight()<< ", " << rotateHandle.contains(pos);

    if (topLeftHandle.contains(pos)) return TopLeft;
    if (topRightHandle.contains(pos)) return TopRight;
    if (bottomLeftHandle.contains(pos)) return BottomLeft;
    if (bottomRightHandle.contains(pos)) return BottomRight;
    if (rotateHandle.contains(pos)) return RotateHandle;

    return NoHandle;
}

// 辅助函数：根据控制点设置鼠标光标
void TransformableRectItem::setHandleCursor(Handle handle)
{
    isRotateHandle = false;
    switch (handle) {
    case TopLeft:
    case BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case TopRight:
    case BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case RotateHandle:
        isRotateHandle = true;
        break;
    default:
        setCursor(Qt::ArrowCursor);
        break;
    }
}
